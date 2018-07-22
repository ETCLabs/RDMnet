/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 63. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
* Copyright 2018 ETC Inc.
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

/* Suppress strncpy() warning on Windows/MSVC. */
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "rdmnet/discovery.h"
#include "rdmnetdiscovery_bonjour.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "lwpa_inet.h"
#include "lwpa_bool.h"
#include "lwpa_pack.h"

typedef struct DiscoveryState
{
  lwpa_rwlock_t lock;
  DNSServiceRef ref_list[ARRAY_SIZE_DEFAULT];

  char registered_fullname[kDNSServiceMaxDomainName];

  /*queries we're waiting to complete*/
  Operations queries;
  /*DNSResolves we're waiting to complete*/
  Operations resolves;
  /*IPAddress resolves we're waiting to complete*/
  Operations addrs;

  BrokersBeingDiscovered brokers;

  ScopesMonitored scopes;
  RdmnetDiscCallbacks callbacks;
  BrokerDiscInfo info_to_register;

  enum BROKER_REGISTRATION_STATE broker_reg_state;
  void *broker_reg_context;

  /*The handle used for lwpa_poll()*/
  lwpa_socket_t dns_reg_handle;
  /*The handle that Bonjour will pass back for service registrations*/
  DNSServiceRef dns_reg_ref;
} DiscoveryState;

static DiscoveryState disc_state;

/******************************************************************************
 * find/insert/delete functions for structs and arrays
 *****************************************************************************/

/*Searches for an entry in the passed operations 'map' struct.
 *Returns false if it didn't contain that ref.
 *value at *ret is set as the index of the value, if found.
 *value at *op_data is set to the op_data inside the found value
 *Assumes a lock is already taken*/
bool operation_lookup(const Operations *map, DNSServiceRef ref, OperationData *op_data, unsigned int *ret)
{
  for (unsigned int i = 0; i < map->count; i++)
    if (map->refs[i] == ref)
    {
      if (op_data != NULL)
        *op_data = map->op_data[i];
      if (ret != NULL)
        *ret = i;
      return true;
    }
  return false;
}

/* TODO: check if there is a better way of discarding entries over array size*/
/*Adds a ref into the passed operations struct.
 *Returns false if a socket handle couldn't be created for the ref.
 *Assumes a lock is already taken.*/
bool operation_insert(Operations *map, DNSServiceRef ref, DNSServiceRef search_ref, char *full_name)
{
  bool result = true;

  lwpa_socket_t handle;
  handle = (DNSServiceRefSockFD(ref));
  if (handle != -1 && !operation_lookup(map, ref, NULL, NULL))
  {
    if (map->count >= ARRAY_SIZE_DEFAULT)
    {
      result = false;
    }
    else
    {
      OperationData data;
      strncpy(data.full_name, full_name, kDNSServiceMaxDomainName);
      data.search_ref = search_ref;
      data.socket = handle;

      map->refs[map->count] = ref;
      map->op_data[map->count] = data;
      map->count++;
    }
  }
  else
  {
    result = false;
  }

  return result;
}

/*Removes an entry from the passed operations struct.
 *Returns false if no matching entry is found.
 *Assumes a lock is already taken.*/
bool operation_lookup_erase(Operations *map, DNSServiceRef ref, OperationData *data)
{
  unsigned int index;
  bool result = operation_lookup(map, ref, data, &index);
  if (result)
  {
    if (index < map->count - 1)
    {
      /*shift elements*/
      for (unsigned int i = index; i < map->count - 1; i++)
      {
        map->op_data[i] = map->op_data[i + 1];
        map->refs[i] = map->refs[i + 1];
      }
    }
    map->count--;
  }

  return result;
}

/* TODO: check if there is a better way of discarding entries over array size*/
/*Adds broker discovery information into brokers.
 *Assumes a lock is already taken.*/
void broker_insert(const char *full_name, const BrokerDiscInfo *broker_info)
{
  if (disc_state.brokers.count < ARRAY_SIZE_DEFAULT)
  {
    strncpy(disc_state.brokers.fullnames[disc_state.brokers.count], full_name, kDNSServiceMaxDomainName);
    disc_state.brokers.info[disc_state.brokers.count] = *broker_info;
    disc_state.brokers.count++;
  }
}

/*Searches for broker discovery information.
 *Returns false if no match was found.
 *value at *ret is set as the index of the value, if found.
 *Assumes a lock is already taken.*/
