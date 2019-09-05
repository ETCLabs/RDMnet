/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2019 ETC Inc.
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

#include "rdmnet/discovery/avahi.h"

#include <assert.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <avahi-common/alternative.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/simple-watch.h>

#include "etcpal/lock.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/opts.h"

// Compile time check of memory configuration
#if !RDMNET_DYNAMIC_MEM
#error "RDMnet Discovery using Avahi requires RDMNET_DYNAMIC_MEM to be enabled (defined nonzero)."
#endif

/**************************** Private constants ******************************/

#define DISCOVERY_QUERY_TIMEOUT 3000

/**************************** Private variables ******************************/

typedef struct DiscoveryState
{
  etcpal_mutex_t lock;

  RdmnetScopeMonitorRef* scope_ref_list;
  RdmnetBrokerRegisterConfig registered_broker;

  RdmnetBrokerRegisterRef broker_ref;

  AvahiSimplePoll* avahi_simple_poll;
  AvahiClient* avahi_client;
} DiscoveryState;

static DiscoveryState disc_state;

/*********************** Private function prototypes *************************/

// Allocation and deallocation
static RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config);
static void scope_monitor_delete(RdmnetScopeMonitorRef* ref);
static DiscoveredBroker* discovered_broker_new(RdmnetScopeMonitorRef* ref, const char* service_name,
                                               const char* full_service_name);
static void discovered_broker_delete(DiscoveredBroker* db);
// static RdmnetBrokerRegisterRef* registered_broker_new();
// static void registered_broker_delete(RdmnetBrokerRegisterRef* rb);

// Add and remove from appropriate lists
static void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new);
static DiscoveredBroker* discovered_broker_lookup_by_name(DiscoveredBroker* list_head, const char* full_name);
static void discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db);
static void scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref);
static void scope_monitor_remove(const RdmnetScopeMonitorRef* ref);

// Notify the appropriate callbacks
static void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info);
static void notify_broker_lost(rdmnet_scope_monitor_t handle, const char* service_name);
static void notify_scope_monitor_error(rdmnet_scope_monitor_t handle, int platform_error);

// Other helpers
static void stop_monitoring_all_internal();
static AvahiStringList* build_txt_record(const RdmnetBrokerDiscInfo* info);
static int send_registration(const RdmnetBrokerDiscInfo* info, AvahiEntryGroup** entry_group, void* context);
static void ip_avahi_to_etcpal(const AvahiAddress* avahi_ip, EtcPalIpAddr* etcpal_ip);
static bool resolved_instance_matches_us(const RdmnetBrokerDiscInfo* their_info, const RdmnetBrokerDiscInfo* our_info);
static bool avahi_txt_record_find(AvahiStringList* txt_list, const char* key, char** value, size_t* value_len);
static void get_full_service_type(const char* scope, char* type_str);
static bool broker_info_is_valid(const RdmnetBrokerDiscInfo* info);
static bool ipv6_valid(EtcPalIpAddr* ip);

/*************************** Function definitions ****************************/

/******************************************************************************
 * DNS-SD / Bonjour functions
 ******************************************************************************/

static void entry_group_callback(AvahiEntryGroup* g, AvahiEntryGroupState state, void* userdata)
{
  (void)userdata;

  rdmnet_registered_broker_t broker_handle = &disc_state.broker_ref;
  if (!broker_handle)
    return;

  if (g == disc_state.broker_ref.avahi_entry_group)
  {
    if (state == AVAHI_ENTRY_GROUP_ESTABLISHED)
    {
      if (broker_handle && broker_handle->config.callbacks.broker_registered)
      {
        broker_handle->config.callbacks.broker_registered(broker_handle, broker_handle->config.my_info.service_name,
                                                          broker_handle->config.callback_context);
      }
    }
    else if (state == AVAHI_ENTRY_GROUP_COLLISION)
    {
      char* new_name = avahi_alternative_service_name(broker_handle->config.my_info.service_name);
      if (new_name)
      {
        rdmnet_safe_strncpy(broker_handle->config.my_info.service_name, new_name,
                            E133_SERVICE_NAME_STRING_PADDED_LENGTH);
        avahi_free(new_name);
      }
      send_registration(&broker_handle->config.my_info, &broker_handle->avahi_entry_group, broker_handle);
    }
    else if (state == AVAHI_ENTRY_GROUP_FAILURE)
    {
      if (broker_handle->config.callbacks.broker_register_error)
      {
        broker_handle->config.callbacks.broker_register_error(
            broker_handle, avahi_client_errno(disc_state.avahi_client), broker_handle->config.callback_context);
      }
    }
  }
}

