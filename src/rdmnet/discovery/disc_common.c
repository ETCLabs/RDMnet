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

#include "disc_common.h"

#include "rdmnet/core/util.h"
#include "rdmnet/private/core.h"
#include "registered_broker.h"
#include "discovered_broker.h"
#include "monitored_scope.h"
#include "disc_platform_api.h"

/***************************** Global variables ******************************/

etcpal_mutex_t rdmnet_disc_lock;

/*********************** Private function prototypes *************************/

static etcpal_error_t start_monitoring_internal(const RdmnetScopeMonitorConfig* config, rdmnet_scope_monitor_t* handle,
                                                int* platform_specific_error);
static void stop_monitoring_all_scopes();
static void unregister_all_brokers();

// Other helpers
static void process_broker_state(RdmnetBrokerRegisterRef* broker_ref);
static bool broker_info_is_valid(const RdmnetBrokerDiscInfo* info);

/*************************** Function definitions ****************************/

/* Internal function to initialize the RDMnet discovery API.
 * Returns kEtcPalErrOk on success, or specific error code on failure.
 */
etcpal_error_t rdmnet_disc_init()
{
  etcpal_error_t res = kEtcPalErrOk;

  if (!etcpal_mutex_create(&rdmnet_disc_lock))
    res = kEtcPalErrSys;

  if (res == kEtcPalErrOk)
    res = rdmnet_disc_platform_init();

  if (res != kEtcPalErrOk)
    etcpal_mutex_destroy(&rdmnet_disc_lock);

  return res;
}

/* Internal function to deinitialize the RDMnet discovery API. */
void rdmnet_disc_deinit()
{
  stop_monitoring_all_scopes();
  unregister_all_brokers();
  etcpal_mutex_destroy(&rdmnet_disc_lock);
}

/*!
 * \brief Initialize an RdmnetBrokerDiscInfo structure with null settings.
 *
 * Note that this does not produce a valid RdmnetBrokerDiscInfo for broker registration - you will
 * need to manipulate the fields with your own broker information before passing it to
 * rdmnet_disc_register_broker().
 *
 * \param[out] broker_info Info struct to nullify.
 */
void rdmnet_disc_init_broker_info(RdmnetBrokerDiscInfo* broker_info)
{
  broker_info->cid = kEtcPalNullUuid;
  memset(broker_info->service_name, 0, E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  broker_info->port = 0;
  broker_info->listen_addrs = NULL;
  broker_info->num_listen_addrs = 0;
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
etcpal_error_t rdmnet_disc_start_monitoring(const RdmnetScopeMonitorConfig* config, rdmnet_scope_monitor_t* handle,
                                            int* platform_specific_error)
{
  if (!config || !handle || !platform_specific_error)
    return kEtcPalErrInvalid;
  if (!rdmnet_core_initialized())
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
etcpal_error_t start_monitoring_internal(const RdmnetScopeMonitorConfig* config, rdmnet_scope_monitor_t* handle,
                                         int* platform_specific_error)
{
  RdmnetScopeMonitorRef* new_monitor = scope_monitor_new(config);
  if (!new_monitor)
    return kEtcPalErrNoMem;

  etcpal_error_t res = rdmnet_disc_platform_start_monitoring(config, new_monitor, platform_specific_error);
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

/*!
 * \brief Stop monitoring an RDMnet scope for brokers.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * \param[in] handle Scope handle to stop monitoring.
 */
void rdmnet_disc_stop_monitoring(rdmnet_scope_monitor_t handle)
{
  if (!handle || !rdmnet_core_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    rdmnet_disc_platform_stop_monitoring(handle);
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
void rdmnet_disc_stop_monitoring_all()
{
  if (!rdmnet_core_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    stop_monitoring_all_scopes();
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
etcpal_error_t rdmnet_disc_register_broker(const RdmnetBrokerRegisterConfig* config, rdmnet_registered_broker_t* handle)
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

/*!
 * \brief Unegister an RDMnet broker on a scope.
 *
 * *This function will deadlock if called directly from an RDMnet discovery callback.*
 *
 * \param[in] handle Broker handle to unregister.
 */
void rdmnet_disc_unregister_broker(rdmnet_registered_broker_t handle)
{
  if (!handle || !rdmnet_core_initialized())
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
 * rdmnet_core_tick().
 */
void rdmnet_disc_tick(void)
{
  if (!rdmnet_core_initialized())
    return;

  if (RDMNET_DISC_LOCK())
  {
    registered_broker_for_each(process_broker_state);
    RDMNET_DISC_UNLOCK();
  }
  rdmnet_disc_platform_tick();
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
      if (rdmnet_disc_platform_register_broker(&broker_ref->config.my_info, broker_ref, &platform_error) !=
          kEtcPalErrOk)
      {
        broker_ref->state = kBrokerStateNotRegistered;
        broker_ref->config.callbacks.broker_register_error(broker_ref, platform_error,
                                                           broker_ref->config.callback_context);
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

void stop_monitoring_all_scopes()
{
  scope_monitor_for_each(rdmnet_disc_platform_stop_monitoring);
  scope_monitor_delete_all();
}

void unregister_all_brokers()
{
  registered_broker_for_each(rdmnet_disc_platform_unregister_broker);
  registered_broker_delete_all();
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
