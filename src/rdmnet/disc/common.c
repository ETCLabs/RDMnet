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

#include "rdmnet/disc/common.h"

#include <string.h>
#include "rdmnet/core/util.h"
#include "rdmnet/disc/registered_broker.h"
#include "rdmnet/disc/discovered_broker.h"
#include "rdmnet/disc/monitored_scope.h"
#include "rdmnet/disc/platform_api.h"

/***************************** Global variables ******************************/

etcpal_mutex_t rdmnet_disc_lock;

/*********************** Private function prototypes *************************/

static etcpal_error_t start_monitoring_internal(const RdmnetScopeMonitorConfig* config,
                                                rdmnet_scope_monitor_t*         handle,
                                                int*                            platform_specific_error);
static void           stop_monitoring_all_scopes(void);
static void           unregister_all_brokers(void);

// Other helpers
static void process_broker_state(RdmnetBrokerRegisterRef* broker_ref);
static bool conflicting_broker_found(RdmnetBrokerRegisterRef* broker_ref, bool* should_deregister);
static bool validate_broker_register_config(const RdmnetBrokerRegisterConfig* config);

/*************************** Function definitions ****************************/

/*
 * Internal function to initialize the RDMnet discovery API.
 * Returns kEtcPalErrOk on success, or specific error code on failure.
 */
etcpal_error_t rdmnet_disc_module_init(const RdmnetNetintConfig* netint_config)
{
  etcpal_error_t res = discovered_broker_module_init();

  if (res == kEtcPalErrOk)
    res = monitored_scope_module_init();

  if (res == kEtcPalErrOk)
  {
    res = registered_broker_module_init();
    if (res != kEtcPalErrOk)
      monitored_scope_module_deinit();
  }

  if (res == kEtcPalErrOk)
  {
    if (!etcpal_mutex_create(&rdmnet_disc_lock))
    {
      registered_broker_module_deinit();
      monitored_scope_module_deinit();
      res = kEtcPalErrSys;
    }
  }

  if (res == kEtcPalErrOk)
  {
    res = rdmnet_disc_platform_init(netint_config);
    if (res != kEtcPalErrOk)
    {
      etcpal_mutex_destroy(&rdmnet_disc_lock);
      registered_broker_module_deinit();
      monitored_scope_module_deinit();
    }
  }

  return res;
}

/* Internal function to deinitialize the RDMnet discovery API. */
void rdmnet_disc_module_deinit(void)
{
  stop_monitoring_all_scopes();
  unregister_all_brokers();
  rdmnet_disc_platform_deinit();
  etcpal_mutex_destroy(&rdmnet_disc_lock);
  registered_broker_module_deinit();
}

/**
 * @brief Initialize an RdmnetBrokerRegisterConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * RdmnetBrokerRegisterConfig config;
 * rdmnet_broker_register_config_init(&config);
 * @endcode
 *
 * @param[out] config Pointer to RdmnetBrokerRegisterConfig to init.
 */
void rdmnet_broker_register_config_init(RdmnetBrokerRegisterConfig* config)
{
  if (config)
    memset(config, 0, sizeof(RdmnetBrokerRegisterConfig));
}

/**
 * @brief Set the callbacks in an RDMnet broker register configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] broker_registered Callback called when a broker is successfully registered.
 * @param[in] broker_register_failed Callback called when a broker registration has failed.
 * @param[in] other_broker_found Callback called when another broker is found on the same scope as a
 *                               registered broker.
 * @param[in] other_broker_lost Callback called when a broker previously found on the same scope as
 *                              a registered broker has gone away.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_broker_register_config_set_callbacks(RdmnetBrokerRegisterConfig*            config,
                                                 RdmnetDiscBrokerRegisteredCallback     broker_registered,
                                                 RdmnetDiscBrokerRegisterFailedCallback broker_register_failed,
                                                 RdmnetDiscOtherBrokerFoundCallback     other_broker_found,
                                                 RdmnetDiscOtherBrokerLostCallback      other_broker_lost,
                                                 void*                                  context)
{
  if (config)
  {
    config->callbacks.broker_registered = broker_registered;
    config->callbacks.broker_register_failed = broker_register_failed;
    config->callbacks.other_broker_found = other_broker_found;
    config->callbacks.other_broker_lost = other_broker_lost;
    config->callbacks.context = context;
  }
}

/**
 * @brief Initialize an RdmnetScopeMonitorConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * RdmnetScopeMonitorConfig config;
 * rdmnet_scope_monitor_config_init(&config);
 * @endcode
 *
 * @param[out] config Pointer to RdmnetScopeMonitorConfig to init.
 */
void rdmnet_scope_monitor_config_init(RdmnetScopeMonitorConfig* config)
{
  if (config)
    memset(config, 0, sizeof(RdmnetScopeMonitorConfig));
}