static void resolve_callback(AvahiServiceResolver* r, AvahiIfIndex interface, AvahiProtocol protocol,
                             AvahiResolverEvent event, const char* name, const char* type, const char* domain,
                             const char* host_name, const AvahiAddress* address, uint16_t port, AvahiStringList* txt,
                             AvahiLookupResultFlags flags, void* userdata)
{
  char addr_str[AVAHI_ADDRESS_STR_MAX] = {0};
  if (address)
    avahi_address_snprint(addr_str, AVAHI_ADDRESS_STR_MAX, address);
  DiscoveredBroker* db = (DiscoveredBroker*)userdata;
  assert(db);

  RdmnetScopeMonitorRef* ref = db->monitor_ref;
  assert(ref);

  if (event == AVAHI_RESOLVER_FAILURE)
  {
    notify_scope_monitor_error(db->monitor_ref, avahi_client_errno(disc_state.avahi_client));
    if (etcpal_mutex_take(&disc_state.lock))
    {
      if (--db->num_outstanding_resolves <= 0 && db->num_successful_resolves == 0)
      {
        // Remove the DiscoveredBroker from the list
        discovered_broker_remove(&ref->broker_list, db);
        discovered_broker_delete(db);
      }
      etcpal_mutex_give(&disc_state.lock);
    }
  }
  else
  {
    bool notify = false;
    RdmnetBrokerDiscInfo notify_info;

    if (etcpal_mutex_take(&disc_state.lock))
    {
      // Update the broker info we're building
      db->info.port = port;

      // Parse the TXT record
      char* value;
      size_t value_len;

      if (avahi_txt_record_find(txt, "ConfScope", &value, &value_len))
      {
        rdmnet_safe_strncpy(
            db->info.scope, value,
            (value_len + 1 > E133_SCOPE_STRING_PADDED_LENGTH ? E133_SCOPE_STRING_PADDED_LENGTH : value_len + 1));
        avahi_free(value);
      }

      if (avahi_txt_record_find(txt, "CID", &value, &value_len))
      {
        etcpal_string_to_uuid(&db->info.cid, value, value_len);
        avahi_free(value);
      }

      if (avahi_txt_record_find(txt, "Model", &value, &value_len))
      {
        rdmnet_safe_strncpy(
            db->info.model, value,
            (value_len + 1 > E133_MODEL_STRING_PADDED_LENGTH ? E133_MODEL_STRING_PADDED_LENGTH : value_len + 1));
        avahi_free(value);
      }

      if (avahi_txt_record_find(txt, "Manuf", &value, &value_len))
      {
        rdmnet_safe_strncpy(
            db->info.manufacturer, value,
            (value_len + 1 > E133_MANUFACTURER_STRING_PADDED_LENGTH ? E133_MANUFACTURER_STRING_PADDED_LENGTH
                                                                    : value_len + 1));
        avahi_free(value);
      }

      if (ref->broker_handle && resolved_instance_matches_us(&db->info, &ref->broker_handle->config.my_info))
      {
        if (--db->num_outstanding_resolves <= 0 && db->num_successful_resolves == 0)
        {
          discovered_broker_remove(&ref->broker_list, db);
          discovered_broker_delete(db);
        }
      }
      else
      {
        EtcPalIpAddr ip_addr;
        ip_avahi_to_etcpal(address, &ip_addr);

        if ((ETCPAL_IP_IS_V4(&ip_addr) && ETCPAL_IP_V4_ADDRESS(&ip_addr) != 0) ||
            (ETCPAL_IP_IS_V6(&ip_addr) && ipv6_valid(&ip_addr)))
        {
          // Add it to the info structure
          BrokerListenAddr* new_addr = (BrokerListenAddr*)malloc(sizeof(BrokerListenAddr));
          new_addr->addr = ip_addr;
          new_addr->next = NULL;

          if (!db->info.listen_addr_list)
          {
            db->info.listen_addr_list = new_addr;
          }
          else
          {
            BrokerListenAddr* cur_addr = db->info.listen_addr_list;
            while (cur_addr->next)
              cur_addr = cur_addr->next;
            cur_addr->next = new_addr;
          }
        }

        notify_info = db->info;
        notify = true;
        --db->num_outstanding_resolves;
        ++db->num_successful_resolves;
      }
      etcpal_mutex_give(&disc_state.lock);
    }

    if (notify)
      notify_broker_found(ref, &notify_info);
  }
  avahi_service_resolver_free(r);
}

