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

#include "rdmnet/discovery/bonjour.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "lwpa/inet.h"
#include "lwpa/bool.h"
#include "lwpa/pack.h"
#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/opts.h"

// DEBUG REMOVEME
#include <intrin.h>

// Compile time check of memory configuration
#if !RDMNET_DYNAMIC_MEM
#error "RDMnet Discovery using Bonjour requires RDMNET_DYNAMIC_MEM to be enabled (defined nonzero)."
#endif

/**************************** Private variables ******************************/

typedef struct DiscoveryState
{
  lwpa_mutex_t lock;

  RdmnetScopeMonitorRef *scope_ref_list;
  RdmnetBrokerRegisterConfig registered_broker;

  RdmnetBrokerRegisterRef broker_ref;

  LwpaPollContext poll_context;
} DiscoveryState;

static DiscoveryState disc_state;

/*********************** Private function prototypes *************************/

// Allocation and deallocation
RdmnetScopeMonitorRef *scope_monitor_new(const RdmnetScopeMonitorConfig *config);
void scope_monitor_delete(RdmnetScopeMonitorRef *ref);
DiscoveredBroker *discovered_broker_new(const char *service_name, const char *full_service_name);
void discovered_broker_delete(DiscoveredBroker *db);
RdmnetBrokerRegisterRef *registered_broker_new();
void registered_broker_delete(RdmnetBrokerRegisterRef *rb);

// Add and remove from appropriate lists
void discovered_broker_insert(DiscoveredBroker **list_head_ptr, DiscoveredBroker *new);
DiscoveredBroker *discovered_broker_lookup_by_name(DiscoveredBroker *list_head, const char *full_name);
DiscoveredBroker *discovered_broker_lookup_by_ref(DiscoveredBroker *list_head, DNSServiceRef dnssd_ref);
void discovered_broker_remove(DiscoveredBroker **list_head_ptr, const DiscoveredBroker *db);
void scope_monitor_insert(RdmnetScopeMonitorRef *scope_ref);
RdmnetScopeMonitorRef *scope_monitor_lookup(DNSServiceRef dnssd_ref);
void scope_monitor_remove(const RdmnetScopeMonitorRef *ref);

// Notify the appropriate callbacks
void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info);
void notify_broker_lost(rdmnet_scope_monitor_t handle, const char *service_name);
void notify_scope_monitor_error(rdmnet_scope_monitor_t handle, int platform_error);

// Other helpers
void get_registration_string(const char *srv_type, const char *scope, char *reg_str);
bool broker_info_is_valid(const RdmnetBrokerDiscInfo *info);
bool ipv6_valid(LwpaIpAddr *ip);

/*************************** Function definitions ****************************/

/******************************************************************************
 * DNS-SD / Bonjour functions
 ******************************************************************************/

void DNSSD_API HandleDNSServiceRegisterReply(DNSServiceRef sdRef, DNSServiceFlags flags, DNSServiceErrorType errorCode,
                                             const char *name, const char *regtype, const char *domain, void *context)
{
  (void)context;

  rdmnet_registered_broker_t broker_handle = &disc_state.broker_ref;
  if (!broker_handle)
    return;

  if (sdRef == disc_state.broker_ref.dnssd_ref)
  {
    if (flags & kDNSServiceFlagsAdd)
    {
      if (broker_handle && broker_handle->config.callbacks.broker_registered)
      {
        broker_handle->config.callbacks.broker_registered(broker_handle, name, broker_handle->config.callback_context);
      }
      DNSServiceConstructFullName(disc_state.broker_ref.full_service_name, name, regtype, domain);
    }
    else
    {
      if (broker_handle->config.callbacks.broker_register_error)
      {
        broker_handle->config.callbacks.broker_register_error(broker_handle, errorCode,
                                                              broker_handle->config.callback_context);
      }
    }
  }
}

