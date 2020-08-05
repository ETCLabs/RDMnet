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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "etcpal/common.h"
#include "etcpal/inet.h"
#include "etcpal/pack.h"
#include "rdmnet/core/util.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/disc/common.h"
#include "rdmnet/disc/discovered_broker.h"
#include "rdmnet/disc/registered_broker.h"
#include "rdmnet/disc/monitored_scope.h"

// Compile time check of memory configuration
#if !RDMNET_DYNAMIC_MEM
#error "RDMnet Discovery using Bonjour requires RDMNET_DYNAMIC_MEM to be enabled (defined nonzero)."
#endif

/**************************** Private constants ******************************/

/* Computed from the maximum size TXT record defined by the E1.33 standard. */
#define TXT_RECORD_BUFFER_LENGTH 663
#define REGISTRATION_STRING_PADDED_LENGTH E133_DNSSD_SRV_TYPE_PADDED_LENGTH + E133_SCOPE_STRING_PADDED_LENGTH + 4

/**************************** Private variables ******************************/

bool                     logged_poll_error = false;
static EtcPalPollContext poll_context;

/*********************** Private function prototypes *************************/

static DiscoveredBroker* discovered_broker_lookup_by_ref(DiscoveredBroker* list_head, DNSServiceRef dnssd_ref);
static void              get_registration_string(const char* srv_type, const char* scope, char* reg_str);
static void              broker_info_to_txt_record(const RdmnetBrokerRegisterRef* ref, TXTRecordRef* txt);
static bool              txt_record_to_broker_info(const unsigned char* txt, uint16_t txt_len, DiscoveredBroker* db);

/*************************** Function definitions ****************************/

/******************************************************************************
 * DNS-SD / Bonjour callback functions
 *****************************************************************************/

void DNSSD_API HandleDNSServiceRegisterReply(DNSServiceRef       sdRef,
                                             DNSServiceFlags     flags,
                                             DNSServiceErrorType errorCode,
                                             const char*         name,
                                             const char*         regtype,
                                             const char*         domain,
                                             void*               context)
{
  ETCPAL_UNUSED_ARG(sdRef);

  RdmnetBrokerRegisterRef* ref = (RdmnetBrokerRegisterRef*)context;
  RDMNET_ASSERT(ref);

  if (broker_register_ref_is_valid(ref))
  {
    if (flags & kDNSServiceFlagsAdd)
    {
      if (ref->callbacks.broker_registered)
      {
        ref->callbacks.broker_registered(ref, name, ref->callbacks.context);
      }
      DNSServiceConstructFullName(ref->full_service_name, name, regtype, domain);
    }
    else
    {
      if (ref->callbacks.broker_register_failed)
      {
        ref->callbacks.broker_register_failed(ref, errorCode, ref->callbacks.context);
      }
    }
  }
}

void DNSSD_API HandleDNSServiceGetAddrInfoReply(DNSServiceRef          sdRef,
                                                DNSServiceFlags        flags,
                                                uint32_t               interfaceIndex,
                                                DNSServiceErrorType    errorCode,
                                                const char*            hostname,
                                                const struct sockaddr* address,
                                                uint32_t               ttl,
                                                void*                  context)
{
  ETCPAL_UNUSED_ARG(interfaceIndex);
  ETCPAL_UNUSED_ARG(hostname);
  ETCPAL_UNUSED_ARG(ttl);

  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)context;
  RDMNET_ASSERT(ref);

  if (!scope_monitor_ref_is_valid(ref))
    return;

  DiscoveredBroker* db = discovered_broker_lookup_by_ref(ref->broker_list, sdRef);
  if (!db || db->platform_data.state != kResolveStateGetAddrInfo)
    return;

  if (errorCode != kDNSServiceErr_NoError)
  {
    // Remove the DiscoveredBroker from the list
    discovered_broker_remove(&ref->broker_list, db);
    discovered_broker_delete(db);
    return;
  }

  // We got a response, but we'll only clean up at the end if the flags tell us we're done getting
  // addrs.
  bool addrs_done = !(flags & kDNSServiceFlagsMoreComing);
  // Only copied to if addrs_done is true;

  // Update the broker info we're building
  EtcPalSockAddr ip_addr;
  if (sockaddr_os_to_etcpal(address, &ip_addr))
  {
    if (!etcpal_ip_is_loopback(&ip_addr.ip) && !etcpal_ip_is_wildcard(&ip_addr.ip))
    {
      discovered_broker_add_listen_addr(db, &ip_addr.ip);
    }
  }

  if (addrs_done)
  {
    // No more addresses, clean up.
    db->platform_data.state = kResolveStateDone;
    etcpal_poll_remove_socket(&poll_context, DNSServiceRefSockFD(sdRef));
    DNSServiceRefDeallocate(sdRef);

    RdmnetBrokerDiscInfo notify_info;
    discovered_broker_fill_disc_info(db, &notify_info);
    notify_broker_found(ref, &notify_info);
  }
}