static void browse_callback(AvahiServiceBrowser* b, AvahiIfIndex interface, AvahiProtocol protocol,
                            AvahiBrowserEvent event, const char* name, const char* type, const char* domain,
                            AVAHI_GCC_UNUSED AvahiLookupResultFlags flags, void* userdata)
{
  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)userdata;
  assert(ref);

  if (event == AVAHI_BROWSER_FAILURE)
  {
    notify_scope_monitor_error(ref, avahi_client_errno(disc_state.avahi_client));
  }
  else if (event == AVAHI_BROWSER_NEW || event == AVAHI_BROWSER_REMOVE)
  {
    char full_name[AVAHI_DOMAIN_NAME_MAX] = {0};
    if (0 != avahi_service_name_join(full_name, AVAHI_DOMAIN_NAME_MAX, name, type, domain))
      return;

    if (event == AVAHI_BROWSER_NEW)
    {
      // We have to take the lock before the DNSServiceResolve call, because we need to add the ref to
      // our map before it responds.
      int resolve_err = 0;
      if (etcpal_mutex_take(&disc_state.lock))
      {
        // Track this resolve operation
        DiscoveredBroker* db = discovered_broker_lookup_by_name(ref->broker_list, full_name);
        if (!db)
        {
          // Allocate a new DiscoveredBroker to track info as it comes in.
          db = discovered_broker_new(ref, name, full_name);
          if (db)
          {
            discovered_broker_insert(&ref->broker_list, db);
          }
        }
        if (db)
        {
          // Start the next part of the resolution.
          if (avahi_service_resolver_new(disc_state.avahi_client, interface, protocol, name, type, domain,
                                         AVAHI_PROTO_UNSPEC, 0, resolve_callback, db))
          {
            ++db->num_outstanding_resolves;
          }
          else
          {
            if (db->num_outstanding_resolves <= 0 && db->num_successful_resolves == 0)
            {
              discovered_broker_remove(&ref->broker_list, db);
              discovered_broker_delete(db);
            }
            resolve_err = avahi_client_errno(disc_state.avahi_client);
          }
        }

        etcpal_mutex_give(&disc_state.lock);
      }

      if (resolve_err != 0)
        notify_scope_monitor_error(ref, resolve_err);
    }
    else
    {
      /*Service removal*/
      if (etcpal_mutex_take(&disc_state.lock))
      {
        DiscoveredBroker* db = discovered_broker_lookup_by_name(ref->broker_list, full_name);
        if (db)
        {
          discovered_broker_remove(&ref->broker_list, db);
          discovered_broker_delete(db);
        }
        etcpal_mutex_give(&disc_state.lock);
      }
      notify_broker_lost(ref, name);
    }
  }
}

static void client_callback(AvahiClient* c, AvahiClientState state, AVAHI_GCC_UNUSED void* userdata)
{
  assert(c);
  /* Called whenever the client or server state changes */
  if (state == AVAHI_CLIENT_FAILURE)
  {
    etcpal_log(rdmnet_log_params, ETCPAL_LOG_ERR, RDMNET_LOG_MSG("Avahi server connection failure: %s"),
             avahi_strerror(avahi_client_errno(c)));
    // avahi_simple_poll_quit(disc_state.avahi_simple_poll);
  }
}