bool broker_lookup(const char *full_name, unsigned int *ret)
{
  for (unsigned int i = 0; i < disc_state.brokers.count; i++)
    if (strcmp(disc_state.brokers.fullnames[i], full_name) == 0)
    {
      if (ret != NULL)
        *ret = i;
      return true;
    }
  return false;
}

/*Removes an entry from brokers.
 *Returns false if no matching entry is found.
 *Assumes a lock is already taken.*/
void broker_erase(const char *full_name)
{
  unsigned int index;
  bool result = broker_lookup(full_name, &index);
  if (result)
  {
    if (index < disc_state.brokers.count - 1)
    {
      /*shift elements*/
      for (unsigned int i = index; i < disc_state.brokers.count - 1; i++)
      {
        disc_state.brokers.info[i] = disc_state.brokers.info[i + 1];
        strncpy(disc_state.brokers.fullnames[i], disc_state.brokers.fullnames[i + 1], kDNSServiceMaxDomainName);
      }
    }

    disc_state.brokers.count--;
  }
}

/* TODO: check if there is a better way of discarding entries over array size*/
/*Adds new scope info into scopes_monitered.
 *Assumes a lock is already taken.*/
void scope_monitored_insert(DNSServiceRef ref, const ScopeMonitorInfo *monitor_info)
{
  if (disc_state.scopes.count < ARRAY_SIZE_DEFAULT)
  {
    disc_state.scopes.refs[disc_state.scopes.count] = ref;
    disc_state.scopes.monitor_info[disc_state.scopes.count] = *monitor_info;
    disc_state.scopes.count++;
  }
}

/*Searches to see if a scope is being monitored.
 *Returns false if no match was found.
 *value at *ret is set as the index of the value, if found.
 *Assumes a lock is already taken.*/
bool scope_monitored_lookup(DNSServiceRef ref, int *ret)
{
  for (unsigned int i = 0; i < disc_state.scopes.count; i++)
    if (disc_state.scopes.refs[i] == ref)
    {
      *ret = i;
      return true;
    }
  return false;
}

/*Removes an entry from disc_state.scopes.
 *Returns false if no matching entry is found.
 *Assumes a lock is already taken.*/
void scope_monitored_erase(unsigned int index)
{
  if (index < disc_state.scopes.count)
  {
    if (index < disc_state.scopes.count - 1)
    {
      /*shift elements*/
      for (unsigned int i = index; i < disc_state.scopes.count - 1; i++)
      {
        disc_state.scopes.monitor_info[i] = disc_state.scopes.monitor_info[i + 1];
        disc_state.scopes.refs[i] = disc_state.scopes.refs[i + 1];
      }
    }

    disc_state.scopes.count--;
  }
}

bool push_query_operations(const Operations *map, DNSServiceRef *current_refs, LwpaPollfd *fds, size_t *nfds)
{
  bool success = true;

  for (unsigned int i = 0; i < map->count; i++)
  {
    /* TODO: something more dynamic with extra entries
     *discard any entry over array size for now*/
    if (*nfds >= LWPA_SOCKET_MAX_POLL_SIZE)
    {
      success = false;
      break;
    }
    current_refs[*nfds] = map->refs[i];
    fds[*nfds].fd = map->op_data[i].socket;
    fds[*nfds].events |= LWPA_POLLIN;
    (*nfds)++;
  }

  return success;
}

/******************************************************************************
 * helper functions
 ******************************************************************************/

void get_registration_string(const char *srv_type, const char *scope, char *reg_str)
{
  /*Bonjour adds in the _sub. for us.*/
  strncpy(reg_str, srv_type, REGISTRATION_STRING_PADDED_LENGTH);
  strcat(reg_str, ",");
  strcat(reg_str, "_");
  strcat(reg_str, scope);
}

bool broker_info_is_valid(const BrokerDiscInfo *info)
{
  /*make sure none of the broker info's fields are empty*/
  return !(info->cid.data == 0 || strlen(info->service_name) == 0 || strlen(info->scope) == 0 ||
           strlen(info->model) == 0 || strlen(info->manufacturer) == 0);
}

bool create_socket_handle_from_raw_int(int raw_sock, lwpa_socket_t *new_handle)
{
  *new_handle = (lwpa_socket_t)raw_sock;
  return true;
}