void DNSSD_API HandleDNSServiceGetAddrInfoReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                                DNSServiceErrorType errorCode, const char *hostname,
                                                const struct sockaddr *address, uint32_t ttl, void *context)
{
  (void)interfaceIndex;
  (void)hostname;
  (void)ttl;

  RdmnetScopeMonitorRef *ref = (RdmnetScopeMonitorRef *)context;
  if (!ref)
    return;

  DiscoveredBroker *db = discovered_broker_lookup_by_ref(ref->broker_list, sdRef);
  if (!db || db->state != kResolveStateGetAddrInfo)
    return;

  if (errorCode != kDNSServiceErr_NoError)
  {
    notify_scope_monitor_error(ref, errorCode);
    if (lwpa_mutex_take(&disc_state.lock))
    {
      // Remove the DiscoveredBroker from the list
      discovered_broker_remove(&ref->broker_list, db);
      discovered_broker_delete(db);
      lwpa_mutex_give(&disc_state.lock);
    }
  }
  else
  {
    // We got a response, but we'll only clean up at the end if the flags tell us we're done getting
    // addrs.
    bool addrs_done = !(flags & kDNSServiceFlagsMoreComing);
    // Only copied to if addrs_done is true;
    RdmnetBrokerDiscInfo notify_info;

    if (lwpa_mutex_take(&disc_state.lock))
    {
      // Update the broker info we're building
      LwpaSockaddr ip_addr;
      if (sockaddr_os_to_lwpa(address, &ip_addr))
      {
        if ((LWPA_IP_IS_V4(&ip_addr.ip) && LWPA_IP_V4_ADDRESS(&ip_addr.ip) != 0) ||
            (LWPA_IP_IS_V6(&ip_addr.ip) && ipv6_valid(&ip_addr.ip)))
        {
          // Add it to the info structure
          BrokerListenAddr *new_addr = (BrokerListenAddr*)malloc(sizeof(BrokerListenAddr));
          new_addr->addr = ip_addr.ip;
          new_addr->next = NULL;

          if (!db->info.listen_addr_list)
          {
            db->info.listen_addr_list = new_addr;
          }
          else
          {
            BrokerListenAddr *cur_addr = db->info.listen_addr_list;
            while (cur_addr->next)
              cur_addr = cur_addr->next;
            cur_addr->next = new_addr;
          }
        }
      }

      if (addrs_done)
      {
        db->state = kResolveStateDone;
        lwpa_poll_remove_socket(&disc_state.poll_context, DNSServiceRefSockFD(sdRef));
        notify_info = db->info;
      }
      lwpa_mutex_give(&disc_state.lock);
    }

    /*No more addresses, clean up.*/
    if (addrs_done)
    {
      DNSServiceRefDeallocate(sdRef);
      notify_broker_found(ref, &notify_info);
    }
  }
}

void DNSSD_API HandleDNSServiceResolveReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                            DNSServiceErrorType errorCode, const char *fullname, const char *hosttarget,
                                            uint16_t port /* In network byte order */, uint16_t txtLen,
                                            const unsigned char *txtRecord, void *context)
{
  (void)flags;
  (void)interfaceIndex;
  (void)fullname;

  RdmnetScopeMonitorRef *ref = (RdmnetScopeMonitorRef *)context;
  if (!ref)
    return;

  DiscoveredBroker *db = discovered_broker_lookup_by_ref(ref->broker_list, sdRef);
  if (!db || db->state != kResolveStateServiceResolve)
    return;

  if (errorCode != kDNSServiceErr_NoError)
  {
    notify_scope_monitor_error(ref, errorCode);
    if (lwpa_mutex_take(&disc_state.lock))
    {
      // Remove the DiscoveredBroker from the list
      discovered_broker_remove(&ref->broker_list, db);
      discovered_broker_delete(db);
      lwpa_mutex_give(&disc_state.lock);
    }
  }
  else
  {
    DNSServiceErrorType getaddrinfo_err = kDNSServiceErr_NoError;

    // We have to take the lock before the DNSServiceGetAddrInfo call, because we need to add the
    // ref to our map before it responds.
    if (lwpa_mutex_take(&disc_state.lock))
    {
      // We got a response, clean up.  We don't need to keep resolving.
      lwpa_poll_remove_socket(&disc_state.poll_context, DNSServiceRefSockFD(sdRef));
      DNSServiceRefDeallocate(sdRef);

      DNSServiceRef addr_ref;
      getaddrinfo_err =
          DNSServiceGetAddrInfo(&addr_ref, 0, 0, 0, hosttarget, &HandleDNSServiceGetAddrInfoReply, context);
      if (getaddrinfo_err == kDNSServiceErr_NoError)
      {
        // Update the broker info.
        db->info.port = lwpa_upack_16b(&port);

        uint8_t value_len = 0;
        const char *value;
        // char sval[16];

        // value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "TxtVers", &value_len));
        // if (value && value_len)
        //{
        //  rdmnet_safe_strncpy(sval, value, 16);
        //  info->txt_vers = atoi(sval);
        //}

        value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "ConfScope", &value_len));
        if (value && value_len)
        {
          rdmnet_safe_strncpy(
              db->info.scope, value,
              (value_len + 1 > E133_SCOPE_STRING_PADDED_LENGTH ? E133_SCOPE_STRING_PADDED_LENGTH : value_len + 1));
        }

        // value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "E133Vers", &value_len));
        // if (value && value_len)
        //{
        //  rdmnet_safe_strncpy(sval, value, 16);
        //  info->e133_vers = atoi(sval);
        //}

        value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "CID", &value_len));
        if (value && value_len)
          lwpa_string_to_uuid(&db->info.cid, value, value_len);

        value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "Model", &value_len));
        if (value && value_len)
        {
          rdmnet_safe_strncpy(
              db->info.model, value,
              (value_len + 1 > E133_MODEL_STRING_PADDED_LENGTH ? E133_MODEL_STRING_PADDED_LENGTH : value_len + 1));
        }

        value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "Manuf", &value_len));
        if (value && value_len)
        {
          rdmnet_safe_strncpy(
              db->info.manufacturer, value,
              (value_len + 1 > E133_MANUFACTURER_STRING_PADDED_LENGTH ? E133_MANUFACTURER_STRING_PADDED_LENGTH
                                                                      : value_len + 1));
        }

        db->state = kResolveStateGetAddrInfo;
        db->dnssd_ref = addr_ref;
        lwpa_poll_add_socket(&disc_state.poll_context, DNSServiceRefSockFD(addr_ref), LWPA_POLL_IN, db->dnssd_ref);
      }
      lwpa_mutex_give(&disc_state.lock);
    }

    if (getaddrinfo_err != kDNSServiceErr_NoError)
      notify_scope_monitor_error(ref, getaddrinfo_err);
  }
}

void DNSSD_API HandleDNSServiceBrowseReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                           DNSServiceErrorType errorCode, const char *serviceName, const char *regtype,
                                           const char *replyDomain, void *context)
{
  (void)sdRef;
  (void)interfaceIndex;

  RdmnetScopeMonitorRef *ref = (RdmnetScopeMonitorRef *)context;
  if (!ref)
    return;

  char full_name[kDNSServiceMaxDomainName] = {0};
  if (DNSServiceConstructFullName(full_name, serviceName, regtype, replyDomain) != kDNSServiceErr_NoError)
    return;

  if (ref->broker_handle)
  {
    // Filter out the service name if it matches our broker instance name.
    if (strcmp(full_name, ref->broker_handle->full_service_name) == 0)
      return;
  }

  if (errorCode != kDNSServiceErr_NoError)
  {
    notify_scope_monitor_error(ref, errorCode);
  }
  else if (flags & kDNSServiceFlagsAdd)
  {
    // We have to take the lock before the DNSServiceResolve call, because we need to add the ref to
    // our map before it responds.
    DNSServiceErrorType resolve_err = kDNSServiceErr_NoError;
    if (lwpa_mutex_take(&disc_state.lock))
    {
      // Start the next part of the resolution.
      DNSServiceRef resolve_ref;
      resolve_err = DNSServiceResolve(&resolve_ref, 0, interfaceIndex, serviceName, regtype, replyDomain,
                                      HandleDNSServiceResolveReply, context);

      if (resolve_err == kDNSServiceErr_NoError)
      {
        // Track this resolve operation
        DiscoveredBroker *db = discovered_broker_lookup_by_name(ref->broker_list, full_name);
        if (!db)
        {
          // Allocate a new DiscoveredBroker to track info as it comes in.
          db = discovered_broker_new(serviceName, full_name);
          if (db)
            discovered_broker_insert(&ref->broker_list, db);
        }
        if (db)
        {
          db->state = kResolveStateServiceResolve;
          db->dnssd_ref = resolve_ref;
          lwpa_poll_add_socket(&disc_state.poll_context, DNSServiceRefSockFD(resolve_ref), LWPA_POLL_IN, db->dnssd_ref);
        }
      }

      lwpa_mutex_give(&disc_state.lock);
    }

    if (resolve_err != kDNSServiceErr_NoError)
      notify_scope_monitor_error(ref, resolve_err);
  }
  else
  {
    /*Service removal*/
    notify_broker_lost(ref, serviceName);
  }
}

