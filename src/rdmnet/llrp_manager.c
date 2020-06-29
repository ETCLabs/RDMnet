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

#include "rdmnet/llrp_manager.h"

#include "etcpal/common.h"
#include "rdm/uid.h"
#include "rdmnet/common_priv.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/core/opts.h"

/***************************** Private macros ********************************/

#define GET_ENCOMPASSING_MANAGER(manager_ptr) (LlrpManager*)((char*)(manager_ptr)-offsetof(LlrpManager, rc_manager))

#define MANAGER_LOCK(manager_ptr) etcpal_mutex_lock(&(manager_ptr)->lock)
#define MANAGER_UNLOCK(manager_ptr) etcpal_mutex_unlock(&(manager_ptr)->lock)

/*********************** Private function prototypes *************************/

// Creating, destroying, and finding managers
static etcpal_error_t validate_llrp_manager_config(const LlrpManagerConfig* config);
static etcpal_error_t create_new_manager(const LlrpManagerConfig* config, llrp_manager_t* handle);
static etcpal_error_t get_manager(llrp_manager_t handle, LlrpManager** manager);
static void           release_manager(LlrpManager* manager);

static void handle_target_discovered(RCLlrpManager* rc_manager, const LlrpDiscoveredTarget* target);
static void handle_rdm_response_received(RCLlrpManager* rc_manager, const LlrpRdmResponse* resp);
static void handle_discovery_finished(RCLlrpManager* rc_manager);
static void handle_manager_destroyed(RCLlrpManager* rc_manager);

// clang-format off
static const RCLlrpManagerCallbacks kManagerCallbacks =
{
  handle_target_discovered,
  handle_rdm_response_received,
  handle_discovery_finished,
  handle_manager_destroyed
};
// clang-format on

/*************************** Function definitions ****************************/

/**
 * @brief Initialize an LlrpManagerConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * LlrpManagerConfig config;
 * llrp_manager_config_init(&config, 0x6574);
 * @endcode
 *
 * @param[out] config Pointer to LlrpManagerConfig to init.
 * @param[in] manufacturer_id ESTA manufacturer ID. All LLRP managers must have one.
 */
void llrp_manager_config_init(LlrpManagerConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(LlrpManagerConfig));
    config->manu_id = manufacturer_id;
  }
}

/**
 * @brief Set the callbacks in an LLRP manager configuration structure.
 *
 * Items marked "optional" can be NULL.
 *
 * @param[out] config Config struct in which to set the callbacks.
 * @param[in] target_discovered Callback called when a new LLRP target has been discovered.
 * @param[in] rdm_response_received Callback called when an LLRP manager receives a response to an
 *                                  RDM command.
 * @param[in] discovery_finished (optional) Callback called when LLRP discovery has finished.
 * @param[in] context (optional) Pointer to opaque data passed back with each callback.
 */
void llrp_manager_config_set_callbacks(LlrpManagerConfig*                     config,
                                       LlrpManagerTargetDiscoveredCallback    target_discovered,
                                       LlrpManagerRdmResponseReceivedCallback rdm_response_received,
                                       LlrpManagerDiscoveryFinishedCallback   discovery_finished,
                                       void*                                  context)
{
  if (config)
  {
    config->callbacks.target_discovered = target_discovered;
    config->callbacks.rdm_response_received = rdm_response_received;
    config->callbacks.discovery_finished = discovery_finished;
    config->callbacks.context = context;
  }
}