/******************************************************************************
 * public functions
 ******************************************************************************/

etcpal_error_t rdmnetdisc_init()
{
  if (!etcpal_mutex_create(&disc_state.lock))
    return kEtcPalErrSys;

  if (!(disc_state.avahi_simple_poll = avahi_simple_poll_new()))
  {
    etcpal_mutex_destroy(&disc_state.lock);
    return kEtcPalErrSys;
  }

  int error;
  disc_state.avahi_client =
      avahi_client_new(avahi_simple_poll_get(disc_state.avahi_simple_poll), 0, client_callback, NULL, &error);
  if (!disc_state.avahi_client)
  {
    etcpal_log(rdmnet_log_params, ETCPAL_LOG_ERR, RDMNET_LOG_MSG("Failed to create Avahi client instance: %s"),
             avahi_strerror(error));
    avahi_simple_poll_free(disc_state.avahi_simple_poll);
    etcpal_mutex_destroy(&disc_state.lock);
    return kEtcPalErrSys;
  }

  disc_state.broker_ref.state = kBrokerStateNotRegistered;
  return kEtcPalErrOk;
}

void rdmnetdisc_deinit()
{
  stop_monitoring_all_internal();

  if (disc_state.avahi_client)
  {
    avahi_client_free(disc_state.avahi_client);
    disc_state.avahi_client = NULL;
  }
  if (disc_state.avahi_simple_poll)
  {
    avahi_simple_poll_free(disc_state.avahi_simple_poll);
    disc_state.avahi_simple_poll = NULL;
  }

  etcpal_mutex_destroy(&disc_state.lock);
}