/* dest char* should have a size of kDNSServiceMaxDomainName*/
void get_bonjour_fullname(const char *service, const char *regtype, const char *domain, char *dest)
{
  if (DNSServiceConstructFullName(dest, service, regtype, domain) != kDNSServiceErr_NoError)
    dest[0] = 0;
}

bool ref_to_scope_internal(DNSServiceRef ref, ScopeMonitorInfo *info)
{
  int index;
  if (scope_monitored_lookup(ref, &index))
  {
    *info = disc_state.scopes.monitor_info[index];
    return true;
  }

  return false;
}

/*If the ref isn't found, returns 0.*/
/*Ref can be a scope monitor ref, or an outstanding operation ref.*/
bool ref_to_scope(DNSServiceRef ref, ScopeMonitorInfo *info)
{
  bool found = false;

  if ((lwpa_rwlock_readlock(&disc_state.lock, LWPA_WAIT_FOREVER)))
  {
    /*Used for resolving other operation maps*/
    OperationData op_data = {0};

    found = ref_to_scope_internal(ref, info);
    if (!found && operation_lookup(&disc_state.queries, ref, &op_data, NULL))
      found = ref_to_scope_internal(op_data.search_ref, info);
    if (!found && operation_lookup(&disc_state.resolves, ref, &op_data, NULL))
      found = ref_to_scope_internal(op_data.search_ref, info);
    if (!found && operation_lookup(&disc_state.addrs, ref, &op_data, NULL))
      found = ref_to_scope_internal(op_data.search_ref, info);

    lwpa_rwlock_readunlock(&disc_state.lock);
  }

  return found;
}

void notify_monitor_error(DNSServiceRef ref, int error, void *context)
{
  ScopeMonitorInfo info;
  if (ref_to_scope(ref, &info) && disc_state.callbacks.scope_monitor_error != NULL)
    disc_state.callbacks.scope_monitor_error(&info, error, context);
}

/*Finshes/Cancels the operation associated with a service ref, and removes it
 * from the appropriate map.*/
/*If this returns true, fills in the operation data associated with the entry,
 * or false if not found.*/