/**
 * @brief Create a new LLRP manager instance.
 *
 * LLRP managers can only be created when #RDMNET_DYNAMIC_MEM is defined nonzero. Otherwise, this
 * function will always fail.
 *
 * @param[in] config Configuration parameters for the LLRP manager to be created.
 * @param[out] handle Handle to the newly-created manager instance.
 * @return #kEtcPalErrOk: Manager created successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No memory to allocate additional manager instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_manager_create(const LlrpManagerConfig* config, llrp_manager_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = validate_llrp_manager_config(config);
  if (res != kEtcPalErrOk)
    return res;

  if (rdmnet_writelock())
  {
    res = create_new_manager(config, handle);
    rdmnet_writeunlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }
  return res;
}

/**
 * @brief Destroy an LLRP manager instance.
 *
 * The handle will be invalidated for any future calls to API functions.
 *
 * @param[in] handle Handle to manager to destroy.
 * @return #kEtcPalErrOk: LLRP manager destroyed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t llrp_manager_destroy(llrp_manager_t handle)
{
  LlrpManager*   manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
    return res;

  rc_llrp_manager_unregister(&manager->rc_manager);
  release_manager(manager);
  return res;
}

/**
 * @brief Start discovery on an LLRP manager.
 *
 * Configure a manager to start discovery and send the first discovery message. Fails if a previous
 * discovery process is still ongoing.
 *
 * @param[in] handle Handle to LLRP manager on which to start discovery.
 * @param[in] filter Discovery filter, made up of one or more of the LLRP_FILTERVAL_* constants
 *                   defined in rdmnet/defs.h
 * @return #kEtcPalErrOk: Discovery started successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 * @return #kEtcPalErrAlready: A discovery operation is already in progress.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t llrp_manager_start_discovery(llrp_manager_t handle, uint16_t filter)
{
  LlrpManager*   manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_manager_start_discovery(&manager->rc_manager, filter);
  release_manager(manager);
  return res;
}

/**
 * @brief Stop discovery on an LLRP manager.
 *
 * Clears all discovery state and known discovered targets.
 *
 * @param[in] handle Handle to LLRP manager on which to stop discovery.
 * @return #kEtcPalErrOk: Discovery stopped successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 */
etcpal_error_t llrp_manager_stop_discovery(llrp_manager_t handle)
{
  LlrpManager*   manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_manager_stop_discovery(&manager->rc_manager);
  release_manager(manager);
  return res;
}