void rdmnetdisc_fill_default_broker_info(RdmnetBrokerDiscInfo* broker_info)
{
  broker_info->cid = kEtcPalNullUuid;
  rdmnet_safe_strncpy(broker_info->service_name, "RDMnet Broker", E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  broker_info->port = 0;
  broker_info->listen_addr_list = NULL;
  rdmnet_safe_strncpy(broker_info->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
  memset(broker_info->model, 0, E133_MODEL_STRING_PADDED_LENGTH);
  memset(broker_info->manufacturer, 0, E133_MANUFACTURER_STRING_PADDED_LENGTH);
}

etcpal_error_t rdmnetdisc_start_monitoring(const RdmnetScopeMonitorConfig* config, rdmnet_scope_monitor_t* handle,
                                         int* platform_specific_error)
{
  if (!config || !handle || !platform_specific_error)
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  RdmnetScopeMonitorRef* new_monitor = scope_monitor_new(config);
  if (!new_monitor)
    return kEtcPalErrNoMem;

  // Start the browse operation in the Bonjour stack.
  char service_str[SERVICE_STR_PADDED_LENGTH];
  get_full_service_type(config->scope, service_str);

  // We have to take the lock before the DNSServiceBrowse call, because we need to add the ref to
  // our map before it responds.
  etcpal_error_t res = kEtcPalErrOk;
  if (etcpal_mutex_take(&disc_state.lock))
  {
    /* Create the service browser */
    new_monitor->avahi_browser =
        avahi_service_browser_new(disc_state.avahi_client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, service_str,
                                  config->domain, 0, browse_callback, new_monitor);
    if (new_monitor->avahi_browser)
    {
      scope_monitor_insert(new_monitor);
    }
    else
    {
      *platform_specific_error = avahi_client_errno(disc_state.avahi_client);
      scope_monitor_delete(new_monitor);
      res = kEtcPalErrSys;
    }
    etcpal_mutex_give(&disc_state.lock);
  }

  if (res == kEtcPalErrOk)
    *handle = new_monitor;

  return res;
}

etcpal_error_t rdmnetdisc_change_monitored_scope(rdmnet_scope_monitor_t handle,
                                               const RdmnetScopeMonitorConfig* new_config)
{
  // TODO reevaluate if this is necessary.
  (void)handle;
  (void)new_config;
  return kEtcPalErrNotImpl;
}

void rdmnetdisc_stop_monitoring(rdmnet_scope_monitor_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (etcpal_mutex_take(&disc_state.lock))
  {
    scope_monitor_remove(handle);
    scope_monitor_delete(handle);
    etcpal_mutex_give(&disc_state.lock);
  }
}

void rdmnetdisc_stop_monitoring_all()
{
  if (!rdmnet_core_initialized())
    return;

  stop_monitoring_all_internal();
}

void stop_monitoring_all_internal()
{
  if (etcpal_mutex_take(&disc_state.lock))
  {
    if (disc_state.scope_ref_list)
    {
      RdmnetScopeMonitorRef* ref = disc_state.scope_ref_list;
      RdmnetScopeMonitorRef* next_ref;
      while (ref)
      {
        next_ref = ref->next;
        scope_monitor_delete(ref);
        ref = next_ref;
      }
      disc_state.scope_ref_list = NULL;
    }
    etcpal_mutex_give(&disc_state.lock);
  }
}

etcpal_error_t rdmnetdisc_register_broker(const RdmnetBrokerRegisterConfig* config, rdmnet_registered_broker_t* handle)
{
  if (!config || !handle || disc_state.broker_ref.state != kBrokerStateNotRegistered ||
      !broker_info_is_valid(&config->my_info))
  {
    return kEtcPalErrInvalid;
  }
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  RdmnetBrokerRegisterRef* broker_ref = &disc_state.broker_ref;

  // Begin monitoring the broker's scope for other brokers
  RdmnetScopeMonitorConfig monitor_config;
  rdmnet_safe_strncpy(monitor_config.scope, config->my_info.scope, E133_SCOPE_STRING_PADDED_LENGTH);
  rdmnet_safe_strncpy(monitor_config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);

  int mon_error;
  rdmnet_scope_monitor_t monitor_handle;
  if (rdmnetdisc_start_monitoring(&monitor_config, &monitor_handle, &mon_error) == kEtcPalErrOk)
  {
    broker_ref->scope_monitor_handle = monitor_handle;
    monitor_handle->broker_handle = broker_ref;

    broker_ref->config = *config;
    broker_ref->state = kBrokerStateQuerying;
    broker_ref->query_timeout_expired = false;
    etcpal_timer_start(&broker_ref->query_timer, DISCOVERY_QUERY_TIMEOUT);
  }
  else if (broker_ref->config.callbacks.scope_monitor_error)
  {
    broker_ref->config.callbacks.scope_monitor_error(broker_ref, monitor_config.scope, mon_error,
                                                     broker_ref->config.callback_context);
  }

  *handle = broker_ref;
  return kEtcPalErrOk;
}

void rdmnetdisc_unregister_broker(rdmnet_registered_broker_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (disc_state.broker_ref.state != kBrokerStateNotRegistered)
  {
    if (disc_state.broker_ref.avahi_entry_group)
    {
      avahi_entry_group_free(disc_state.broker_ref.avahi_entry_group);
      disc_state.broker_ref.avahi_entry_group = NULL;
    }

    /* Since the broker only cares about scopes while it is running, shut down any outstanding
     * queries for that scope.*/
    rdmnetdisc_stop_monitoring(handle->scope_monitor_handle);

    /*Reset the state*/
    disc_state.broker_ref.state = kBrokerStateNotRegistered;
  }
}

AvahiStringList* build_txt_record(const RdmnetBrokerDiscInfo* info)
{
  AvahiStringList* txt_list = NULL;

  char int_conversion[16];
  snprintf(int_conversion, 16, "%d", E133_DNSSD_TXTVERS);
  txt_list = avahi_string_list_add_pair(NULL, "TxtVers", int_conversion);

  if (txt_list)
    txt_list = avahi_string_list_add_pair(txt_list, "ConfScope", info->scope);

  if (txt_list)
  {
    snprintf(int_conversion, 16, "%d", E133_DNSSD_E133VERS);
    txt_list = avahi_string_list_add_pair(txt_list, "E133Vers", int_conversion);
  }

  if (txt_list)
  {
    /*The CID can't have hyphens, so we'll strip them.*/
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    etcpal_uuid_to_string(cid_str, &info->cid);
    char* src = cid_str;
    for (char* dst = src; *dst != 0; ++src, ++dst)
    {
      if (*src == '-')
        ++src;

      *dst = *src;
    }
    txt_list = avahi_string_list_add_pair(txt_list, "CID", cid_str);
  }

  if (txt_list)
    txt_list = avahi_string_list_add_pair(txt_list, "Model", info->model);

  if (txt_list)
    txt_list = avahi_string_list_add_pair(txt_list, "Manuf", info->manufacturer);

  return txt_list;
}

/* If returns !0, this was an error from Avahi.  Reset the state and notify the callback.*/
int send_registration(const RdmnetBrokerDiscInfo* info, AvahiEntryGroup** entry_group, void* context)
{
  int res = 0;

  if (!(*entry_group))
  {
    *entry_group = avahi_entry_group_new(disc_state.avahi_client, entry_group_callback, context);
    if (!(*entry_group))
    {
      return avahi_client_errno(disc_state.avahi_client);
    }
  }

  AvahiEntryGroup* group = *entry_group;
  if (avahi_entry_group_is_empty(group))
  {
    char service_type[E133_DNSSD_SRV_TYPE_PADDED_LENGTH];
    rdmnet_safe_strncpy(service_type, E133_DNSSD_SRV_TYPE, E133_DNSSD_SRV_TYPE_PADDED_LENGTH);

    char full_service_type[SERVICE_STR_PADDED_LENGTH];
    get_full_service_type(info->scope, full_service_type);

    AvahiStringList* txt_list = build_txt_record(info);
    assert(txt_list);

    // Add the unqualified service type
    res = avahi_entry_group_add_service_strlst(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, info->service_name,
                                               service_type, NULL, NULL, info->port, txt_list);
    if (res < 0)
      return res;

    avahi_string_list_free(txt_list);

    // Add the subtype
    res = avahi_entry_group_add_service_subtype(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0, info->service_name,
                                                service_type, NULL, full_service_type);
    if (res < 0)
      return res;

    // Commit the result
    res = avahi_entry_group_commit(group);
  }

  return res;
}

void rdmnetdisc_tick()
{
  if (!rdmnet_core_initialized())
    return;

  // TODO: For now, we are only allowing one registered broker. This should be able to be expanded
  // to allow up to n brokers, however.
  RdmnetBrokerRegisterRef* broker_ref = &disc_state.broker_ref;
  switch (broker_ref->state)
  {
    case kBrokerStateQuerying:
    {
      if (!broker_ref->query_timeout_expired && etcpal_timer_is_expired(&broker_ref->query_timer))
        broker_ref->query_timeout_expired = true;

      if (broker_ref->query_timeout_expired && !broker_ref->scope_monitor_handle->broker_list)
      {
        // If the initial query timeout is expired and we haven't discovered any conflicting brokers,
        // we can proceed.
        broker_ref->state = kBrokerStateRegisterStarted;

        int reg_result = send_registration(&broker_ref->config.my_info, &broker_ref->avahi_entry_group, broker_ref);
        if (reg_result != 0)
        {
          broker_ref->state = kBrokerStateNotRegistered;
          if (broker_ref->config.callbacks.broker_register_error != NULL)
          {
            broker_ref->config.callbacks.broker_register_error(broker_ref, reg_result,
                                                               broker_ref->config.callback_context);
          }
        }
      }
    }
    break;

    case kBrokerStateNotRegistered:
    case kBrokerStateRegisterStarted:
    case kBrokerStateRegistered:
      // Nothing to do here right now.
      break;
  }

  avahi_simple_poll_iterate(disc_state.avahi_simple_poll, 0);
}

void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info)
{
  if (handle->broker_handle)
  {
    if (handle->broker_handle->config.callbacks.broker_found)
    {
      handle->broker_handle->config.callbacks.broker_found(handle->broker_handle, broker_info,
                                                           handle->broker_handle->config.callback_context);
    }
  }
  else if (handle->config.callbacks.broker_found)
  {
    handle->config.callbacks.broker_found(handle, broker_info, handle->config.callback_context);
  }
}

void notify_broker_lost(rdmnet_scope_monitor_t handle, const char* service_name)
{
  if (handle->broker_handle)
  {
    if (handle->broker_handle->config.callbacks.broker_lost)
    {
      handle->broker_handle->config.callbacks.broker_lost(handle->broker_handle, handle->config.scope, service_name,
                                                          handle->broker_handle->config.callback_context);
    }
  }
  else if (handle->config.callbacks.broker_lost)
  {
    handle->config.callbacks.broker_lost(handle, handle->config.scope, service_name, handle->config.callback_context);
  }
}

void notify_scope_monitor_error(rdmnet_scope_monitor_t handle, int platform_error)
{
  if (handle->broker_handle)
  {
    if (handle->broker_handle->config.callbacks.scope_monitor_error)
    {
      handle->broker_handle->config.callbacks.scope_monitor_error(
          handle->broker_handle, handle->config.scope, platform_error, handle->broker_handle->config.callback_context);
    }
  }
  else if (handle->config.callbacks.scope_monitor_error)
  {
    handle->config.callbacks.scope_monitor_error(handle, handle->config.scope, platform_error,
                                                 handle->config.callback_context);
  }
}

/******************************************************************************
 * helper functions
 ******************************************************************************/

void ip_avahi_to_etcpal(const AvahiAddress* avahi_ip, EtcPalIpAddr* etcpal_ip)
{
  if (avahi_ip->proto == AVAHI_PROTO_INET)
  {
    ETCPAL_IP_SET_V4_ADDRESS(etcpal_ip, ntohl(avahi_ip->data.ipv4.address));
  }
  else if (avahi_ip->proto == AVAHI_PROTO_INET6)
  {
    ETCPAL_IP_SET_V6_ADDRESS(etcpal_ip, avahi_ip->data.ipv6.address);
  }
  else
  {
    ETCPAL_IP_SET_INVALID(etcpal_ip);
  }
}

// Determine if a resolved service instance matches our locally-registered broker, per the method
// described in ANSI E1.33 Section 9.1.4.
bool resolved_instance_matches_us(const RdmnetBrokerDiscInfo* their_info, const RdmnetBrokerDiscInfo* our_info)
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

bool broker_info_is_valid(const RdmnetBrokerDiscInfo* info)
{
  // Make sure none of the broker info's fields are empty
  return !(ETCPAL_UUID_IS_NULL(&info->cid) || strlen(info->service_name) == 0 || strlen(info->scope) == 0 ||
           strlen(info->model) == 0 || strlen(info->manufacturer) == 0);
}

/* 0000:0000:0000:0000:0000:0000:0000:0001 is a loopback address
 * 0000:0000:0000:0000:0000:0000:0000:0000 is not valid */
bool ipv6_valid(EtcPalIpAddr* ip)
{
  return (!etcpal_ip_is_loopback(ip) && !etcpal_ip_is_wildcard(ip));
}

RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config)
{
  RdmnetScopeMonitorRef* new_monitor = (RdmnetScopeMonitorRef*)malloc(sizeof(RdmnetScopeMonitorRef));
  if (new_monitor)
  {
    new_monitor->config = *config;
    new_monitor->avahi_browser = NULL;
    new_monitor->broker_handle = NULL;
    new_monitor->broker_list = NULL;
    new_monitor->next = NULL;
  }
  return new_monitor;
}

