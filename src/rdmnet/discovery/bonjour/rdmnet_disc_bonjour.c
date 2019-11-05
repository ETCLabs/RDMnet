/******************************************************************************
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
 ******************************************************************************
 * This file is a part of RDMnet. For more information, go to:
 * https://github.com/ETCLabs/RDMnet
 *****************************************************************************/

#include "rdmnet/discovery/common.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "etcpal/bool.h"
#include "etcpal/inet.h"
#include "etcpal/pack.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/opts.h"

// Compile time check of memory configuration
#if !RDMNET_DYNAMIC_MEM
#error "RDMnet Discovery using Bonjour requires RDMNET_DYNAMIC_MEM to be enabled (defined nonzero)."
#endif

/**************************** Private constants ******************************/

/* Computed from the maximum size TXT record defined by the E1.33 standard. */
#define TXT_RECORD_BUFFER_LENGTH 663
#define REGISTRATION_STRING_PADDED_LENGTH E133_DNSSD_SRV_TYPE_PADDED_LENGTH + E133_SCOPE_STRING_PADDED_LENGTH + 4

/**************************** Private variables ******************************/

static EtcPalPollContext poll_context;

/*********************** Private function prototypes *************************/

static DiscoveredBroker* discovered_broker_lookup_by_ref(DiscoveredBroker* list_head, DNSServiceRef dnssd_ref);

static void get_registration_string(const char* srv_type, const char* scope, char* reg_str);
static void broker_info_to_txt_record(const RdmnetBrokerDiscInfo* info, TXTRecordRef* txt);
static void txt_record_to_broker_info(const unsigned char* txt, uint16_t txt_len, RdmnetBrokerDiscInfo* info);

/*************************** Function definitions ****************************/

/******************************************************************************
 * DNS-SD / Bonjour callback functions
 ******************************************************************************/

void DNSSD_API HandleDNSServiceRegisterReply(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
                                             const char* name, const char* regtype, const char* domain, void* context)
{
  RDMNET_UNUSED_ARG(sdRef);

  RdmnetBrokerRegisterRef* ref = (RdmnetBrokerRegisterRef*)context;
  assert(ref);

  if (RDMNET_DISC_LOCK())
  {
    if (broker_register_ref_is_valid(ref))
    {
      if (flags & kDNSServiceFlagsAdd)
      {
        if (ref->config.callbacks.broker_registered)
        {
          ref->config.callbacks.broker_registered(ref, name, ref->config.callback_context);
        }
        DNSServiceConstructFullName(ref->full_service_name, name, regtype, domain);
      }
      else
      {
        if (ref->config.callbacks.broker_register_error)
        {
          ref->config.callbacks.broker_register_error(ref, errorCode, ref->config.callback_context);
        }
      }
    }
    RDMNET_DISC_UNLOCK();
  }
}

void DNSSD_API HandleDNSServiceGetAddrInfoReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                                DNSServiceErrorType errorCode, const char* hostname,
                                                const struct sockaddr* address, uint32_t ttl, void* context)
{
  RDMNET_UNUSED_ARG(interfaceIndex);
  RDMNET_UNUSED_ARG(hostname);
  RDMNET_UNUSED_ARG(ttl);

  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)context;
  assert(ref);

  if (RDMNET_DISC_LOCK())
  {
    if (!scope_monitor_ref_is_valid(ref))
    {
      RDMNET_DISC_UNLOCK();
      return;
    }

    DiscoveredBroker* db = discovered_broker_lookup_by_ref(ref->broker_list, sdRef);
    if (!db || db->platform_data.state != kResolveStateGetAddrInfo)
    {
      RDMNET_DISC_UNLOCK();
      return;
    }

    if (errorCode != kDNSServiceErr_NoError)
    {
      // Remove the DiscoveredBroker from the list
      discovered_broker_remove(&ref->broker_list, db);
      discovered_broker_delete(db);
      RDMNET_DISC_UNLOCK();
      return;
    }

    // We got a response, but we'll only clean up at the end if the flags tell us we're done getting
    // addrs.
    bool addrs_done = !(flags & kDNSServiceFlagsMoreComing);
    // Only copied to if addrs_done is true;
    RdmnetBrokerDiscInfo notify_info;

    // Update the broker info we're building
    EtcPalSockAddr ip_addr;
    if (sockaddr_os_to_etcpal(address, &ip_addr))
    {
      if (!etcpal_ip_is_loopback(&ip_addr.ip) && !etcpal_ip_is_wildcard(&ip_addr.ip))
      {
        // Add it to the info structure
        BrokerListenAddr* new_addr = (BrokerListenAddr*)malloc(sizeof(BrokerListenAddr));
        new_addr->addr = ip_addr.ip;
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
    }

    if (addrs_done)
    {
      // No more addresses, clean up.
      db->platform_data.state = kResolveStateDone;
      etcpal_poll_remove_socket(&poll_context, DNSServiceRefSockFD(sdRef));
      notify_info = db->info;
      DNSServiceRefDeallocate(sdRef);
      notify_broker_found(ref, &notify_info);
    }
    RDMNET_DISC_UNLOCK();
  }
}

void DNSSD_API HandleDNSServiceResolveReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                            DNSServiceErrorType errorCode, const char* fullname, const char* hosttarget,
                                            uint16_t port /* In network byte order */, uint16_t txtLen,
                                            const unsigned char* txtRecord, void* context)
{
  RDMNET_UNUSED_ARG(flags);
  RDMNET_UNUSED_ARG(interfaceIndex);
  RDMNET_UNUSED_ARG(fullname);

  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)context;
  assert(ref);

  if (RDMNET_DISC_LOCK())
  {
    if (!scope_monitor_ref_is_valid(ref))
    {
      RDMNET_DISC_UNLOCK();
      return;
    }

    DiscoveredBroker* db = discovered_broker_lookup_by_ref(ref->broker_list, sdRef);
    if (!db || db->platform_data.state != kResolveStateServiceResolve)
    {
      RDMNET_DISC_UNLOCK();
      return;
    }

    if (errorCode != kDNSServiceErr_NoError)
    {
      // Remove the DiscoveredBroker from the list
      discovered_broker_remove(&ref->broker_list, db);
      discovered_broker_delete(db);
      RDMNET_DISC_UNLOCK();
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
      db->info.port = etcpal_upack_16b((const uint8_t*)&port);

      db->platform_data.state = kResolveStateGetAddrInfo;
      db->platform_data.dnssd_ref = addr_ref;
      etcpal_poll_add_socket(&poll_context, DNSServiceRefSockFD(addr_ref), ETCPAL_POLL_IN, db->platform_data.dnssd_ref);
    }
    RDMNET_DISC_UNLOCK();
  }
}

void DNSSD_API HandleDNSServiceBrowseReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                           DNSServiceErrorType errorCode, const char* serviceName, const char* regtype,
                                           const char* replyDomain, void* context)
{
  RDMNET_UNUSED_ARG(sdRef);
  RDMNET_UNUSED_ARG(interfaceIndex);

  RdmnetScopeMonitorRef* ref = (RdmnetScopeMonitorRef*)context;
  assert(ref);

  char full_name[kDNSServiceMaxDomainName];
  if (DNSServiceConstructFullName(full_name, serviceName, regtype, replyDomain) != kDNSServiceErr_NoError)
    return;

  if (RDMNET_DISC_LOCK())
  {
    if (!scope_monitor_ref_is_valid(ref))
    {
      RDMNET_DISC_UNLOCK();
      return;
    }

    // Filter out the service name if it matches our broker instance name.
    if (ref->broker_handle && (strcmp(full_name, ref->broker_handle->full_service_name) == 0))
    {
      RDMNET_DISC_UNLOCK();
      return;
    }

    if (errorCode != kDNSServiceErr_NoError)
    {
      notify_scope_monitor_error(ref, errorCode);
      RDMNET_DISC_UNLOCK();
      return;
    }

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
        DiscoveredBroker* db = discovered_broker_lookup_by_name(ref->broker_list, full_name);
        if (!db)
        {
          // Allocate a new DiscoveredBroker to track info as it comes in.
          db = discovered_broker_new(serviceName, full_name);
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
      DiscoveredBroker* db = discovered_broker_lookup_by_name(ref->broker_list, full_name);
      if (db)
      {
        discovered_broker_remove(&ref->broker_list, db);
        discovered_broker_delete(db);
      }
    }
    RDMNET_DISC_UNLOCK();
  }
}