/**
 * @brief Send an RDM command from an LLRP manager.
 *
 * On success, provides the sequence number to correlate with a response.
 *
 * @param[in] handle Handle to LLRP manager from which to send the RDM command.
 * @param[in] destination Addressing information for LLRP target to which to send the command.
 * @param[in] command_class Whether this is a GET or a SET command.
 * @param[in] param_id The command's RDM parameter ID.
 * @param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * @param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * @param[out] seq_num Filled in on success with the LLRP sequence number of the command.
 * @return #kEtcPalErrOk: Command sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_manager_send_rdm_command(llrp_manager_t             handle,
                                             const LlrpDestinationAddr* destination,
                                             rdmnet_command_class_t     command_class,
                                             uint16_t                   param_id,
                                             const uint8_t*             data,
                                             uint8_t                    data_len,
                                             uint32_t*                  seq_num)
{
  LlrpManager*   manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_manager_send_rdm_command(&manager->rc_manager, destination, command_class, param_id, data, data_len,
                                         seq_num);
  release_manager(manager);
  return res;
}

/**
 * @brief Send an RDM GET command from an LLRP manager.
 *
 * On success, provides the sequence number to correlate with a response.
 *
 * @param[in] handle Handle to LLRP manager from which to send the GET command.
 * @param[in] destination Addressing information for LLRP target to which to send the command.
 * @param[in] param_id The command's RDM parameter ID.
 * @param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * @param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * @param[out] seq_num Filled in on success with the LLRP sequence number of the command.
 * @return #kEtcPalErrOk: Command sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_manager_send_get_command(llrp_manager_t             handle,
                                             const LlrpDestinationAddr* destination,
                                             uint16_t                   param_id,
                                             const uint8_t*             data,
                                             uint8_t                    data_len,
                                             uint32_t*                  seq_num)
{
  LlrpManager*   manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_manager_send_rdm_command(&manager->rc_manager, destination, kRdmnetCCGetCommand, param_id, data,
                                         data_len, seq_num);
  release_manager(manager);
  return res;
}

/**
 * @brief Send an RDM SET command from an LLRP manager.
 *
 * On success, provides the sequence number to correlate with a response.
 *
 * @param[in] handle Handle to LLRP manager from which to send the SET command.
 * @param[in] destination Addressing information for LLRP target to which to send the command.
 * @param[in] param_id The command's RDM parameter ID.
 * @param[in] data Any RDM parameter data associated with the command (NULL for no data).
 * @param[in] data_len Length of any RDM parameter data associated with the command (0 for no data).
 * @param[out] seq_num Filled in on success with the LLRP sequence number of the command.
 * @return #kEtcPalErrOk: Command sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP manager instance.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_manager_send_set_command(llrp_manager_t             handle,
                                             const LlrpDestinationAddr* destination,
                                             uint16_t                   param_id,
                                             const uint8_t*             data,
                                             uint8_t                    data_len,
                                             uint32_t*                  seq_num)
{
  LlrpManager*   manager;
  etcpal_error_t res = get_manager(handle, &manager);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_manager_send_rdm_command(&manager->rc_manager, destination, kRdmnetCCSetCommand, param_id, data,
                                         data_len, seq_num);
  release_manager(manager);
  return res;
}

etcpal_error_t validate_llrp_manager_config(const LlrpManagerConfig* config)
{
  if ((config->netint.ip_type != kEtcPalIpTypeV4 && config->netint.ip_type != kEtcPalIpTypeV6) ||
      ETCPAL_UUID_IS_NULL(&config->cid) || config->manu_id == 0 || !config->callbacks.target_discovered ||
      !config->callbacks.rdm_response_received || !config->callbacks.discovery_finished)
  {
    return kEtcPalErrInvalid;
  }
  return kEtcPalErrOk;
}

etcpal_error_t create_new_manager(const LlrpManagerConfig* config, llrp_manager_t* handle)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  LlrpManager* new_manager = alloc_llrp_manager_instance();
  if (!new_manager)
    return res;

  RCLlrpManager* rc_manager = &new_manager->rc_manager;
  rc_manager->cid = config->cid;
  rc_manager->uid.manu = 0x8000 | config->manu_id;
  rc_manager->uid.id = (uint32_t)rand();
  rc_manager->netint = config->netint;
  rc_manager->callbacks = kManagerCallbacks;
  rc_manager->lock = &new_manager->lock;
  res = rc_llrp_manager_register(rc_manager);
  if (res != kEtcPalErrOk)
  {
    free_llrp_manager_instance(new_manager);
    return res;
  }

  new_manager->callbacks = config->callbacks;
  *handle = new_manager->id.handle;
  return res;
}

etcpal_error_t get_manager(llrp_manager_t handle, LlrpManager** manager)
{
  if (handle == LLRP_MANAGER_INVALID)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  LlrpManager* found_manager = (LlrpManager*)find_struct_instance(handle, kRdmnetStructTypeLlrpManager);
  if (!found_manager)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }

  if (!MANAGER_LOCK(found_manager))
  {
    rdmnet_readunlock();
    return kEtcPalErrSys;
  }

  *manager = found_manager;
  // Return keeping the locks
  return kEtcPalErrOk;
}

void release_manager(LlrpManager* manager)
{
  MANAGER_UNLOCK(manager);
  rdmnet_readunlock();
}

void handle_target_discovered(RCLlrpManager* rc_manager, const LlrpDiscoveredTarget* target)
{
  RDMNET_ASSERT(rc_manager);
  LlrpManager* manager = GET_ENCOMPASSING_MANAGER(rc_manager);
  manager->callbacks.target_discovered(manager->id.handle, target, manager->callbacks.context);
}

void handle_rdm_response_received(RCLlrpManager* rc_manager, const LlrpRdmResponse* resp)
{
  RDMNET_ASSERT(rc_manager);
  LlrpManager* manager = GET_ENCOMPASSING_MANAGER(rc_manager);
  manager->callbacks.rdm_response_received(manager->id.handle, resp, manager->callbacks.context);
}

void handle_discovery_finished(RCLlrpManager* rc_manager)
{
  RDMNET_ASSERT(rc_manager);
  LlrpManager* manager = GET_ENCOMPASSING_MANAGER(rc_manager);
  manager->callbacks.discovery_finished(manager->id.handle, manager->callbacks.context);
}

void handle_manager_destroyed(RCLlrpManager* rc_manager)
{
  RDMNET_ASSERT(rc_manager);
  LlrpManager* manager = GET_ENCOMPASSING_MANAGER(rc_manager);
  free_llrp_manager_instance(manager);
}
