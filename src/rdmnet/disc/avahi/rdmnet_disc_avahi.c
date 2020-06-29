/******************************************************************************
 * Copyright 2020 ETC Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>
#include "etcpal/common.h"
#include "etcpal/lock.h"
#include "rdmnet/core/util.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/disc/common.h"
#include "rdmnet/disc/platform_api.h"
#include "rdmnet/disc/discovered_broker.h"
#include "rdmnet/disc/registered_broker.h"
#include "rdmnet/disc/monitored_scope.h"

// Compile time check of memory configuration
#if !RDMNET_DYNAMIC_MEM
#error "RDMnet Discovery using Avahi requires RDMNET_DYNAMIC_MEM to be enabled (defined nonzero)."
#endif

/**************************** Private constants ******************************/

#define DISCOVERY_QUERY_TIMEOUT 3000

#define SERVICE_STR_PADDED_LENGTH (E133_DNSSD_SRV_TYPE_PADDED_LENGTH + E133_SCOPE_STRING_PADDED_LENGTH + 10)

/**************************** Private variables ******************************/

static AvahiSimplePoll* avahi_simple_poll;
static AvahiClient*     avahi_client;

/*********************** Private function prototypes *************************/

// Other helpers
static void             remove_resolver_from_list(RdmnetDiscoveredBrokerPlatformData* platform_data,
                                                  AvahiServiceResolver*               resolver);
static AvahiStringList* broker_info_to_txt_record(const RdmnetBrokerRegisterRef* ref);
static bool             txt_record_to_broker_info(AvahiStringList* txt, DiscoveredBroker* db);
static void ip_avahi_to_etcpal(const AvahiAddress* avahi_ip, EtcPalIpAddr* etcpal_ip, AvahiIfIndex if_index);
static bool resolved_instance_matches_us(const DiscoveredBroker* their_info, const RdmnetBrokerRegisterRef* our_info);
static bool avahi_txt_record_find(AvahiStringList* txt_list, const char* key, char** value, size_t* value_len);
static void get_full_service_type(const char* scope, char* type_str);
static bool ipv6_valid(EtcPalIpAddr* ip);

/*************************** Function definitions ****************************/

/******************************************************************************
 * DNS-SD / Avahi functions
 ******************************************************************************/

static void entry_group_callback(AvahiEntryGroup* g, AvahiEntryGroupState state, void* userdata)
{
  ETCPAL_UNUSED_ARG(userdata);

  RdmnetBrokerRegisterRef* ref = (RdmnetBrokerRegisterRef*)userdata;
  RDMNET_ASSERT(ref);

  if (g == ref->platform_data.avahi_entry_group)
  {
    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED)
    {
      if (ref->callbacks.broker_registered)
      {
        ref->callbacks.broker_registered(ref, ref->service_instance_name, ref->callbacks.context);
      }
    }
    else if (state == AVAHI_ENTRY_GROUP_COLLISION)
    {
      char* new_name = avahi_alternative_service_name(ref->service_instance_name);
      if (new_name)
      {
        rdmnet_safe_strncpy(ref->service_instance_name, new_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
        avahi_free(new_name);
      }
      rdmnet_disc_platform_register_broker(ref, NULL);
    }
    else if (state == AVAHI_ENTRY_GROUP_FAILURE)
    {
      if (ref->callbacks.broker_register_failed)
      {
        ref->callbacks.broker_register_failed(ref, avahi_client_errno(avahi_client), ref->callbacks.context);
      }
    }
  }
}