/******************************************************************************
 * Platform-specific functions from rdmnet/discovery/common.h
 ******************************************************************************/

etcpal_error_t rdmnet_disc_platform_init(void)
{
  return etcpal_poll_context_init(&poll_context);
}

void rdmnet_disc_platform_deinit(void)
{
  etcpal_poll_context_deinit(&poll_context);
}

etcpal_error_t rdmnet_disc_platform_start_monitoring(const RdmnetScopeMonitorConfig* config,
                                                     RdmnetScopeMonitorRef* handle, int* platform_specific_error)
{
  // Start the browse operation in the Bonjour stack.
  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(E133_DNSSD_SRV_TYPE, config->scope, reg_str);

  DNSServiceErrorType result = DNSServiceBrowse(&handle->platform_data.dnssd_ref, 0, 0, reg_str, config->domain,
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

/* If returns !0, this was an error from Bonjour.  Reset the state and notify the callback.*/
etcpal_error_t rdmnet_disc_platform_register_broker(const RdmnetBrokerDiscInfo* info,
                                                    RdmnetBrokerRegisterRef* broker_ref, int* platform_specific_error)
{
  // Before we start the registration, we have to massage a few parameters
  uint16_t net_port = 0;
  etcpal_pack_16b((uint8_t*)&net_port, info->port);

  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(E133_DNSSD_SRV_TYPE, info->scope, reg_str);

  TXTRecordRef txt;
  broker_info_to_txt_record(info, &txt);

  // TODO: If we want to register a device on a particular interface instead of all interfaces,
  // we'll have to have multiple reg_refs and do a DNSServiceRegister on each interface. Not
  // ideal.
  DNSServiceErrorType result = DNSServiceRegister(
      &broker_ref->platform_data.dnssd_ref, 0, 0, info->service_name, reg_str, NULL, NULL, net_port,
      TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), HandleDNSServiceRegisterReply, broker_ref);

  if (result == kDNSServiceErr_NoError)
  {
    etcpal_poll_add_socket(&poll_context, DNSServiceRefSockFD(broker_ref->platform_data.dnssd_ref), ETCPAL_POLL_IN,
                           broker_ref->platform_data.dnssd_ref);
  }

  TXTRecordDeallocate(&txt);

  return (result == kDNSServiceErr_NoError ? kEtcPalErrOk : kEtcPalErrSys);
}

void rdmnet_disc_platform_tick()
{
  EtcPalPollEvent event = {ETCPAL_SOCKET_INVALID};
  etcpal_error_t poll_res = kEtcPalErrSys;

  if (RDMNET_DISC_LOCK())
  {
    poll_res = etcpal_poll_wait(&poll_context, &event, 0);
    RDMNET_DISC_UNLOCK();
  }

  if (poll_res == kEtcPalErrOk && event.events & ETCPAL_POLL_IN)
  {
    DNSServiceErrorType process_error;
    process_error = DNSServiceProcessResult((DNSServiceRef)event.user_data);

    if (process_error != kDNSServiceErr_NoError)
    {
      if (RDMNET_DISC_LOCK())
      {
        etcpal_poll_remove_socket(&poll_context, event.socket);
        etcpal_close(event.socket);
        RDMNET_DISC_UNLOCK();
      }
    }
  }
  else if (poll_res != kEtcPalErrTimedOut)
  {
    // TODO error handling
  }
}

/* Searches for a DiscoveredBroker instance by associated DNSServiceRef in a list.
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

/******************************************************************************
 * helper functions
 ******************************************************************************/

void get_registration_string(const char* srv_type, const char* scope, char* reg_str)
{
  RDMNET_MSVC_BEGIN_NO_DEP_WARNINGS()

  // Bonjour adds in the _sub. for us.
  RDMNET_MSVC_NO_DEP_WRN strncpy(reg_str, srv_type, REGISTRATION_STRING_PADDED_LENGTH);
  strcat(reg_str, ",");
  strcat(reg_str, "_");
  strcat(reg_str, scope);

  RDMNET_MSVC_END_NO_DEP_WARNINGS()
}

/* Create a TXT record with the required key/value pairs from E1.33 from the RdmnetBrokerDiscInfo */
void broker_info_to_txt_record(const RdmnetBrokerDiscInfo* info, TXTRecordRef* txt)
{
  static uint8_t txt_buffer[TXT_RECORD_BUFFER_LENGTH];
  TXTRecordCreate(txt, TXT_RECORD_BUFFER_LENGTH, txt_buffer);

  char int_conversion[16];
  snprintf(int_conversion, 16, "%d", E133_DNSSD_TXTVERS);
  DNSServiceErrorType result = TXTRecordSetValue(txt, "TxtVers", (uint8_t)(strlen(int_conversion)), int_conversion);
  if (result == kDNSServiceErr_NoError)
  {
    result = TXTRecordSetValue(txt, "ConfScope", (uint8_t)(strlen(info->scope)), info->scope);
  }
  if (result == kDNSServiceErr_NoError)
  {
    snprintf(int_conversion, 16, "%d", E133_DNSSD_E133VERS);
    result = TXTRecordSetValue(txt, "E133Vers", (uint8_t)(strlen(int_conversion)), int_conversion);
  }
  if (result == kDNSServiceErr_NoError)
  {
    /*The CID can't have hyphens, so we'll strip them.*/
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    etcpal_uuid_to_string(&info->cid, cid_str);
    char* src = cid_str;
    for (char* dst = src; *dst != 0; ++src, ++dst)
    {
      if (*src == '-')
        ++src;

      *dst = *src;
    }
    result = TXTRecordSetValue(txt, "CID", (uint8_t)(strlen(cid_str)), cid_str);
  }
  if (result == kDNSServiceErr_NoError)
  {
    result = TXTRecordSetValue(txt, "Model", (uint8_t)(strlen(info->model)), info->model);
  }
  if (result == kDNSServiceErr_NoError)
  {
    result = TXTRecordSetValue(txt, "Manuf", (uint8_t)(strlen(info->manufacturer)), info->manufacturer);
  }
}

/* Create an RdmnetBrokerDiscInfo struct from a TXT record with the required key/value pairs from
 * E1.33.
 */
void txt_record_to_broker_info(const unsigned char* txt, uint16_t txt_len, RdmnetBrokerDiscInfo* info)
{
  uint8_t value_len = 0;
  const char* value;

  value = (const char*)(TXTRecordGetValuePtr(txt_len, txt, "ConfScope", &value_len));
  if (value && value_len)
  {
    rdmnet_safe_strncpy(
        info->scope, value,
        (value_len + 1 > E133_SCOPE_STRING_PADDED_LENGTH ? E133_SCOPE_STRING_PADDED_LENGTH : value_len + 1));
  }

  value = (const char*)(TXTRecordGetValuePtr(txt_len, txt, "CID", &value_len));
  if (value && value_len && value_len < ETCPAL_UUID_STRING_BYTES)
  {
    char cid_str[ETCPAL_UUID_STRING_BYTES];
    rdmnet_safe_strncpy(cid_str, value, value_len + 1);
    etcpal_string_to_uuid(cid_str, &info->cid);
  }

  value = (const char*)(TXTRecordGetValuePtr(txt_len, txt, "Model", &value_len));
  if (value && value_len)
  {
    rdmnet_safe_strncpy(
        info->model, value,
        (value_len + 1 > E133_MODEL_STRING_PADDED_LENGTH ? E133_MODEL_STRING_PADDED_LENGTH : value_len + 1));
  }

  value = (const char*)(TXTRecordGetValuePtr(txt_len, txt, "Manuf", &value_len));
  if (value && value_len)
  {
    rdmnet_safe_strncpy(info->manufacturer, value,
                        (value_len + 1 > E133_MANUFACTURER_STRING_PADDED_LENGTH ? E133_MANUFACTURER_STRING_PADDED_LENGTH
                                                                                : value_len + 1));
  }
}