void scope_monitor_delete(RdmnetScopeMonitorRef* ref)
{
  DiscoveredBroker* db = ref->broker_list;
  DiscoveredBroker* next_db;
  while (db)
  {
    next_db = db->next;
    discovered_broker_delete(db);
    db = next_db;
  }
  if (ref->avahi_browser)
    avahi_service_browser_free(ref->avahi_browser);
  free(ref);
}

DiscoveredBroker* discovered_broker_new(RdmnetScopeMonitorRef* ref, const char* service_name,
                                        const char* full_service_name)
{
  DiscoveredBroker* new_db = (DiscoveredBroker*)malloc(sizeof(DiscoveredBroker));
  if (new_db)
  {
    rdmnetdisc_fill_default_broker_info(&new_db->info);
    rdmnet_safe_strncpy(new_db->info.service_name, service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_db->full_service_name, full_service_name, AVAHI_DOMAIN_NAME_MAX);
    new_db->monitor_ref = ref;
    new_db->num_outstanding_resolves = 0;
    new_db->num_successful_resolves = 0;
    new_db->next = NULL;
  }
  return new_db;
}

void discovered_broker_delete(DiscoveredBroker* db)
{
  BrokerListenAddr* listen_addr = db->info.listen_addr_list;
  while (listen_addr)
  {
    BrokerListenAddr* next_listen_addr = listen_addr->next;
    free(listen_addr);
    listen_addr = next_listen_addr;
  }
  free(db);
}