void DNSSD_API HandleDNSServiceResolveReply(DNSServiceRef        sdRef,
                                            DNSServiceFlags      flags,
                                            uint32_t             interfaceIndex,
                                            DNSServiceErrorType  errorCode,
                                            const char*          fullname,
                                            const char*          hosttarget,
                                            uint16_t             port /* In network byte order */,
                                            uint16_t             txtLen,
                                            const unsigned char* txtRecord,
                                            void*                context)
{
  ETCPAL_UNUSED_ARG(flags);
  ETCPAL_UNUSED_ARG(interfaceIndex);
  ETCPAL_UNUSED_ARG(fullname);

  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)context;
  RDMNET_ASSERT(ref);

  if (!scope_monitor_ref_is_valid(ref))
    return;

  DiscoveredBroker* db = discovered_broker_lookup_by_ref(ref->broker_list, sdRef);
  if (!db || db->platform_data.state != kResolveStateServiceResolve)
    return;

  if (errorCode != kDNSServiceErr_NoError || !txt_record_to_broker_info(txtRecord, txtLen, db))
  {
    // Remove the DiscoveredBroker from the list
    discovered_broker_remove(&ref->broker_list, db);
    discovered_broker_delete(db);
    return;
  }

  DNSServiceErrorType getaddrinfo_err = kDNSServiceErr_NoError;

  // We got a response, clean up.  We don't need to keep resolving.
  etcpal_poll_remove_socket(&poll_context, DNSServiceRefSockFD(sdRef));
  DNSServiceRefDeallocate(sdRef);

  DNSServiceRef addr_ref;
  getaddrinfo_err = DNSServiceGetAddrInfo(&addr_ref, 0, 0, 0, hosttarget, &HandleDNSServiceGetAddrInfoReply, context);
  if (getaddrinfo_err == kDNSServiceErr_NoError)
  {
    // Update the broker info.
    db->port = etcpal_unpack_u16b((const uint8_t*)&port);

    db->platform_data.state = kResolveStateGetAddrInfo;
    db->platform_data.dnssd_ref = addr_ref;
    etcpal_poll_add_socket(&poll_context, DNSServiceRefSockFD(addr_ref), ETCPAL_POLL_IN, db->platform_data.dnssd_ref);
  }
}