/******************************************************************************
 * public functions
 ******************************************************************************/

lwpa_error_t rdmnetdisc_init()
{
  lwpa_error_t res = kLwpaErrOk;

  if (!lwpa_mutex_create(&disc_state.lock))
    res = kLwpaErrSys;

  if (res == kLwpaErrOk)
    res = lwpa_poll_context_init(&disc_state.poll_context);

  if (res == kLwpaErrOk)
    disc_state.broker_ref.state = kBrokerStateNotRegistered;

  return res;
}

void rdmnetdisc_deinit()
{
  rdmnetdisc_stop_monitoring_all();
  lwpa_poll_context_deinit(&disc_state.poll_context);
  lwpa_mutex_destroy(&disc_state.lock);
}

void rdmnetdisc_fill_default_broker_info(RdmnetBrokerDiscInfo *broker_info)
{
  broker_info->cid = kLwpaNullUuid;
  memset(broker_info->service_name, 0, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  broker_info->port = 0;
  broker_info->listen_addr_list = NULL;
  rdmnet_safe_strncpy(broker_info->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
  memset(broker_info->model, 0, E133_MODEL_STRING_PADDED_LENGTH);
  memset(broker_info->manufacturer, 0, E133_MANUFACTURER_STRING_PADDED_LENGTH);
}

lwpa_error_t rdmnetdisc_start_monitoring(const RdmnetScopeMonitorConfig *config, rdmnet_scope_monitor_t *handle,
                                         int *platform_specific_error)
{
  if (!config || !handle || !platform_specific_error)
    return kLwpaErrInvalid;
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  RdmnetScopeMonitorRef *new_monitor = scope_monitor_new(config);
  if (!new_monitor)
    return kLwpaErrNoMem;

  // Start the browse operation in the Bonjour stack.
  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(E133_DNSSD_SRV_TYPE, config->scope, reg_str);

  // We have to take the lock before the DNSServiceBrowse call, because we need to add the ref to
  // our map before it responds.
  lwpa_error_t res = kLwpaErrOk;
  if (lwpa_mutex_take(&disc_state.lock))
  {
    DNSServiceErrorType result = DNSServiceBrowse(&new_monitor->dnssd_ref, 0, 0, reg_str, config->domain,
                                                  &HandleDNSServiceBrowseReply, new_monitor);
    if (result == kDNSServiceErr_NoError)
    {
      lwpa_poll_add_socket(&disc_state.poll_context, DNSServiceRefSockFD(new_monitor->dnssd_ref), LWPA_POLL_IN,
                           new_monitor->dnssd_ref);
      scope_monitor_insert(new_monitor);
    }
    else
    {
      *platform_specific_error = result;
      scope_monitor_delete(new_monitor);
      res = kLwpaErrSys;
    }
    lwpa_mutex_give(&disc_state.lock);
  }

  if (res == kLwpaErrOk)
    *handle = new_monitor;

  return res;
}

lwpa_error_t rdmnetdisc_change_monitored_scope(rdmnet_scope_monitor_t handle,
                                               const RdmnetScopeMonitorConfig *new_config)
{
  // TODO reevaluate if this is necessary.
  (void)handle;
  (void)new_config;
  return kLwpaErrNotImpl;
  /*
    lwpa_error_t res = kLwpaErrSys;
    if (lwpa_mutex_take(&disc_state.lock, LWPA_WAIT_FOREVER))
    {
      // Stop the existing browse operation
      DNSServiceRefDeallocate(handle->dnssd_ref);
      handle->socket = LWPA_SOCKET_INVALID;

      // Copy in the new config
      handle->config = *new_config;

      // Start the new browse operation in the Bonjour stack.
      char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
      get_registration_string(E133_DNSSD_SRV_TYPE, new_config->scope, reg_str);

      DNSServiceErrorType result =
          DNSServiceBrowse(&handle->dnssd_ref, 0, 0, reg_str, new_config->domain, &HandleDNSServiceBrowseReply, handle);
      if (result == kDNSServiceErr_NoError)
      {
        handle->socket = DNSServiceRefSockFD(handle->dnssd_ref);
        res = kLwpaErrOk;
      }
      else
      {
        res = kLwpaErrSys;
      }
      lwpa_mutex_give(&disc_state.lock);
    }
    return res;
    */
}

void rdmnetdisc_stop_monitoring(rdmnet_scope_monitor_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (lwpa_mutex_take(&disc_state.lock))
  {
    scope_monitor_remove(handle);
    scope_monitor_delete(handle);
    lwpa_mutex_give(&disc_state.lock);
  }
}

void rdmnetdisc_stop_monitoring_all()
{
  if (!rdmnet_core_initialized())
    return;

  if (lwpa_mutex_take(&disc_state.lock))
  {
    if (disc_state.scope_ref_list)
    {
      RdmnetScopeMonitorRef *ref = disc_state.scope_ref_list;
      RdmnetScopeMonitorRef *next_ref;
      while (ref)
      {
        next_ref = ref->next;
        scope_monitor_delete(ref);
        ref = next_ref;
      }
      disc_state.scope_ref_list = NULL;
    }
    lwpa_mutex_give(&disc_state.lock);
  }
}

lwpa_error_t rdmnetdisc_register_broker(const RdmnetBrokerRegisterConfig *config, rdmnet_registered_broker_t *handle)
{
  if (!config || !handle || disc_state.broker_ref.state != kBrokerStateNotRegistered ||
      !broker_info_is_valid(&config->my_info))
  {
    return kLwpaErrInvalid;
  }
  if (!rdmnet_core_initialized())
    return kLwpaErrNotInit;

  disc_state.broker_ref.config = *config;
  disc_state.broker_ref.state = kBrokerStateInfoSet;

  *handle = &disc_state.broker_ref;
  return kLwpaErrOk;
}

void rdmnetdisc_unregister_broker(rdmnet_registered_broker_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (disc_state.broker_ref.state != kBrokerStateNotRegistered)
  {
    DNSServiceRefDeallocate(disc_state.broker_ref.dnssd_ref);

    /* Since the broker only cares about scopes while it is running, shut down any outstanding
     * queries for that scope.*/
    rdmnetdisc_stop_monitoring(handle->scope_monitor_handle);

    /*Reset the state*/
    disc_state.broker_ref.state = kBrokerStateNotRegistered;
  }
}

/* If returns !0, this was an error from Bonjour.  Reset the state and notify the callback.*/
DNSServiceErrorType send_registration(const RdmnetBrokerDiscInfo *info, DNSServiceRef *created_ref, void *context)
{
  DNSServiceErrorType result = kDNSServiceErr_NoError;

  /*Before we start the registration, we have to massage a few parameters*/
  uint16_t net_port = 0;
  lwpa_pack_16b(&net_port, info->port);

  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(E133_DNSSD_SRV_TYPE, info->scope, reg_str);

  u_int txt_buffer[TXT_RECORD_BUFFER_LENGTH];
  TXTRecordRef txt;
  TXTRecordCreate(&txt, TXT_RECORD_BUFFER_LENGTH, txt_buffer);

  char int_conversion[16];
  snprintf(int_conversion, 16, "%d", E133_DNSSD_TXTVERS);
  result = TXTRecordSetValue(&txt, "TxtVers", (uint8_t)(strlen(int_conversion)), int_conversion);
  if (result == kDNSServiceErr_NoError)
  {
    result = TXTRecordSetValue(&txt, "ConfScope", (uint8_t)(strlen(info->scope)), info->scope);
  }
  if (result == kDNSServiceErr_NoError)
  {
    snprintf(int_conversion, 16, "%d", E133_DNSSD_E133VERS);
    result = TXTRecordSetValue(&txt, "E133Vers", (uint8_t)(strlen(int_conversion)), int_conversion);
  }
  if (result == kDNSServiceErr_NoError)
  {
    /*The CID can't have hyphens, so we'll strip them.*/
    char cid_str[LWPA_UUID_STRING_BYTES];
    lwpa_uuid_to_string(cid_str, &info->cid);
    char *src = cid_str;
    for (char *dst = src; *dst != 0; ++src, ++dst)
    {
      if (*src == '-')
        ++src;

      *dst = *src;
    }
    result = TXTRecordSetValue(&txt, "CID", (uint8_t)(strlen(cid_str)), cid_str);
  }
  if (result == kDNSServiceErr_NoError)
  {
    result = TXTRecordSetValue(&txt, "Model", (uint8_t)(strlen(info->model)), info->model);
  }
  if (result == kDNSServiceErr_NoError)
  {
    result = TXTRecordSetValue(&txt, "Manuf", (uint8_t)(strlen(info->manufacturer)), info->manufacturer);
  }

  if (result == kDNSServiceErr_NoError)
  {
    // TODO: If we want to register a device on a particular interface instead of all interfaces,
    // we'll have to have multiple reg_refs and do a DNSServiceRegister on each interface. Not
    // ideal.
    result = DNSServiceRegister(created_ref, 0, 0, info->service_name, reg_str, NULL, NULL, net_port,
                                TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), HandleDNSServiceRegisterReply,
                                context);
  }

  TXTRecordDeallocate(&txt);

  return result;
}

void rdmnetdisc_tick()
{
  if (!rdmnet_core_initialized())
    return;

  // TODO: For now, we are only allowing one registered broker. This should be able to be expanded
  // to allow up to n brokers, however.
  RdmnetBrokerRegisterRef *broker_ref = &disc_state.broker_ref;
  switch (broker_ref->state)
  {
    case kBrokerStateInfoSet:
    {
      // The info was set. Start the registration and monitoring
      broker_ref->state = kBrokerStateRegisterStarted;

      DNSServiceErrorType reg_result =
          send_registration(&broker_ref->config.my_info, &broker_ref->dnssd_ref, broker_ref);

      if (reg_result == kDNSServiceErr_NoError)
      {
        lwpa_poll_add_socket(&disc_state.poll_context, DNSServiceRefSockFD(broker_ref->dnssd_ref), LWPA_POLL_IN,
                             broker_ref->dnssd_ref);

        // Begin monitoring the broker's scope for other brokers
        RdmnetScopeMonitorConfig config;
        rdmnet_safe_strncpy(config.scope, broker_ref->config.my_info.scope, E133_SCOPE_STRING_PADDED_LENGTH);
        rdmnet_safe_strncpy(config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);

        int mon_error;
        rdmnet_scope_monitor_t monitor_handle;
        if (rdmnetdisc_start_monitoring(&config, &monitor_handle, &mon_error) == kLwpaErrOk)
        {
          broker_ref->scope_monitor_handle = monitor_handle;
          monitor_handle->broker_handle = broker_ref;
        }
        else if (broker_ref->config.callbacks.scope_monitor_error)
        {
          broker_ref->config.callbacks.scope_monitor_error(broker_ref, config.scope, mon_error,
                                                           broker_ref->config.callback_context);
        }
      }
      else
      {
        broker_ref->state = kBrokerStateNotRegistered;
        if (broker_ref->config.callbacks.broker_register_error != NULL)
        {
          broker_ref->config.callbacks.broker_register_error(broker_ref, reg_result,
                                                             broker_ref->config.callback_context);
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

  LwpaPollEvent event = {LWPA_SOCKET_INVALID};
  lwpa_error_t poll_res = kLwpaErrSys;

  // Do the poll inside the mutex so that sockets can't be added and removed during poll
  // Since we are using an immediate timeout, this is not a big deal
  if (lwpa_mutex_take(&disc_state.lock))
  {
    poll_res = lwpa_poll_wait(&disc_state.poll_context, &event, 0);
    lwpa_mutex_give(&disc_state.lock);
  }
  if (poll_res == kLwpaErrOk && event.events & LWPA_POLL_IN)
  {
    DNSServiceErrorType process_error;
    process_error = DNSServiceProcessResult((DNSServiceRef)event.user_data);

    if (process_error != kDNSServiceErr_NoError)
    {
      lwpa_poll_remove_socket(&disc_state.poll_context, event.socket);
      lwpa_close(event.socket);
    }
  }
  else if (poll_res != kLwpaErrTimedOut)
  {
    // TODO error handling
  }
}

void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info)
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

void notify_broker_lost(rdmnet_scope_monitor_t handle, const char *service_name)
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

void get_registration_string(const char *srv_type, const char *scope, char *reg_str)
{
  RDMNET_MSVC_BEGIN_NO_DEP_WARNINGS()

  // Bonjour adds in the _sub. for us.
  RDMNET_MSVC_NO_DEP_WRN strncpy(reg_str, srv_type, REGISTRATION_STRING_PADDED_LENGTH);
  strcat(reg_str, ",");
  strcat(reg_str, "_");
  strcat(reg_str, scope);

  RDMNET_MSVC_END_NO_DEP_WARNINGS()
}

bool broker_info_is_valid(const RdmnetBrokerDiscInfo *info)
{
  // Make sure none of the broker info's fields are empty
  return !(info->cid.data == 0 || strlen(info->service_name) == 0 || strlen(info->scope) == 0 ||
           strlen(info->model) == 0 || strlen(info->manufacturer) == 0);
}

/* 0000:0000:0000:0000:0000:0000:0000:0001 is a loopback address
 * 0000:0000:0000:0000:0000:0000:0000:0000 is not valid */
bool ipv6_valid(LwpaIpAddr *ip)
{
  return (!lwpa_ip_is_loopback(ip) && !lwpa_ip_is_wildcard(ip));
}

RdmnetScopeMonitorRef *scope_monitor_new(const RdmnetScopeMonitorConfig *config)
{
  RdmnetScopeMonitorRef *new_monitor = (RdmnetScopeMonitorRef*)malloc(sizeof(RdmnetScopeMonitorRef));
  if (new_monitor)
  {
    new_monitor->config = *config;
    new_monitor->broker_handle = NULL;
    new_monitor->broker_list = NULL;
    new_monitor->next = NULL;
  }
  return new_monitor;
}

void scope_monitor_delete(RdmnetScopeMonitorRef *ref)
{
  DiscoveredBroker *db = ref->broker_list;
  DiscoveredBroker *next_db;
  while (db)
  {
    next_db = db->next;
    discovered_broker_delete(db);
    db = next_db;
  }
  lwpa_poll_remove_socket(&disc_state.poll_context, DNSServiceRefSockFD(ref->dnssd_ref));
  DNSServiceRefDeallocate(ref->dnssd_ref);
  free(ref);
}

DiscoveredBroker *discovered_broker_new(const char *service_name, const char *full_service_name)
{
  DiscoveredBroker *new_db = (DiscoveredBroker*)malloc(sizeof(DiscoveredBroker));
  if (new_db)
  {
    rdmnetdisc_fill_default_broker_info(&new_db->info);
    rdmnet_safe_strncpy(new_db->info.service_name, service_name, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(new_db->full_service_name, full_service_name, kDNSServiceMaxDomainName);
    new_db->state = kResolveStateServiceResolve;
    new_db->dnssd_ref = NULL;
    new_db->next = NULL;
  }
  return new_db;
}

void discovered_broker_delete(DiscoveredBroker *db)
{
  if (db->state != kResolveStateDone)
  {
    lwpa_poll_remove_socket(&disc_state.poll_context, DNSServiceRefSockFD(db->dnssd_ref));
    DNSServiceRefDeallocate(db->dnssd_ref);
  }
  BrokerListenAddr *listen_addr = db->info.listen_addr_list;
  while (listen_addr)
  {
    BrokerListenAddr *next_listen_addr = listen_addr->next;
    free(listen_addr);
    listen_addr = next_listen_addr;
  }
  free(db);
}

RdmnetBrokerRegisterRef *registered_broker_new(const RdmnetBrokerRegisterConfig *config)
{
  RdmnetBrokerRegisterRef *new_rb = (RdmnetBrokerRegisterRef*)malloc(sizeof(RdmnetBrokerRegisterRef));
  if (new_rb)
  {
    new_rb->config = *config;
    new_rb->scope_monitor_handle = NULL;
    new_rb->state = kBrokerStateNotRegistered;
    new_rb->full_service_name[0] = '\0';
    new_rb->dnssd_ref = NULL;
  }
  return new_rb;
}

void registered_broker_delete(RdmnetBrokerRegisterRef *rb)
{
  free(rb);
}

/* Adds broker discovery information into brokers.
 * Assumes a lock is already taken.*/
void discovered_broker_insert(DiscoveredBroker **list_head_ptr, DiscoveredBroker *new)
{
  if (*list_head_ptr)
  {
    DiscoveredBroker *cur = *list_head_ptr;
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
DiscoveredBroker *discovered_broker_lookup_by_name(DiscoveredBroker *list_head, const char *full_name)
{
  for (DiscoveredBroker *current = list_head; current; current = current->next)
  {
    if (strcmp(current->full_service_name, full_name) == 0)
    {
      return current;
    }
  }
  return NULL;
}

/* Searches for a DiscoveredBroker instance by associated DNSServiceRef in a list.
 * Returns the found instance or NULL if no match was found.
 * Assumes a lock is already taken.
 */
DiscoveredBroker *discovered_broker_lookup_by_ref(DiscoveredBroker *list_head, DNSServiceRef dnssd_ref)
{
  for (DiscoveredBroker *current = list_head; current; current = current->next)
  {
    if (current->dnssd_ref == dnssd_ref)
    {
      return current;
    }
  }
  return NULL;
}

/* Removes a DiscoveredBroker instance from a list.
 * Assumes a lock is already taken.*/
void discovered_broker_remove(DiscoveredBroker **list_head_ptr, const DiscoveredBroker *db)
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
    DiscoveredBroker *prev_db = *list_head_ptr;
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
void scope_monitor_insert(RdmnetScopeMonitorRef *scope_ref)
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
      RdmnetScopeMonitorRef *ref = disc_state.scope_ref_list;
      for (; ref->next; ref = ref->next)
        ;
      ref->next = scope_ref;
    }
  }
}

/* Searches to see if a scope is being monitored.
 * Returns NULL if no match was found.
 * Assumes a lock is already taken. */
RdmnetScopeMonitorRef *scope_monitor_lookup(DNSServiceRef dnssd_ref)
{
  for (RdmnetScopeMonitorRef *ref = disc_state.scope_ref_list; ref; ref = ref->next)
  {
    if (ref->dnssd_ref == dnssd_ref)
    {
      return ref;
    }
  }
  return NULL;
}

/* Removes an entry from disc_state.scope_ref_list.
 * Assumes a lock is already taken. */
void scope_monitor_remove(const RdmnetScopeMonitorRef *ref)
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
    RdmnetScopeMonitorRef *prev_ref = disc_state.scope_ref_list;
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