bool finish_operation(DNSServiceRef ref, bool cancel_ref, bool remove_broker_info, OperationData *data)
{
  bool data_init = false;

  if (ref != NULL)
  {
    if (lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
    {
      data_init = operation_lookup_erase(&disc_state.queries, ref, data);
      if (!data_init)
        data_init = operation_lookup_erase(&disc_state.resolves, ref, data);
      if (!data_init)
        data_init = operation_lookup_erase(&disc_state.addrs, ref, data);

      if (data_init && remove_broker_info && data->full_name[0] != 0)
        broker_erase(data->full_name);

      lwpa_rwlock_writeunlock(&disc_state.lock);
    }

    if (cancel_ref)
      DNSServiceRefDeallocate(ref);
  }

  return data_init;
}

/*The version of FinishOperation that doesn't care about the operation data.
 * Always cancels the ref & removes the broker info.*/
void cancel_operation(DNSServiceRef ref)
{
  OperationData ignored;
  finish_operation(ref, true, true, &ignored);
}

/* 0000:0000:0000:0000:0000:0000:0000:0001 is a loopback address
 * 0000:0000:0000:0000:0000:0000:0000:0000 is not vaid*/
bool ipv6_valid(LwpaIpAddr *ip)
{
  return !(lwpaip_v6_address(ip)[0] == 0 && lwpaip_v6_address(ip)[1] == 0 && lwpaip_v6_address(ip)[2] == 0 &&
           lwpaip_v6_address(ip)[3] == 0 && lwpaip_v6_address(ip)[4] == 0 && lwpaip_v6_address(ip)[5] == 0 &&
           lwpaip_v6_address(ip)[6] == 0 && (lwpaip_v6_address(ip)[7] == 0 || lwpaip_v6_address(ip)[7] == 1));
}

/******************************************************************************
 * DNS-SD / Bonjour functions
 ******************************************************************************/

void DNSSD_API process_DNSServiceRegisterReply(DNSServiceRef sdRef, DNSServiceFlags flags,
                                               DNSServiceErrorType errorCode, const char *name, const char *regtype,
                                               const char *domain, void *context)
{
  if (sdRef == disc_state.dns_reg_ref)
  {
    if (flags & kDNSServiceFlagsAdd)
    {
      if (disc_state.callbacks.broker_registered != NULL)
      {
        disc_state.callbacks.broker_registered(&disc_state.info_to_register, name, context);
      }
      get_bonjour_fullname(name, regtype, domain, disc_state.registered_fullname);
    }
    else
    {
      if (disc_state.callbacks.broker_register_error != NULL)
      {
        disc_state.callbacks.broker_register_error(&disc_state.info_to_register, errorCode, context);
      }
    }
  }
}

void DNSSD_API process_DNSServiceGetAddrInfoReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                                  DNSServiceErrorType errorCode, const char *hostname,
                                                  const struct sockaddr *address, uint32_t ttl, void *context)
{
  (void)interfaceIndex;
  (void)hostname;
  (void)ttl;

  if (errorCode != 0)
  {
    notify_monitor_error(sdRef, errorCode, context);
    cancel_operation(sdRef);
  }
  else
  {
    /*We got a response, but we'll only clean up at the end if the flags tell
     * us we're done getting addrs.*/
    bool addrs_done = !(flags & kDNSServiceFlagsMoreComing);
    /*Only copied to if addrs_done is true;*/
    BrokerDiscInfo notify_info = {0};

    if (lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
    {
      OperationData op_data = {0};
      if (operation_lookup(&disc_state.addrs, sdRef, &op_data, NULL))
      {
        /*Update the broker info we're building*/
        unsigned int index;
        if (broker_lookup(op_data.full_name, &index))
        {
          BrokerDiscInfo *info = &disc_state.brokers.info[index];
          LwpaSockaddr ip_addr = {0};

          sockaddr_plat_to_lwpa(&ip_addr, address);

          if ((lwpaip_is_v4(&ip_addr.ip) && lwpaip_v4_address(&ip_addr.ip) != 0) ||
              (lwpaip_is_v6(&ip_addr.ip) && ipv6_valid(&ip_addr.ip)))
          {
            /*normalize the address and add it to the info.*/
            if ((info->port != 0) && (ip_addr.port == 0))
              ip_addr.port = info->port;
            info->listen_addrs[info->listen_addrs_count++] = ip_addr;
          }

          if (addrs_done)
            notify_info = *info;
        }
      }

      lwpa_rwlock_writeunlock(&disc_state.lock);
    }

    /*No more addresses, clean up.*/
    if (addrs_done)
    {
      cancel_operation(sdRef);
      if (notify_info.listen_addrs_count != 0 && disc_state.callbacks.broker_found != NULL)
      {
        disc_state.callbacks.broker_found(notify_info.scope, &notify_info, context);
      }
    }
  }
}

void DNSSD_API process_DNSServiceResolveReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                              DNSServiceErrorType errorCode, const char *fullname,
                                              const char *hosttarget, uint16_t port /* In network byte order */,
                                              uint16_t txtLen, const unsigned char *txtRecord, void *context)
{
  (void)interfaceIndex;
  (void)flags;

  if (errorCode != 0)
  {
    notify_monitor_error(sdRef, errorCode, context);
    cancel_operation(sdRef);
  }
  else
  {
    /*We got a response, clean up.  We don't need to keep resolving.*/
    OperationData op_data = {0};
    bool op_result = finish_operation(sdRef, true, false, &op_data);

    /*In case we have an error, this will != 0*/
    DNSServiceErrorType monitor_error = 0;
    DNSServiceRef error_search_ref = {0};

    /*We have to take the lock before the DNSServiceGetAddrInfo call, because
     * we need to add the ref to our map before it responds.*/
    if (op_result && lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
    {
      DNSServiceRef addr_ref = {0};
      monitor_error =
          DNSServiceGetAddrInfo(&addr_ref, 0, 0, 0, hosttarget, &process_DNSServiceGetAddrInfoReply, context);
      if (monitor_error != kDNSServiceErr_NoError)
      {
        error_search_ref = op_data.search_ref;
      }
      else
      {
        /*Update the broker info.*/
        unsigned int info_index;
        if (broker_lookup(fullname, &info_index))
        {
          BrokerDiscInfo *info = &disc_state.brokers.info[info_index];
          info->port = upack_16b(&port);

          uint8_t value_len = 0;
          const char *value;
          // char sval[16];

          // value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord,
          //                                            "TxtVers", &value_len));
          // if (value && value_len)
          //{
          //  strcpy(sval, value);
          //  info->txt_vers = atoi(sval);
          //}

          value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "ConfScope", &value_len));
          if (value && value_len)
            strncpy(info->scope, value, value_len);

          // value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord,
          //                                            "E133Vers", &value_len));
          // if (value && value_len)
          //{
          //  strcpy(sval, value);
          //  info->e133_vers = atoi(sval);
          //}

          value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "CID", &value_len));
          if (value && value_len)
            string_to_cid(&info->cid, value, value_len);

          value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "Model", &value_len));
          if (value && value_len)
            strncpy(info->model, value, value_len);

          value = (const char *)(TXTRecordGetValuePtr(txtLen, txtRecord, "Manuf", &value_len));
          if (value && value_len)
            strncpy(info->manufacturer, value, value_len);
        }

        if (!operation_insert(&disc_state.addrs, addr_ref, op_data.search_ref, op_data.full_name))
        {
          error_search_ref = op_data.search_ref;
          monitor_error = kDNSServiceErr_Unknown;
        }
      }

      lwpa_rwlock_writeunlock(&disc_state.lock);
    }

    if (monitor_error != 0)
      notify_monitor_error(sdRef, monitor_error, context);
  }
}