void DNSSD_API HandleDNSServiceBrowseReply(DNSServiceRef       sdRef,
                                           DNSServiceFlags     flags,
                                           uint32_t            interfaceIndex,
                                           DNSServiceErrorType errorCode,
                                           const char*         serviceName,
                                           const char*         regtype,
                                           const char*         replyDomain,
                                           void*               context)
{
  ETCPAL_UNUSED_ARG(sdRef);
  ETCPAL_UNUSED_ARG(interfaceIndex);

  if (errorCode != kDNSServiceErr_NoError)
  {
    // TODO log?
    return;
  }

  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)context;
  RDMNET_ASSERT(ref);

  char full_name[kDNSServiceMaxDomainName];
  if (DNSServiceConstructFullName(full_name, serviceName, regtype, replyDomain) != kDNSServiceErr_NoError)
    return;

  if (!scope_monitor_ref_is_valid(ref))
    return;

  // Filter out the service name if it matches our broker instance name.
  if (ref->broker_handle && (strcmp(full_name, ref->broker_handle->full_service_name) == 0))
    return;

  if (flags & kDNSServiceFlagsAdd)
  {
    DNSServiceErrorType resolve_err = kDNSServiceErr_NoError;
    // Start the next part of the resolution.
    DNSServiceRef resolve_ref;
    resolve_err = DNSServiceResolve(&resolve_ref, 0, interfaceIndex, serviceName, regtype, replyDomain,
                                    HandleDNSServiceResolveReply, context);

    if (resolve_err == kDNSServiceErr_NoError)
    {
      // Track this resolve operation
      DiscoveredBroker* db = discovered_broker_find_by_name(ref->broker_list, full_name);
      if (!db)
      {
        // Allocate a new DiscoveredBroker to track info as it comes in.
        db = discovered_broker_new(ref, serviceName, full_name);
        if (db)
          discovered_broker_insert(&ref->broker_list, db);
      }
      if (db)
      {
        db->platform_data.state = kResolveStateServiceResolve;
        db->platform_data.dnssd_ref = resolve_ref;
        etcpal_poll_add_socket(&poll_context, DNSServiceRefSockFD(resolve_ref), ETCPAL_POLL_IN,
                               db->platform_data.dnssd_ref);
      }
    }
  }
  else
  {
    // Service removal
    notify_broker_lost(ref, serviceName);
    DiscoveredBroker* db = discovered_broker_find_by_name(ref->broker_list, full_name);
    if (db)
    {
      discovered_broker_remove(&ref->broker_list, db);
      discovered_broker_delete(db);
    }
  }
}

/******************************************************************************
 * Platform-specific functions from rdmnet/discovery/common.h
 *****************************************************************************/

etcpal_error_t rdmnet_disc_platform_init(const RdmnetNetintConfig* netint_config)
{
  // TODO restrict network interfaces using netint_config
  ETCPAL_UNUSED_ARG(netint_config);
  return etcpal_poll_context_init(&poll_context);
}

void rdmnet_disc_platform_deinit(void)
{
  etcpal_poll_context_deinit(&poll_context);
}

void rdmnet_disc_platform_tick()
{
  if (RDMNET_DISC_LOCK())
  {
    EtcPalPollEvent event = {ETCPAL_SOCKET_INVALID};
    etcpal_error_t  poll_res = etcpal_poll_wait(&poll_context, &event, 0);
    if (poll_res == kEtcPalErrOk && event.events & ETCPAL_POLL_IN)
    {
      DNSServiceErrorType process_error;
      process_error = DNSServiceProcessResult((DNSServiceRef)event.user_data);

      if (process_error != kDNSServiceErr_NoError)
      {
        etcpal_poll_remove_socket(&poll_context, event.socket);
        etcpal_close(event.socket);
      }
    }
    else if ((poll_res != kEtcPalErrTimedOut && poll_res != kEtcPalErrNoSockets) && !logged_poll_error)
    {
      RDMNET_LOG_CRIT("Socket poll operation for RDMnet discovery failed: '%s'", etcpal_strerror(poll_res));
      logged_poll_error = true;
    }
    RDMNET_DISC_UNLOCK();
  }
}

etcpal_error_t rdmnet_disc_platform_start_monitoring(RdmnetScopeMonitorRef* handle, int* platform_specific_error)
{
  // Start the browse operation in the Bonjour stack.
  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(E133_DNSSD_SRV_TYPE, handle->scope, reg_str);

  DNSServiceErrorType result = DNSServiceBrowse(&handle->platform_data.dnssd_ref, 0, 0, reg_str, handle->domain,
                                                &HandleDNSServiceBrowseReply, handle);
  if (result == kDNSServiceErr_NoError)
  {
    etcpal_poll_add_socket(&poll_context, DNSServiceRefSockFD(handle->platform_data.dnssd_ref), ETCPAL_POLL_IN,
                           handle->platform_data.dnssd_ref);
    return kEtcPalErrOk;
  }
  else
  {
    *platform_specific_error = result;
    return kEtcPalErrSys;
  }
}