static void resolve_callback(AvahiServiceResolver*  r,
                             AvahiIfIndex           interface,
                             AvahiProtocol          protocol,
                             AvahiResolverEvent     event,
                             const char*            name,
                             const char*            type,
                             const char*            domain,
                             const char*            host_name,
                             const AvahiAddress*    address,
                             uint16_t               port,
                             AvahiStringList*       txt,
                             AvahiLookupResultFlags flags,
                             void*                  userdata)
{
  char addr_str[AVAHI_ADDRESS_STR_MAX] = {0};
  if (address)
    avahi_address_snprint(addr_str, AVAHI_ADDRESS_STR_MAX, address);
  DiscoveredBroker* db = (DiscoveredBroker*)userdata;
  RDMNET_ASSERT(db);

  RdmnetScopeMonitorRef* ref = db->monitor_ref;
  RDMNET_ASSERT(ref);

  if (event != AVAHI_RESOLVER_FAILURE && txt_record_to_broker_info(txt, db))
  {
    // Update the broker info we're building
    db->port = port;

    if (!(ref->broker_handle && resolved_instance_matches_us(db, ref->broker_handle)))
    {
      EtcPalIpAddr ip_addr;
      ip_avahi_to_etcpal(address, &ip_addr, interface);

      if ((ETCPAL_IP_IS_V4(&ip_addr) && ETCPAL_IP_V4_ADDRESS(&ip_addr) != 0) ||
          (ETCPAL_IP_IS_V6(&ip_addr) && ipv6_valid(&ip_addr)))
      {
        // Add it to the info structure
        discovered_broker_add_listen_addr(db, &ip_addr);
      }

      RdmnetBrokerDiscInfo notify_info;
      discovered_broker_fill_disc_info(db, &notify_info);
      if (db->platform_data.notified)
      {
        notify_broker_updated(ref, &notify_info);
      }
      else
      {
        notify_broker_found(ref, &notify_info);
        db->platform_data.notified = true;
      }
    }
  }
  remove_resolver_from_list(&db->platform_data, r);
  avahi_service_resolver_free(r);
}

static void browse_callback(AvahiServiceBrowser*                    b,
                            AvahiIfIndex                            interface,
                            AvahiProtocol                           protocol,
                            AvahiBrowserEvent                       event,
                            const char*                             name,
                            const char*                             type,
                            const char*                             domain,
                            AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
                            void*                                   userdata)
{
  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)userdata;
  RDMNET_ASSERT(ref);

  if (event == AVAHI_BROWSER_FAILURE)
  {
    // TODO log?
    return;
  }

  if (event == AVAHI_BROWSER_NEW || event == AVAHI_BROWSER_REMOVE)
  {
    char full_name[AVAHI_DOMAIN_NAME_MAX] = {0};
    if (0 != avahi_service_name_join(full_name, AVAHI_DOMAIN_NAME_MAX, name, type, domain))
      return;

    if (!scope_monitor_ref_is_valid(ref))
      return;

    // Filter out the service name if it matches our broker instance name.
    if (ref->broker_handle && (strcmp(full_name, ref->broker_handle->full_service_name) == 0))
      return;

    if (event == AVAHI_BROWSER_NEW)
    {
      // Track this resolve operation
      DiscoveredBroker* db = discovered_broker_find_by_name(ref->broker_list, full_name);
      if (!db)
      {
        // Allocate a new DiscoveredBroker to track info as it comes in.
        db = discovered_broker_new(ref, name, full_name);
        if (db)
          discovered_broker_insert(&ref->broker_list, db);
      }
      if (db)
      {
        // Start the next part of the resolution.
        AvahiServiceResolver* resolver = avahi_service_resolver_new(
            avahi_client, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, db);

        if (resolver)
        {
          if (db->platform_data.resolvers)
          {
            AvahiServiceResolver** new_resolver_arr = (AvahiServiceResolver**)realloc(
                db->platform_data.resolvers, (db->platform_data.num_resolvers + 1) * sizeof(AvahiServiceResolver*));
            if (new_resolver_arr)
            {
              db->platform_data.resolvers = new_resolver_arr;
              db->platform_data.resolvers[db->platform_data.num_resolvers] = resolver;
              ++db->platform_data.num_resolvers;
            }
          }
          else
          {
            db->platform_data.resolvers = (AvahiServiceResolver**)malloc(sizeof(AvahiServiceResolver*));
            db->platform_data.resolvers[0] = resolver;
            db->platform_data.num_resolvers = 1;
          }
        }
      }
    }
    else
    {
      // Service removal
      notify_broker_lost(ref, name);
      DiscoveredBroker* db = discovered_broker_find_by_name(ref->broker_list, full_name);
      if (db)
      {
        discovered_broker_remove(&ref->broker_list, db);
        discovered_broker_delete(db);
      }
    }
  }
}

static void client_callback(AvahiClient* c, AvahiClientState state, AVAHI_GCC_UNUSED void* userdata)
{
  RDMNET_ASSERT(c);
  // Called whenever the client or server state changes
  if (state == AVAHI_CLIENT_FAILURE)
  {
    RDMNET_LOG_ERR("Avahi server connection failure: %s", avahi_strerror(avahi_client_errno(c)));
    // avahi_simple_poll_quit(avahi_simple_poll);
  }
}

/******************************************************************************
 * public functions
 ******************************************************************************/