/**
 * @brief Set the callbacks in an RDMnet scope monitor configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] broker_found Callback called when a broker is discovered on the scope.
 * @param[in] broker_updated Callback called when a previously-discovered broker's information is updated.
 * @param[in] broker_lost Callback called when a previously-discovered broker is lost.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void rdmnet_scope_monitor_config_set_callbacks(RdmnetScopeMonitorConfig*       config,
                                               RdmnetDiscBrokerFoundCallback   broker_found,
                                               RdmnetDiscBrokerUpdatedCallback broker_updated,
                                               RdmnetDiscBrokerLostCallback    broker_lost,
                                               void*                           context)
{
  if (config)
  {
    config->callbacks.broker_found = broker_found;
    config->callbacks.broker_updated = broker_updated;
    config->callbacks.broker_lost = broker_lost;
    config->callbacks.context = context;
  }
}

/**
 * @brief Begin monitoring an RDMnet scope for brokers.
 *
 * Expect to receive callbacks from the RDMnet tick thread when brokers are found and lost.
 * Asynchronous monitoring errors can also be delivered via the scope_monitor_error callback.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * @param[in] config Configuration struct with details about the scope to monitor.
 * @param[out] handle Filled in on success with a handle that can be used to access the monitored
 *                    scope later.
 * @param[out] platform_specific_error Filled in on failure with a platform-specific error code.
 * @return #kEtcPalErrOk: Monitoring started successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Couldn't allocate resources to monitor this scope.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_disc_start_monitoring(const RdmnetScopeMonitorConfig* config,
                                            rdmnet_scope_monitor_t*         handle,
                                            int*                            platform_specific_error)
{
  if (!config || !handle || !platform_specific_error)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = kEtcPalErrOk;
  if (RDMNET_DISC_LOCK())
  {
    res = start_monitoring_internal(config, handle, platform_specific_error);
    RDMNET_DISC_UNLOCK();
  }
  else
  {
    res = kEtcPalErrSys;
  }

  return res;
}

/* Do the actual tasks related to monitoring a scope, within the lock. */
etcpal_error_t start_monitoring_internal(const RdmnetScopeMonitorConfig* config,
                                         rdmnet_scope_monitor_t*         handle,
                                         int*                            platform_specific_error)
{
  RdmnetScopeMonitorRef* new_monitor = scope_monitor_new(config);
  if (!new_monitor)
    return kEtcPalErrNoMem;

  etcpal_error_t res = rdmnet_disc_platform_start_monitoring(new_monitor, platform_specific_error);
  if (res == kEtcPalErrOk)
  {
    scope_monitor_insert(new_monitor);
    *handle = new_monitor;
  }
  else
  {
    scope_monitor_delete(new_monitor);
  }

  return res;
}

/**
 * @brief Stop monitoring an RDMnet scope for brokers.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * @param[in] handle Scope handle to stop monitoring.
 */
void rdmnet_disc_stop_monitoring(rdmnet_scope_monitor_t handle)
{
  if (!handle || !rc_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    rdmnet_disc_platform_stop_monitoring(handle);
    scope_monitor_remove(handle);
    scope_monitor_delete(handle);
    RDMNET_DISC_UNLOCK();
  }
}

/**
 * @brief Stop monitoring all RDMnet scopes for brokers.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 */
void rdmnet_disc_stop_monitoring_all(void)
{
  if (!rc_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    stop_monitoring_all_scopes();
    RDMNET_DISC_UNLOCK();
  }
}

/**
 * @brief Register an RDMnet broker on a scope.
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
 * @param[in] config Configuration struct with details about the broker to register.
 * @param[out] handle Filled in on success with a handle that can be used to access the registered
 *                    broker later.
 * @return #kEtcPalErrOk: Monitoring started successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: Couldn't allocate resources to register this broker.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t rdmnet_disc_register_broker(const RdmnetBrokerRegisterConfig* config, rdmnet_registered_broker_t* handle)
{
  if (!config || !handle || !validate_broker_register_config(config))
    return kEtcPalErrInvalid;
  if (!rc_initialized())
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
      RdmnetScopeMonitorConfig monitor_config;
      memset(&monitor_config, 0, sizeof monitor_config);
      monitor_config.scope = config->scope;

      int mon_error;
      res = start_monitoring_internal(&monitor_config, &broker_ref->scope_monitor_handle, &mon_error);
      if (res == kEtcPalErrOk)
      {
        // We wait for the timeout to make sure there are no other brokers around with the same
        // scope.
        registered_broker_insert(broker_ref);
        broker_ref->state = kBrokerStateQuerying;
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

/**
 * @brief Unegister an RDMnet broker on a scope.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * @param[in] handle Broker handle to unregister.
 */
void rdmnet_disc_unregister_broker(rdmnet_registered_broker_t handle)
{
  if (!handle || !rc_initialized())
    return;

  if (handle->state != kBrokerStateNotRegistered)
  {
    /* Since the broker only cares about scopes while it is running, shut down any outstanding
     * queries for that scope.*/
    rdmnet_disc_stop_monitoring(handle->scope_monitor_handle);
    handle->scope_monitor_handle = NULL;

    if (RDMNET_DISC_LOCK())
    {
      rdmnet_disc_platform_unregister_broker(handle);
      registered_broker_remove(handle);
      registered_broker_delete(handle);
      RDMNET_DISC_UNLOCK();
    }
  }
}