void rdmnet_disc_platform_stop_monitoring(RdmnetScopeMonitorRef* handle)
{
  etcpal_poll_remove_socket(&poll_context, DNSServiceRefSockFD(handle->platform_data.dnssd_ref));
  DNSServiceRefDeallocate(handle->platform_data.dnssd_ref);
}

/* If returns !0, this was an error from Bonjour.  Reset the state and notify the callback.*/
etcpal_error_t rdmnet_disc_platform_register_broker(RdmnetBrokerRegisterRef* broker_ref, int* platform_specific_error)
{
  // Before we start the registration, we have to massage a few parameters
  uint16_t net_port = 0;
  etcpal_pack_u16b((uint8_t*)&net_port, broker_ref->port);

  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(E133_DNSSD_SRV_TYPE, broker_ref->scope, reg_str);

  TXTRecordRef txt;
  broker_info_to_txt_record(broker_ref, &txt);

  // TODO: If we want to register a device on a particular interface instead of all interfaces,
  // we'll have to have multiple reg_refs and do a DNSServiceRegister on each interface. Not
  // ideal.
  DNSServiceErrorType result = DNSServiceRegister(
      &broker_ref->platform_data.dnssd_ref, 0, 0, broker_ref->service_instance_name, reg_str, NULL, NULL, net_port,
      TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), HandleDNSServiceRegisterReply, broker_ref);

  if (result == kDNSServiceErr_NoError)
  {
    etcpal_poll_add_socket(&poll_context, DNSServiceRefSockFD(broker_ref->platform_data.dnssd_ref), ETCPAL_POLL_IN,
                           broker_ref->platform_data.dnssd_ref);
  }
  else
  {
    *platform_specific_error = result;
  }

  TXTRecordDeallocate(&txt);

  return (result == kDNSServiceErr_NoError ? kEtcPalErrOk : kEtcPalErrSys);
}

void rdmnet_disc_platform_unregister_broker(rdmnet_registered_broker_t handle)
{
  etcpal_poll_remove_socket(&poll_context, DNSServiceRefSockFD(handle->platform_data.dnssd_ref));
  DNSServiceRefDeallocate(handle->platform_data.dnssd_ref);
}

void discovered_broker_free_platform_resources(DiscoveredBroker* db)
{
  etcpal_poll_remove_socket(&poll_context, DNSServiceRefSockFD(db->platform_data.dnssd_ref));
  DNSServiceRefDeallocate(db->platform_data.dnssd_ref);
}

/******************************************************************************
 * helper functions
 *****************************************************************************/

/*
 * Searches for a DiscoveredBroker instance by associated DNSServiceRef in a list.
 * Returns the found instance or NULL if no match was found.
 * Assumes a lock is already taken.
 */
DiscoveredBroker* discovered_broker_lookup_by_ref(DiscoveredBroker* list_head, DNSServiceRef dnssd_ref)
{
  for (DiscoveredBroker* current = list_head; current; current = current->next)
  {
    if (current->platform_data.dnssd_ref == dnssd_ref)
    {
      return current;
    }
  }
  return NULL;
}

void get_registration_string(const char* srv_type, const char* scope, char* reg_str)
{
  ETCPAL_MSVC_BEGIN_NO_DEP_WARNINGS()

  // Bonjour adds in the _sub. for us.
  strncpy(reg_str, srv_type, REGISTRATION_STRING_PADDED_LENGTH);
  strcat(reg_str, ",_");
  strcat(reg_str, scope);

  ETCPAL_MSVC_END_NO_DEP_WARNINGS()
}