void DNSSD_API process_DNSServiceBrowseReply(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex,
                                             DNSServiceErrorType errorCode, const char *serviceName,
                                             const char *regtype, const char *replyDomain, void *context)
{
  /*We're not calling FinishOperation here, because we want the monitoring
   * process on that ref to continue!*/

  /*Filter out the service name if it matches our broker instance name.*/
  char full_name[kDNSServiceMaxDomainName] = {0};
  get_bonjour_fullname(serviceName, regtype, replyDomain, full_name);
  if (strcmp(full_name, disc_state.registered_fullname) == 0)
    return;

  if (errorCode != 0)
  {
    notify_monitor_error(sdRef, errorCode, context);
  }
  else if (flags & kDNSServiceFlagsAdd)
  {
    /*Start the next part of the resolution*/
    BrokerDiscInfo info = {0};
    strncpy(info.service_name, serviceName, E133_SERVICE_NAME_STRING_PADDED_LENGTH);

    /*In case we have an error, this will != 0*/
    int monitor_error = 0;

    /*We have to take the lock before the DNSServiceResolve call, because we
     * need to add the ref to our map before it responds.*/

    if (lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
    {
      DNSServiceRef resolve_ref = {0};
      monitor_error = DNSServiceResolve(&resolve_ref, 0, interfaceIndex, serviceName, regtype, replyDomain,
                                        &process_DNSServiceResolveReply, context);
      if ((monitor_error == 0) && !broker_lookup(full_name, NULL))
        broker_insert(full_name, &info);

      /*Set up the operation map*/
      OperationData op_data = {0};
      if (!operation_lookup(&disc_state.queries, sdRef, &op_data, NULL) ||
          !operation_insert(&disc_state.resolves, resolve_ref, op_data.search_ref, full_name))
      {
        monitor_error = kDNSServiceErr_Unknown;
      }

      lwpa_rwlock_writeunlock(&disc_state.lock);
    }

    if (monitor_error != kDNSServiceErr_NoError)
      notify_monitor_error(sdRef, monitor_error, context);
  }
  else
  {
    /*Service removal*/
    if (disc_state.callbacks.broker_lost != NULL)
      disc_state.callbacks.broker_lost(serviceName, context);
  }
}

/*Call this to start monitoring mdns for brokers at the supplied scope.
 *If true is returned, you will asynchronously receive BrokerFound
 *notifications until you call StopMonitoringScope or Shutdown. If false is
 *returned, the platform_specific_error is filled in.  You may also receive a
 *ScopeMonitorError notifications.*/
DNSServiceErrorType send_query(const char *srv_type, const char *scope, const char *domain, DNSServiceRef *ref,
                               void *context)
{
  DNSServiceErrorType result = kDNSServiceErr_Unknown;
  char reg_str[REGISTRATION_STRING_PADDED_LENGTH];
  get_registration_string(srv_type, scope, reg_str);

  /*We have to take the lock before the DNSServiceBrowse call, because we need
   * to add the ref to our map before it responds.*/
  if (lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
  {
    result = DNSServiceBrowse(ref, 0, 0, reg_str, domain, &process_DNSServiceBrowseReply, context);

    /*For high level browse refs are the same and don't have full name yet.*/
    if (result == kDNSServiceErr_NoError)
      result = operation_insert(&disc_state.queries, *ref, *ref, "") ? 0 : 1;

    lwpa_rwlock_writeunlock(&disc_state.lock);
  }

  return result;
}

/******************************************************************************
 * public functions
 ******************************************************************************/

lwpa_error_t rdmnetdisc_init(RdmnetDiscCallbacks *callbacks)
{
  disc_state.callbacks = *callbacks;
  disc_state.broker_reg_state = kBrokerNotRegistered;
  lwpa_rwlock_create(&disc_state.lock);

  return LWPA_OK;
}

void rdmnetdisc_deinit()
{
  rdmnetdisc_stopmonitoring_all_scopes();
}

void fill_default_scope_info(ScopeMonitorInfo *scope_info)
{
  strncpy(scope_info->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
  strncpy(scope_info->domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
}

void fill_default_broker_info(BrokerDiscInfo *broker_info)
{
  memset(broker_info->service_name, 0, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  broker_info->port = 0;
  memset(broker_info->listen_addrs, 0, sizeof(LwpaSockaddr) * ARRAY_SIZE_DEFAULT);
  broker_info->listen_addrs_count = 0;
  strncpy(broker_info->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
  memset(broker_info->model, 0, E133_MODEL_STRING_PADDED_LENGTH);
  memset(broker_info->manufacturer, 0, E133_MANUFACTURER_STRING_PADDED_LENGTH);
}

lwpa_error_t rdmnetdisc_startmonitoring(const ScopeMonitorInfo *scope_info, int *platform_specific_error, void *context)
{
  (void)context;

  DNSServiceRef ref;
  *platform_specific_error = send_query(E133_DNSSD_SRV_TYPE, scope_info->scope, scope_info->domain, &ref, context);

  if (*platform_specific_error == 0)
  {
    if (lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
    {
      scope_monitored_insert(ref, scope_info);

      lwpa_rwlock_writeunlock(&disc_state.lock);
    }
  }

  /* TODO: should have some kind of handling or reporting for errors from send_query */

  return LWPA_OK;
}

void rdmnetdisc_stopmonitoring(const ScopeMonitorInfo *scope_info)
{
  DNSServiceRef ref = NULL;
  if (lwpa_rwlock_writelock(&disc_state.lock, LWPA_WAIT_FOREVER))
  {
    for (unsigned int i = 0; i < disc_state.scopes.count; i++)
    {
      if (0 == strncmp(disc_state.scopes.monitor_info[i].scope, scope_info->scope, E133_SCOPE_STRING_PADDED_LENGTH))
      {
        ref = disc_state.scopes.refs[i];
        scope_monitored_erase(i);
        break;
      }
    }

    lwpa_rwlock_writeunlock(&disc_state.lock);
  }

  if (ref)
    cancel_operation(ref);
}

void rdmnetdisc_stopmonitoring_all_scopes()
{
  for (unsigned int i = 0; i < disc_state.scopes.count; i++)
    cancel_operation(disc_state.scopes.refs[i]);

  memset(disc_state.scopes.refs, 0, sizeof(disc_state.scopes.refs));
  memset(disc_state.scopes.monitor_info, 0, sizeof(disc_state.scopes.monitor_info));
  disc_state.scopes.count = 0;
}

lwpa_error_t rdmnetdisc_registerbroker(const BrokerDiscInfo *broker_info, void *context)
{
  if (disc_state.broker_reg_state != kBrokerNotRegistered || disc_state.dns_reg_ref != NULL ||
      !broker_info_is_valid(broker_info))
  {
    return false;
  }

  disc_state.info_to_register = *broker_info;
  disc_state.broker_reg_state = kBrokerInfoSet;
  disc_state.broker_reg_context = context;

  return LWPA_OK;
}

void rdmnetdisc_unregisterbroker()
{
  if (disc_state.broker_reg_state != kBrokerNotRegistered)
  {
    if (disc_state.dns_reg_ref)
    {
      DNSServiceRefDeallocate(disc_state.dns_reg_ref);
      disc_state.dns_reg_ref = NULL;
    }

    /*Since the broker only cares about scopes while it is running, shut down
     * any outstanding queries for that scope.*/
    ScopeMonitorInfo info = {0};
    strncpy(info.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
    strncpy(info.scope, disc_state.info_to_register.scope, E133_SCOPE_STRING_PADDED_LENGTH);
    rdmnetdisc_stopmonitoring(&info);

    /*Reset the state*/
    disc_state.broker_reg_state = kBrokerNotRegistered;
  }
}

/* TODO: magic number int_conversion
 */
/*If returns !0, this was an error from Bonjour.  Reset the state and notify
 * the callback.*/
DNSServiceErrorType send_registration(const BrokerDiscInfo *info, void *context)
{
  DNSServiceErrorType result = kDNSServiceErr_NoError;

  /*Before we start the registration, we have to massage a few parameters*/
  uint16_t net_port = 0;
  pack_16b(&net_port, info->port);

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
    char cid_str[CID_STRING_BYTES];
    cid_to_string(cid_str, &info->cid);
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
    /*TODO: If we want to register a device on a particular interface instead
     * of all interfaces, we'll have to have multiple reg_refs and do a
     * DNSServiceRegister on each interface. Not ideal.*/
    result = DNSServiceRegister(&disc_state.dns_reg_ref, 0, 0, info->service_name, reg_str, NULL, NULL, net_port,
                                TXTRecordGetLength(&txt), TXTRecordGetBytesPtr(&txt), process_DNSServiceRegisterReply,
                                context);

    if (result == kDNSServiceErr_NoError)
    {
      result =
          create_socket_handle_from_raw_int(DNSServiceRefSockFD(disc_state.dns_reg_ref), &disc_state.dns_reg_handle)
              ? 0
              : 1;
    }
  }

  TXTRecordDeallocate(&txt);

  return result;
}

void rdmnetdisc_tick()
{
  switch (disc_state.broker_reg_state)
  {
    case kBrokerInfoSet:
    {
      /*The info was set.  Start the registration and monitoring*/
      disc_state.broker_reg_state = kBrokeRegisterStarted;

      DNSServiceErrorType reg_result = send_registration(&disc_state.info_to_register, disc_state.broker_reg_context);

      if (reg_result != kDNSServiceErr_NoError)
      {
        disc_state.broker_reg_state = kBrokerNotRegistered;
        if (disc_state.callbacks.broker_register_error != NULL)
        {
          disc_state.callbacks.broker_register_error(&disc_state.info_to_register, reg_result, disc_state.broker_reg_context);
        }
      }

      int mon_error = 0;
      ScopeMonitorInfo info;
      strncpy(info.scope, disc_state.info_to_register.scope, E133_SCOPE_STRING_PADDED_LENGTH);
      strncpy(info.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
      if (rdmnetdisc_startmonitoring(&info, &mon_error, disc_state.broker_reg_context) != LWPA_OK)
      {
        if (disc_state.callbacks.scope_monitor_error != NULL)
          disc_state.callbacks.scope_monitor_error(&info, mon_error, disc_state.broker_reg_context);
      }
    }
    break;

    case kBrokerNotRegistered:
    case kBrokeRegisterStarted:
    case kBrokerRegistered:
      /*Nothing to do here right now.*/
      break;
  }

  DNSServiceRef current_refs[ARRAY_SIZE_DEFAULT] = {0};

  static LwpaPollfd fds[LWPA_SOCKET_MAX_POLL_SIZE] = {0};
  size_t nfds = 0;

  if (disc_state.dns_reg_ref)
  {
    /*If the ref was created, the socket handle is initialized, too.*/
    current_refs[nfds] = disc_state.dns_reg_ref;

    fds[nfds].fd = disc_state.dns_reg_handle;
    fds[nfds].events |= LWPA_POLLIN;
    nfds++;
  }

  if (lwpa_rwlock_readlock(&disc_state.lock, LWPA_WAIT_FOREVER))
  {
    push_query_operations(&disc_state.queries, current_refs, fds, &nfds);
    push_query_operations(&disc_state.resolves, current_refs, fds, &nfds);
    push_query_operations(&disc_state.addrs, current_refs, fds, &nfds);

    lwpa_rwlock_readunlock(&disc_state.lock);
  }

  if (nfds > 0 && lwpa_poll(fds, nfds, 200) > 0)
  {
    for (unsigned int i = 0; i < nfds; i++)
    {
      if (!(fds[i].revents & LWPA_POLLIN))
        continue;

      DNSServiceErrorType process_error;
      process_error = DNSServiceProcessResult(current_refs[i]);

      if (process_error != kDNSServiceErr_NoError)
      {
        /*For now, do nothing and keep processing.  We may want to kill the socket later.*/
      }
    }
  }
}