// RdmnetBrokerRegisterRef* registered_broker_new(const RdmnetBrokerRegisterConfig* config)
//{
//  RdmnetBrokerRegisterRef* new_rb = (RdmnetBrokerRegisterRef*)malloc(sizeof(RdmnetBrokerRegisterRef));
//  if (new_rb)
//  {
//    new_rb->config = *config;
//    new_rb->scope_monitor_handle = NULL;
//    new_rb->state = kBrokerStateNotRegistered;
//    new_rb->full_service_name[0] = '\0';
//    new_rb->avahi_entry_group = NULL;
//  }
//  return new_rb;
//}
//
// void registered_broker_delete(RdmnetBrokerRegisterRef* rb)
//{
//  free(rb);
//}

/* Adds broker discovery information into brokers.
 * Assumes a lock is already taken.*/
void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new)
{
  if (*list_head_ptr)
  {
    DiscoveredBroker* cur = *list_head_ptr;
    for (; cur->next; cur = cur->next)
      ;
    cur->next = new;
  }
  else
  {
    *list_head_ptr = new;
  }
}

/* Searches for a DiscoveredBroker instance by full name in a list.
 * Returns the found instance or NULL if no match was found.
 * Assumes a lock is already taken.
 */
DiscoveredBroker* discovered_broker_lookup_by_name(DiscoveredBroker* list_head, const char* full_name)
{
  for (DiscoveredBroker* current = list_head; current; current = current->next)
  {
    if (strcmp(current->full_service_name, full_name) == 0)
    {
      return current;
    }
  }
  return NULL;
}