/* Create a TXT record with the required key/value pairs from E1.33 from the RdmnetBrokerDiscInfo */
void broker_info_to_txt_record(const RdmnetBrokerRegisterRef* ref, TXTRecordRef* txt)
{
  static uint8_t txt_buffer[TXT_RECORD_BUFFER_LENGTH];
  TXTRecordCreate(txt, TXT_RECORD_BUFFER_LENGTH, txt_buffer);

  char int_conversion[16];
  snprintf(int_conversion, 16, "%d", E133_DNSSD_TXTVERS);
  TXTRecordSetValue(txt, E133_TXT_VERS_KEY, (uint8_t)(strlen(int_conversion)), int_conversion);

  TXTRecordSetValue(txt, E133_TXT_SCOPE_KEY, (uint8_t)(strlen(ref->scope)), ref->scope);

  snprintf(int_conversion, 16, "%d", E133_DNSSD_E133VERS);
  TXTRecordSetValue(txt, E133_TXT_E133VERS_KEY, (uint8_t)(strlen(int_conversion)), int_conversion);

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

  TXTRecordSetValue(txt, E133_TXT_CID_KEY, (uint8_t)(strlen(cid_str)), cid_str);

  char uid_str[RDM_UID_STRING_BYTES];
  rdm_uid_to_string(&ref->uid, uid_str);

  // Strip colons from the UID string to conform to E1.33 TXT record rules.
  stripped_len = 0;
  for (size_t i = 0; uid_str[i] != '\0'; ++i)
  {
    if (uid_str[i] != ':')
      uid_str[stripped_len++] = uid_str[i];
  }
  uid_str[stripped_len] = '\0';

  TXTRecordSetValue(txt, E133_TXT_UID_KEY, (uint8_t)(strlen(uid_str)), uid_str);

  TXTRecordSetValue(txt, E133_TXT_MODEL_KEY, (uint8_t)(strlen(ref->model)), ref->model);

  TXTRecordSetValue(txt, E133_TXT_MANUFACTURER_KEY, (uint8_t)(strlen(ref->manufacturer)), ref->manufacturer);

  for (const DnsTxtRecordItemInternal* txt_item = ref->additional_txt_items;
       txt_item < ref->additional_txt_items + ref->num_additional_txt_items; ++txt_item)
  {
    TXTRecordSetValue(txt, txt_item->key, txt_item->value_len, txt_item->value);
  }
}

/*
 * Create an RdmnetBrokerDiscInfo struct from a TXT record with the required key/value pairs from
 * E1.33.
 */
bool txt_record_to_broker_info(const unsigned char* txt, uint16_t txt_len, DiscoveredBroker* db)
{
  uint8_t     value_len = 0;
  const char* value = NULL;

  // If the TXTVers key is not set to E133_DNSSD_TXT_VERS, we cannot parse this TXT record.
  value = TXTRecordGetValuePtr(txt_len, txt, E133_TXT_VERS_KEY, &value_len);
  if (!value || value_len == 0 || value_len > 15)
    return false;

  char number_conversion_buf[16];
  memcpy(number_conversion_buf, value, value_len);
  number_conversion_buf[value_len] = '\0';
  if (atoi(number_conversion_buf) != E133_DNSSD_TXTVERS)
    return false;

  for (uint16_t i = 0; i < TXTRecordGetCount(txt_len, txt); ++i)
  {
    char key[256];
    if (TXTRecordGetItemAtIndex(txt_len, txt, i, 256, key, &value_len, (const void**)&value) == kDNSServiceErr_NoError)
    {
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
        etcpal_string_to_uuid(cid_str, &db->cid);
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
        memcpy(number_conversion_buf, value, value_len);
        number_conversion_buf[value_len] = '\0';
        db->e133_version = atoi(number_conversion_buf);
      }
      else
      {
        // Add a non-standard TXT record item
        discovered_broker_add_txt_record_item(db, key, (const uint8_t*)value, value_len);
      }
    }
  }
  return true;
}