etcpal_error_t rdmnet_disc_platform_init(const RdmnetNetintConfig* netint_config)
{
  if (!(avahi_simple_poll = avahi_simple_poll_new()))
    return kEtcPalErrSys;

  int error;
  avahi_client = avahi_client_new(avahi_simple_poll_get(avahi_simple_poll), 0, client_callback, NULL, &error);
  if (!avahi_client)
  {
    RDMNET_LOG_ERR("Failed to create Avahi client instance: %s", avahi_strerror(error));
    avahi_simple_poll_free(avahi_simple_poll);
    return kEtcPalErrSys;
  }

  return kEtcPalErrOk;
}

void rdmnet_disc_platform_deinit()
{
  if (avahi_client)
  {
    avahi_client_free(avahi_client);
    avahi_client = NULL;
  }
  if (avahi_simple_poll)
  {
    avahi_simple_poll_free(avahi_simple_poll);
    avahi_simple_poll = NULL;
  }
}

void rdmnet_disc_platform_tick()
{
  if (RDMNET_DISC_LOCK())
  {
    avahi_simple_poll_iterate(avahi_simple_poll, 0);
    RDMNET_DISC_UNLOCK();
  }
}

etcpal_error_t rdmnet_disc_platform_start_monitoring(RdmnetScopeMonitorRef* handle, int* platform_specific_error)
{
  // Start the browse operation in the Avahi stack.
  char service_str[SERVICE_STR_PADDED_LENGTH];
  get_full_service_type(handle->scope, service_str);

  // Create the service browser
  handle->platform_data.avahi_browser = avahi_service_browser_new(
      avahi_client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, service_str, handle->domain, 0, browse_callback, handle);
  if (handle->platform_data.avahi_browser)
  {
    return kEtcPalErrOk;
  }
  else
  {
    *platform_specific_error = avahi_client_errno(avahi_client);
    return kEtcPalErrSys;
  }
}

void rdmnet_disc_platform_stop_monitoring(rdmnet_scope_monitor_t handle)
{
  if (handle->platform_data.avahi_browser)
    avahi_service_browser_free(handle->platform_data.avahi_browser);
}

/* If returns !0, this was an error from Avahi.  Reset the state and notify the callback. */
etcpal_error_t rdmnet_disc_platform_register_broker(RdmnetBrokerRegisterRef* ref, int* platform_specific_error)
{
  int res = 0;

  if (!ref->platform_data.avahi_entry_group)
  {
    ref->platform_data.avahi_entry_group = avahi_entry_group_new(avahi_client, entry_group_callback, ref);
    if (!ref->platform_data.avahi_entry_group)
    {
      if (platform_specific_error)
        *platform_specific_error = avahi_client_errno(avahi_client);
      return kEtcPalErrSys;
    }
  }

  AvahiEntryGroup* group = ref->platform_data.avahi_entry_group;
  if (avahi_entry_group_is_empty(group))
  {
    char service_type[E133_DNSSD_SRV_TYPE_PADDED_LENGTH];
    rdmnet_safe_strncpy(service_type, E133_DNSSD_SRV_TYPE, E133_DNSSD_SRV_TYPE_PADDED_LENGTH);

    char full_service_type[SERVICE_STR_PADDED_LENGTH];
    get_full_service_type(ref->scope, full_service_type);

    AvahiStringList* txt_list = broker_info_to_txt_record(ref);
    RDMNET_ASSERT(txt_list);

    // Add the unqualified service type
    res =
        avahi_entry_group_add_service_strlst(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, ref->service_instance_name,
                                             service_type, NULL, NULL, ref->port, txt_list);
    if (res < 0)
      return res;

    avahi_string_list_free(txt_list);

    // Add the subtype
    res = avahi_entry_group_add_service_subtype(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0,
                                                ref->service_instance_name, service_type, NULL, full_service_type);
    if (res < 0)
      return res;

    // Commit the result
    res = avahi_entry_group_commit(group);
  }

  return res;
}

void rdmnet_disc_platform_unregister_broker(rdmnet_registered_broker_t handle)
{
  if (handle->platform_data.avahi_entry_group)
  {
    avahi_entry_group_free(handle->platform_data.avahi_entry_group);
    handle->platform_data.avahi_entry_group = NULL;
  }
}

void discovered_broker_free_platform_resources(DiscoveredBroker* db)
{
  if (db->platform_data.resolvers)
  {
    for (AvahiServiceResolver** resolver = db->platform_data.resolvers;
         resolver < db->platform_data.resolvers + db->platform_data.num_resolvers; ++resolver)
    {
      avahi_service_resolver_free(*resolver);
    }
    free(db->platform_data.resolvers);
  }
}