/* Removes a DiscoveredBroker instance from a list.
 * Assumes a lock is already taken.*/
void discovered_broker_remove(DiscoveredBroker** list_head_ptr, const DiscoveredBroker* db)
{
  if (!(*list_head_ptr))
    return;

  if (*list_head_ptr == db)
  {
    // Remove from the head of the list
    *list_head_ptr = (*list_head_ptr)->next;
  }
  else
  {
    // Find in the list and remove.
    DiscoveredBroker* prev_db = *list_head_ptr;
    for (; prev_db->next; prev_db = prev_db->next)
    {
      if (prev_db->next == db)
      {
        prev_db->next = prev_db->next->next;
        break;
      }
    }
  }
}

/* Adds new scope info to the scope_ref_list.
 * Assumes a lock is already taken.*/
void scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref)
{
  if (scope_ref)
  {
    scope_ref->next = NULL;

    if (!disc_state.scope_ref_list)
    {
      // Make the new scope the head of the list.
      disc_state.scope_ref_list = scope_ref;
    }
    else
    {
      // Insert the new scope at the end of the list.
      RdmnetScopeMonitorRef* ref = disc_state.scope_ref_list;
      for (; ref->next; ref = ref->next)
        ;
      ref->next = scope_ref;
    }
  }
}

/* Removes an entry from disc_state.scope_ref_list.
 * Assumes a lock is already taken. */
void scope_monitor_remove(const RdmnetScopeMonitorRef* ref)
{
  if (!disc_state.scope_ref_list)
    return;

  if (ref == disc_state.scope_ref_list)
  {
    // Remove the element at the head of the list
    disc_state.scope_ref_list = ref->next;
  }
  else
  {
    RdmnetScopeMonitorRef* prev_ref = disc_state.scope_ref_list;
    for (; prev_ref->next; prev_ref = prev_ref->next)
    {
      if (prev_ref->next == ref)
      {
        prev_ref->next = ref->next;
        break;
      }
    }
  }
}
