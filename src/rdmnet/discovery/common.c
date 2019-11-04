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

#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"

/***************************** Global variables ******************************/

etcpal_mutex_t rdmnetdisc_lock;

/**************************** Private constants ******************************/

#define MAX_SCOPES_MONITORED ((RDMNET_MAX_SCOPES_PER_CONTROLLER * RDMNET_MAX_CONTROLLERS) + RDMNET_MAX_DEVICES)

/**************************** Private variables ******************************/

static RdmnetScopeMonitorRef* scope_ref_list;
static RdmnetBrokerRegisterRef* broker_ref_list;

/*********************** Private function prototypes *************************/

static RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config);
static void scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref);
static void scope_monitor_remove(const RdmnetScopeMonitorRef* ref);
static void scope_monitor_delete(RdmnetScopeMonitorRef* ref);

static RdmnetBrokerRegisterRef* registered_broker_new(const RdmnetBrokerRegisterConfig* config);
static void registered_broker_delete(RdmnetBrokerRegisterRef* rb);

static void stop_monitoring_all_internal();

// Other helpers
static bool broker_info_is_valid(const RdmnetBrokerDiscInfo* info);

/*************************** Function definitions ****************************/

/* Internal function to initialize the RDMnet discovery API.
 * Returns kEtcPalErrOk on success, or specific error code on failure.
 */
etcpal_error_t rdmnetdisc_init()
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!etcpal_mutex_create(&rdmnetdisc_lock))
    res = kEtcPalErrSys;

  if (res == kEtcPalErrOk)
    res = rdmnetdisc_platform_init();

  if (res != kEtcPalErrOk)
    etcpal_mutex_destroy(&rdmnetdisc_lock);

  return res;
}

/* Internal function to deinitialize the RDMnet discovery API. */
void rdmnetdisc_deinit()
{
  stop_monitoring_all_internal();
  etcpal_mutex_destroy(&rdmnetdisc_lock);
}

/*!
 * \brief Initialize an RdmnetBrokerDiscInfo structure with null settings.
 *
 * Note that this does not produce a valid RdmnetBrokerDiscInfo for broker registration - you will
 * need to manipulate the fields with your own broker information before passing it to
 * rdmnetdisc_register_broker().
 *
 * \param[out] broker_info Info struct to nullify.
 */