/******************************************************************************
 * helper functions
 ******************************************************************************/

void remove_resolver_from_list(RdmnetDiscoveredBrokerPlatformData* platform_data, AvahiServiceResolver* resolver)
{
  if (platform_data->num_resolvers == 0)
    return;

  for (size_t i = 0; i < platform_data->num_resolvers; ++i)
  {
    if (platform_data->resolvers[i] == resolver)
    {
      if (i != platform_data->num_resolvers - 1)
      {
        memmove(&platform_data->resolvers[i], &platform_data->resolvers[i + 1],
                (platform_data->num_resolvers - 1 - i) * sizeof(AvahiServiceResolver*));
      }
      --platform_data->num_resolvers;
    }
  }
}

AvahiStringList* broker_info_to_txt_record(const RdmnetBrokerRegisterRef* ref)
{
  AvahiStringList* txt_list = NULL;

  char int_conversion[16];
  snprintf(int_conversion, 16, "%d", E133_DNSSD_TXTVERS);
  txt_list = avahi_string_list_add_pair(NULL, E133_TXT_VERS_KEY, int_conversion);

  if (txt_list)
    txt_list = avahi_string_list_add_pair(txt_list, E133_TXT_SCOPE_KEY, ref->scope);

  if (txt_list)
  {
    snprintf(int_conversion, 16, "%d", E133_DNSSD_E133VERS);
    txt_list = avahi_string_list_add_pair(txt_list, E133_TXT_E133VERS_KEY, int_conversion);
  }

  if (txt_list)
  {
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    etcpal_uuid_to_string(&ref->cid, cid_str);

    // Strip hyphens from the CID string to conform to E1.33 TXT record rules
    size_t stripped_len = 0;
    for (size_t i = 0; cid_str[i] != '\0'; ++i)
    {
      if (cid_str[i] != '-')
        cid_str[stripped_len++] = cid_str[i];
    }
    cid_str[stripped_len] = '\0';

    txt_list = avahi_string_list_add_pair(txt_list, E133_TXT_CID_KEY, cid_str);
  }

  if (txt_list)
  {
    char uid_str[RDM_UID_STRING_BYTES];
    rdm_uid_to_string(&ref->uid, uid_str);

    // Strip colons from the UID string to conform to E1.33 TXT record rules.
    size_t stripped_len = 0;
    for (size_t i = 0; uid_str[i] != '\0'; ++i)
    {
      if (uid_str[i] != ':')
        uid_str[stripped_len++] = uid_str[i];
    }
    uid_str[stripped_len] = '\0';

    txt_list = avahi_string_list_add_pair(txt_list, E133_TXT_UID_KEY, uid_str);
  }

  if (txt_list)
    txt_list = avahi_string_list_add_pair(txt_list, E133_TXT_MODEL_KEY, ref->model);

  if (txt_list)
    txt_list = avahi_string_list_add_pair(txt_list, E133_TXT_MANUFACTURER_KEY, ref->manufacturer);

  for (const DnsTxtRecordItemInternal* txt_item = ref->additional_txt_items;
       txt_item < ref->additional_txt_items + ref->num_additional_txt_items; ++txt_item)
  {
    if (txt_list)
      txt_list = avahi_string_list_add_pair_arbitrary(txt_list, txt_item->key, txt_item->value, txt_item->value_len);
  }

  return txt_list;
}