/* Internal function to handle periodic RDMnet discovery functionality, called from
 * rc_tick().
 */
void rdmnet_disc_module_tick(void)
{
  if (RDMNET_DISC_LOCK())
  {
    registered_broker_for_each(process_broker_state);
    RDMNET_DISC_UNLOCK();
  }
  rdmnet_disc_platform_tick();
}

bool rdmnet_disc_broker_should_deregister(const EtcPalUuid* this_broker_cid, const EtcPalUuid* other_broker_cid)
{
  return ETCPAL_UUID_CMP(this_broker_cid, other_broker_cid) < 0;
}

void process_broker_state(RdmnetBrokerRegisterRef* broker_ref)
{
  if (etcpal_timer_is_expired(&broker_ref->query_timer))
  {
    etcpal_timer_reset(&broker_ref->query_timer);

    bool should_deregister = false;
    if (conflicting_broker_found(broker_ref, &should_deregister))
    {
      if (should_deregister && (broker_ref->state == kBrokerStateRegistered))
      {
        rdmnet_disc_platform_unregister_broker(broker_ref);
        broker_ref->state = kBrokerStateQuerying;
      }
    }
    else if (broker_ref->state == kBrokerStateQuerying)
    {
      // If at least the initial query timeout is expired and there aren't any conflicting brokers, we can proceed.
      int platform_error = 0;
      if (rdmnet_disc_platform_register_broker(broker_ref, &platform_error) == kEtcPalErrOk)
      {
        broker_ref->state = kBrokerStateRegistered;
      }
      else
      {
        broker_ref->state = kBrokerStateNotRegistered;
        broker_ref->callbacks.broker_register_failed(broker_ref, platform_error, broker_ref->callbacks.context);
      }
    }
  }
}

bool conflicting_broker_found(RdmnetBrokerRegisterRef* broker_ref, bool* should_deregister)
{
  RDMNET_ASSERT(broker_ref && should_deregister);

  *should_deregister = false;
  for (const DiscoveredBroker* db = broker_ref->scope_monitor_handle->broker_list; db; db = db->next)
  {
    if (ETCPAL_UUID_CMP(&broker_ref->cid, &db->cid) != 0)
    {
      if (rdmnet_disc_broker_should_deregister(&broker_ref->cid, &db->cid))
        *should_deregister = true;

      if (!broker_ref->netints)
        return true;  // All interfaces enabled, so this broker already conflicts

      for (size_t i = 0; i < broker_ref->num_netints; ++i)
      {
        for (size_t j = 0; j < db->num_listen_addrs; ++j)
        {
          if (db->listen_addr_netint_array[j] == 0)
            return true;  // TODO: Remove this case once interface support is added to Avahi & lwmdns impls.
          if (db->listen_addr_netint_array[j] == broker_ref->netints[i])
            return true;  // This broker can be reached on one of our enabled interfaces
        }
      }
    }
  }

  return false;
}

bool validate_broker_register_config(const RdmnetBrokerRegisterConfig* config)
{
  // Make sure none of the broker info's fields are empty
  return !(ETCPAL_UUID_IS_NULL(&config->cid) || !config->service_instance_name ||
           strlen(config->service_instance_name) == 0 || !config->scope || strlen(config->scope) == 0 ||
           !config->model || strlen(config->model) == 0 || !config->manufacturer || strlen(config->manufacturer) == 0);
}

void stop_monitoring_all_scopes(void)
{
  scope_monitor_for_each(rdmnet_disc_platform_stop_monitoring);
  scope_monitor_delete_all();
}

void unregister_all_brokers(void)
{
  registered_broker_for_each(rdmnet_disc_platform_unregister_broker);
  registered_broker_delete_all();
}

void notify_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info)
{
  if (handle->broker_handle)
  {
    if ((ETCPAL_UUID_CMP(&handle->broker_handle->cid, &broker_info->cid) != 0) &&
        handle->broker_handle->callbacks.other_broker_found)
    {
      handle->broker_handle->callbacks.other_broker_found(handle->broker_handle, broker_info,
                                                          handle->broker_handle->callbacks.context);
    }
  }
  else if (handle->callbacks.broker_found)
  {
    handle->callbacks.broker_found(handle, broker_info, handle->callbacks.context);
  }
}

void notify_broker_updated(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info)
{
  if (!handle->broker_handle && handle->callbacks.broker_updated)
  {
    handle->callbacks.broker_updated(handle, broker_info, handle->callbacks.context);
  }
}

void notify_broker_lost(rdmnet_scope_monitor_t handle, const char* service_name, const EtcPalUuid* broker_cid)
{
  if (handle->broker_handle)
  {
    if ((ETCPAL_UUID_CMP(&handle->broker_handle->cid, broker_cid) != 0) &&
        handle->broker_handle->callbacks.other_broker_lost)
    {
      handle->broker_handle->callbacks.other_broker_lost(handle->broker_handle, handle->scope, service_name,
                                                         handle->broker_handle->callbacks.context);
    }
  }
  else if (handle->callbacks.broker_lost)
  {
    handle->callbacks.broker_lost(handle, handle->scope, service_name, handle->callbacks.context);
  }
}