void rdmnetdisc_init_broker_info(RdmnetBrokerDiscInfo* broker_info)
{
  broker_info->cid = kEtcPalNullUuid;
  memset(broker_info->service_name, 0, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  broker_info->port = 0;
  broker_info->listen_addr_list = NULL;
  rdmnet_safe_strncpy(broker_info->scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
  memset(broker_info->model, 0, E133_MODEL_STRING_PADDED_LENGTH);
  memset(broker_info->manufacturer, 0, E133_MANUFACTURER_STRING_PADDED_LENGTH);
}

/*!
 * \brief Begin monitoring an RDMnet scope for brokers.
 *
 * Expect to receive callbacks from the RDMnet tick thread when brokers are found and lost.
 * Asynchronous monitoring errors can also be delivered via the scope_monitor_error callback.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * \param[in] config Configuration struct with details about the scope to monitor.
 * \param[out] handle Filled in on success with a handle that can be used to access the monitored
 *                    scope later.
 * \param[out] platform_specific_error Filled in on failure with a platform-specific error code.
 * \return #kEtcPalErrOk: Monitoring started successfully.
 * \return #kEtcPalInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: Couldn't allocate resources to monitor this scope.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnetdisc_start_monitoring(const RdmnetScopeMonitorConfig* config, rdmnet_scope_monitor_t* handle,
                                           int* platform_specific_error)
{
  if (!config || !handle || !platform_specific_error)
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrOk;
  if (RDMNET_DISC_LOCK())
  {
    RdmnetScopeMonitorRef* new_monitor = scope_monitor_new(config);
    if (!new_monitor)
      res = kEtcPalErrNoMem;

    res = rdmnetdisc_platform_start_monitoring(config, new_monitor, platform_specific_error);
    if (res == kEtcPalErrOk)
    {
      scope_monitor_insert(new_monitor);
      *handle = new_monitor;
    }
    else
    {
      scope_monitor_delete(new_monitor);
    }

    RDMNET_DISC_UNLOCK();
  }
  else
  {
    res = kEtcPalErrSys;
  }

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

/*!
 * \brief Stop monitoring an RDMnet scope for brokers.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * \param[in] handle Scope handle to stop monitoring.
 */
void rdmnetdisc_stop_monitoring(rdmnet_scope_monitor_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    rdmnetdisc_platform_stop_monitoring(handle);
    scope_monitor_remove(handle);
    scope_monitor_delete(handle);
    RDMNET_DISC_UNLOCK();
  }
}

/*!
 * \brief Stop monitoring all RDMnet scopes for brokers.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 */
void rdmnetdisc_stop_monitoring_all()
{
  if (!rdmnet_core_initialized())
    return;

  stop_monitoring_all_internal();
}

void stop_monitoring_all_internal()
{
  if (RDMNET_DISC_LOCK())
  {
    if (scope_ref_list)
    {
      RdmnetScopeMonitorRef* ref = scope_ref_list;
      RdmnetScopeMonitorRef* next_ref;
      while (ref)
      {
        next_ref = ref->next;
        rdmnetdisc_platform_stop_monitoring(ref);
        scope_monitor_delete(ref);
        ref = next_ref;
      }
      scope_ref_list = NULL;
    }
    RDMNET_DISC_UNLOCK();
  }
}

/*!
 * \brief Register an RDMnet broker on a scope.
 *
 * The library will also monitor the given scope for conflicting brokers. There will be a holdoff
 * period initially where the scope will be queried for conflicting brokers before registering. If a
 * conflicting broker is found during this time, you will get a broker_found() callback and no
 * broker_registered() callback - this indicates that the local broker should shutdown until
 * receiving a corresponding broker_lost() callback.
 *
 * The broker_registered() callback will be called when the broker is successfully registered.
 *
 * Asynchronous register and monitoring errors can also be delivered via the broker_register_error()
 * and scope_monitor_error() callbacks.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * \param[in] config Configuration struct with details about the broker to register.
 * \param[out] handle Filled in on success with a handle that can be used to access the registered
 *                    broker later.
 * \return #kEtcPalErrOk: Monitoring started successfully.
 * \return #kEtcPalInvalid: Invalid argument.
 * \return #kEtcPalErrNotInit: Module not initialized.
 * \return #kEtcPalErrNoMem: Couldn't allocate resources to register this broker.
 * \return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnetdisc_register_broker(const RdmnetBrokerRegisterConfig* config, rdmnet_registered_broker_t* handle)
{
  if (!config || !handle || !broker_info_is_valid(&config->my_info))
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrOk;
  if (RDMNET_DISC_LOCK())
  {
    RdmnetBrokerRegisterRef* broker_ref = registered_broker_new(config);
    if (!broker_ref)
      res = kEtcPalErrNoMem;

    if (res == kEtcPalErrOk)
    {
      // Begin monitoring the broker's scope for other brokers
      // Static is OK here as we're inside the lock.
      static RdmnetScopeMonitorConfig monitor_config;
      rdmnet_safe_strncpy(monitor_config.scope, config->my_info.scope, E133_SCOPE_STRING_PADDED_LENGTH);
      rdmnet_safe_strncpy(monitor_config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);

      int mon_error;
      res = rdmnetdisc_start_monitoring(&monitor_config, &broker_ref->scope_monitor_handle, &mon_error);

      if (res == kEtcPalErrOk)
      {
        // We wait for the timeout to make sure there are no other brokers around with the same
        // scope.
        broker_ref->scope_monitor_handle->broker_handle = broker_ref;
        etcpal_timer_start(&broker_ref->query_timer, BROKER_REG_QUERY_TIMEOUT);
        *handle = broker_ref;
      }
      else
      {
        registered_broker_delete(broker_ref);
      }
    }
    RDMNET_DISC_UNLOCK();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/*!
 * \brief Unegister an RDMnet broker on a scope.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * \param[in] handle Broker handle to unregister.
 */
void rdmnetdisc_unregister_broker(rdmnet_registered_broker_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (handle->state != kBrokerStateNotRegistered)
  {
    /* Since the broker only cares about scopes while it is running, shut down any outstanding
     * queries for that scope.*/
    rdmnetdisc_stop_monitoring(handle->scope_monitor_handle);
    handle->scope_monitor_handle = NULL;

    if (RDMNET_DISC_LOCK())
    {
      rdmnetdisc_platform_unregister_broker(handle);
      registered_broker_remove(handle);
      registered_broker_delete(handle);
      RDMNET_DISC_UNLOCK();
    }
  }
}

/* Internal function to handle periodic RDMnet discovery functionality, called from
 * rdmnet_core_tick().
 */
void rdmnetdisc_tick(void)
{
  if (!rdmnet_core_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    for (RdmnetBrokerRegisterRef* broker_ref = broker_ref_list; broker_ref; broker_ref = broker_ref->next)
    {
      process_broker_state(broker_ref);
    }
    RDMNET_DISC_UNLOCK();
  }
  rdmnetdisc_platform_tick();
}

void process_broker_state(RdmnetBrokerRegisterRef* broker_ref)
{
  if (broker_ref->state == kBrokerStateQuerying)
  {
    if (!broker_ref->query_timeout_expired && etcpal_timer_is_expired(&broker_ref->query_timer))
      broker_ref->query_timeout_expired = true;

    if (broker_ref->query_timeout_expired && !broker_ref->scope_monitor_handle->broker_list)
    {
      // If the initial query timeout is expired and we haven't discovered any conflicting brokers,
      // we can proceed.
      broker_ref->state = kBrokerStateRegisterStarted;

      int platform_error = 0;
      if (rdmnetdisc_platform_register_broker(&broker_ref->config.my_info, broker_ref, &platform_error) != kEtcPalErrOk)
      {
        broker_ref->state = kBrokerStateNotRegistered;
        broker_ref->config.callbacks.broker_register_error(broker_ref, platform_error,
                                                           broker_ref->config.callback_context);
      }
    }
  }
}

/* Adds a new scope info to the scope_ref_list. Assumes a lock is already taken. */
void scope_monitor_insert(RdmnetScopeMonitorRef* scope_ref)
{
  if (scope_ref)
  {
    scope_ref->next = NULL;

    if (!scope_ref_list)
    {
      // Make the new scope the head of the list.
      scope_ref_list = scope_ref;
    }
    else
    {
      // Insert the new scope at the end of the list.
      RdmnetScopeMonitorRef* ref = scope_ref_list;
      for (; ref->next; ref = ref->next)
        ;
      ref->next = scope_ref;
    }
  }
}

RdmnetScopeMonitorRef* scope_monitor_new(const RdmnetScopeMonitorConfig* config)
{
  RdmnetScopeMonitorRef* new_monitor = (RdmnetScopeMonitorRef*)malloc(sizeof(RdmnetScopeMonitorRef));
  if (new_monitor)
  {
    new_monitor->config = *config;
    new_monitor->broker_handle = NULL;
    new_monitor->broker_list = NULL;
    new_monitor->next = NULL;
  }
  return new_monitor;
}

/* Removes an entry from scope_ref_list. Assumes a lock is already taken. */
void scope_monitor_remove(const RdmnetScopeMonitorRef* ref)
{
  if (!scope_ref_list)
    return;

  if (ref == scope_ref_list)
  {
    // Remove the element at the head of the list
    scope_ref_list = ref->next;
  }
  else
  {
    RdmnetScopeMonitorRef* prev_ref = scope_ref_list;
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
  free(ref);
}

DiscoveredBroker* discovered_broker_new(const char* service_name, const char* full_service_name)
{
  DiscoveredBroker* new_db = (DiscoveredBroker*)malloc(sizeof(DiscoveredBroker));
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

void discovered_broker_delete(DiscoveredBroker* db)
{
  if (db->state != kResolveStateDone)
  {
    etcpal_poll_remove_socket(&disc_state.poll_context, DNSServiceRefSockFD(db->dnssd_ref));
    DNSServiceRefDeallocate(db->dnssd_ref);
  }
  BrokerListenAddr* listen_addr = db->info.listen_addr_list;
  while (listen_addr)
  {
    BrokerListenAddr* next_listen_addr = listen_addr->next;
    free(listen_addr);
    listen_addr = next_listen_addr;
  }
  free(db);
}

RdmnetBrokerRegisterRef* registered_broker_new(const RdmnetBrokerRegisterConfig* config)
{
  RdmnetBrokerRegisterRef* new_rb = (RdmnetBrokerRegisterRef*)malloc(sizeof(RdmnetBrokerRegisterRef));
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

void registered_broker_delete(RdmnetBrokerRegisterRef* rb)
{
  free(rb);
}

/* Adds broker discovery information into brokers.
 * Assumes a lock is already taken.*/
void discovered_broker_insert(DiscoveredBroker** list_head_ptr, DiscoveredBroker* new_db)
{
  if (*list_head_ptr)
  {
    DiscoveredBroker* cur = *list_head_ptr;
    for (; cur->next; cur = cur->next)
      ;
    cur->next = new_db;
  }
  else
  {
    *list_head_ptr = new_db;
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

bool broker_info_is_valid(const RdmnetBrokerDiscInfo* info)
{
  // Make sure none of the broker info's fields are empty
  return !(ETCPAL_UUID_IS_NULL(&info->cid) || strlen(info->service_name) == 0 || strlen(info->scope) == 0 ||
           strlen(info->model) == 0 || strlen(info->manufacturer) == 0);
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