bool txt_record_to_broker_info(AvahiStringList* txt, DiscoveredBroker* db)
{
  // Parse the TXT record
  char*  value = NULL;
  size_t value_len = 0;

  // If the TXTVers key is not set to E133_DNSSD_TXT_VERS, we cannot parse this TXT record.
  if (avahi_txt_record_find(txt, E133_TXT_VERS_KEY, &value, &value_len) && value && value_len != 0 && value_len < 16)
  {
    char txt_vers_buf[16];
    memcpy(txt_vers_buf, value, value_len);
    txt_vers_buf[value_len] = '\0';
    if (atoi(txt_vers_buf) != E133_DNSSD_TXTVERS)
      return false;
  }
  else
  {
    return false;
  }

  for (AvahiStringList* item = txt; item; item = avahi_string_list_get_next(item))
  {
    char* key;
    avahi_string_list_get_pair(item, &key, &value, &value_len);
    if ((strcmp(key, E133_TXT_SCOPE_KEY) == 0) && value && value_len)
    {
      rdmnet_safe_strncpy(
          db->scope, value,
          (value_len + 1 > E133_SCOPE_STRING_PADDED_LENGTH ? E133_SCOPE_STRING_PADDED_LENGTH : value_len + 1));
    }
    else if ((strcmp(key, E133_TXT_CID_KEY) == 0) && value && value_len && value_len < ETCPAL_UUID_STRING_BYTES)
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      rdmnet_safe_strncpy(cid_str, value, ETCPAL_UUID_STRING_BYTES);
      etcpal_string_to_uuid(value, &db->cid);
    }
    else if ((strcmp(key, E133_TXT_UID_KEY) == 0) && value && value_len && value_len < RDM_UID_STRING_BYTES)
    {
      char uid_str[RDM_UID_STRING_BYTES];
      rdmnet_safe_strncpy(uid_str, value, RDM_UID_STRING_BYTES);
      rdm_string_to_uid(uid_str, &db->uid);
    }
    else if ((strcmp(key, E133_TXT_MODEL_KEY) == 0) && value && value_len)
    {
      rdmnet_safe_strncpy(
          db->model, value,
          (value_len + 1 > E133_MODEL_STRING_PADDED_LENGTH ? E133_MODEL_STRING_PADDED_LENGTH : value_len + 1));
    }
    else if ((strcmp(key, E133_TXT_MANUFACTURER_KEY) == 0) && value && value_len)
    {
      rdmnet_safe_strncpy(
          db->manufacturer, value,
          (value_len + 1 > E133_MANUFACTURER_STRING_PADDED_LENGTH ? E133_MANUFACTURER_STRING_PADDED_LENGTH
                                                                  : value_len + 1));
    }
    else if (strcmp(key, E133_TXT_VERS_KEY) == 0)
    {
      // Do nothing; we have already handled this key above.
    }
    else if ((strcmp(key, E133_TXT_E133VERS_KEY) == 0) && value && value_len > 0 && value_len < 16)
    {
      char e133_vers_buf[16];
      memcpy(value, e133_vers_buf, value_len);
      e133_vers_buf[value_len] = '\0';
      db->e133_version = atoi(e133_vers_buf);
    }
    else
    {
      // Add a non-standard TXT record item
      discovered_broker_add_txt_record_item(db, key, (uint8_t*)value, value_len);
    }

    avahi_free(key);
    avahi_free(value);
  }
  return true;
}

void ip_avahi_to_etcpal(const AvahiAddress* avahi_ip, EtcPalIpAddr* etcpal_ip, AvahiIfIndex if_index)
{
  if (avahi_ip->proto == AVAHI_PROTO_INET)
  {
    ETCPAL_IP_SET_V4_ADDRESS(etcpal_ip, ntohl(avahi_ip->data.ipv4.address));
  }
  else if (avahi_ip->proto == AVAHI_PROTO_INET6)
  {
    ETCPAL_IP_SET_V6_ADDRESS(etcpal_ip, avahi_ip->data.ipv6.address);
    if (etcpal_ip_is_link_local(etcpal_ip) && if_index != AVAHI_IF_UNSPEC)
      etcpal_ip->addr.v6.scope_id = if_index;
  }
  else
  {
    ETCPAL_IP_SET_INVALID(etcpal_ip);
  }
}

// Determine if a resolved service instance matches our locally-registered broker, per the method
// described in ANSI E1.33 Section 9.1.4.
bool resolved_instance_matches_us(const DiscoveredBroker* their_info, const RdmnetBrokerRegisterRef* our_info)
{
  // TODO: host name must also be included in this comparison.
  return ((their_info->port == our_info->port) && (0 == strcmp(their_info->scope, our_info->scope)) &&
          (0 == ETCPAL_UUID_CMP(&their_info->cid, &our_info->cid)));
}

bool avahi_txt_record_find(AvahiStringList* txt_list, const char* key, char** value, size_t* value_len)
{
  AvahiStringList* found = avahi_string_list_find(txt_list, key);
  if (found)
  {
    char* tmp_key;
    if (0 == avahi_string_list_get_pair(found, &tmp_key, value, value_len))
    {
      // We don't need the key, free immediately. The value will be freed by the user
      avahi_free(tmp_key);
      return true;
    }
  }
  return false;
}

void get_full_service_type(const char* scope, char* reg_str)
{
  sprintf(reg_str, "_%s._sub.%s", scope, E133_DNSSD_SRV_TYPE);
}

// Test to make sure an address is not a loopback or wildcard address.
bool ipv6_valid(EtcPalIpAddr* ip)
{
  return (!etcpal_ip_is_loopback(ip) && !etcpal_ip_is_wildcard(ip));
}
