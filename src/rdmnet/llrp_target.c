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

#include "rdmnet/llrp_target.h"

#include "etcpal/common.h"
#include "rdm/uid.h"
#include "rdmnet/common_priv.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/core/opts.h"

/***************************** Private macros ********************************/

#define GET_ENCOMPASSING_TARGET(target_ptr) (LlrpTarget*)((char*)(target_ptr)-offsetof(LlrpTarget, rc_target))

#define TARGET_LOCK(target_ptr) etcpal_mutex_lock(&(target_ptr)->lock)
#define TARGET_UNLOCK(target_ptr) etcpal_mutex_unlock(&(target_ptr)->lock)

/*********************** Private function prototypes *************************/

// Creating, destroying, and finding targets
static etcpal_error_t validate_llrp_target_config(const LlrpTargetConfig* config);
static etcpal_error_t create_new_target(const LlrpTargetConfig* config, llrp_target_t* handle);
static etcpal_error_t get_target(llrp_target_t handle, LlrpTarget** target);
static void           release_target(LlrpTarget* target);

static void handle_rdm_command_received(RCLlrpTarget*                rc_target,
                                        const LlrpRdmCommand*        cmd,
                                        RCLlrpTargetSyncRdmResponse* response);
static void handle_target_destroyed(RCLlrpTarget* rc_target);

// clang-format off
static const RCLlrpTargetCallbacks kTargetCallbacks =
{
  handle_rdm_command_received,
  handle_target_destroyed
};
// clang-format on

/*************************** Function definitions ****************************/

/**
 * @brief Initialize an LlrpTargetConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * @code
 * LlrpTargetConfig config;
 * llrp_target_config_init(&config, 0x6574);
 * @endcode
 *
 * @param[out] config Pointer to LlrpTargetConfig to init.
 * @param[in] manufacturer_id ESTA manufacturer ID. All LLRP targets must have one.
 */
void llrp_target_config_init(LlrpTargetConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(LlrpTargetConfig));
    RDMNET_INIT_DYNAMIC_UID_REQUEST(&config->uid, manufacturer_id);
  }
}

/**
 * @brief Create a new LLRP target instance.
 * @param[in] config Configuration parameters for the LLRP target to be created.
 * @param[out] handle Handle to the newly-created target instance.
 * @return #kEtcPalErrOk: Target created successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNoMem: No memory to allocate additional target instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_target_create(const LlrpTargetConfig* config, llrp_target_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;

  etcpal_error_t res = validate_llrp_target_config(config);
  if (res != kEtcPalErrOk)
    return res;

  if (rdmnet_writelock())
  {
    res = create_new_target(config, handle);
    rdmnet_writeunlock();
  }
  else
  {
    res = kEtcPalErrSys;
  }
  return res;
}

/**
 * @brief Destroy an LLRP target instance.
 *
 * The handle will be invalidated for any future calls to API functions.
 *
 * @param[in] handle Handle to target to destroy.
 * @return #kEtcPalErrOk: LLRP target destroyed successfully.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP target instance.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 */
etcpal_error_t llrp_target_destroy(llrp_target_t handle)
{
  LlrpTarget*    target;
  etcpal_error_t res = get_target(handle, &target);
  if (res != kEtcPalErrOk)
    return res;

  rc_llrp_target_unregister(&target->rc_target);
  rdmnet_unregister_struct_instance(target);
  release_target(target);
  return res;
}

/**
 * @brief Send an RDM ACK response from an LLRP target.
 *
 * @param[in] handle Handle to LLRP target from which to send the response.
 * @param[in] received_cmd Previously-received command that the ACK is a response to.
 * @param[in] response_data Parameter data that goes with this ACK, or NULL if no data.
 * @param[in] response_data_len Length in bytes of response_data, or 0 if no data.
 * @return #kEtcPalErrOk: ACK sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP target instance.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_target_send_ack(llrp_target_t              handle,
                                    const LlrpSavedRdmCommand* received_cmd,
                                    const uint8_t*             response_data,
                                    uint8_t                    response_data_len)
{
  LlrpTarget*    target;
  etcpal_error_t res = get_target(handle, &target);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_target_send_ack(&target->rc_target, received_cmd, response_data, response_data_len);
  release_target(target);
  return res;
}

/**
 * @brief Send an RDM NACK response from an LLRP target.
 *
 * @param[in] handle Handle to LLRP target from which to send the response.
 * @param[in] received_cmd Previously-received command that the NACK is a response to.
 * @param[in] nack_reason RDM NACK reason code to send with the NACK.
 * @return #kEtcPalErrOk: NACK sent successfully.
 * @return #kEtcPalErrInvalid: Invalid argument provided.
 * @return #kEtcPalErrNotInit: Module not initialized.
 * @return #kEtcPalErrNotFound: Handle is not associated with a valid LLRP target instance.
 * @return Note: Other error codes might be propagated from underlying socket calls.
 */
etcpal_error_t llrp_target_send_nack(llrp_target_t              handle,
                                     const LlrpSavedRdmCommand* received_cmd,
                                     rdm_nack_reason_t          nack_reason)
{
  LlrpTarget*    target;
  etcpal_error_t res = get_target(handle, &target);
  if (res != kEtcPalErrOk)
    return res;

  res = rc_llrp_target_send_nack(&target->rc_target, received_cmd, nack_reason);
  release_target(target);
  return res;
}

etcpal_error_t validate_llrp_target_config(const LlrpTargetConfig* config)
{
  if (ETCPAL_UUID_IS_NULL(&config->cid) || !config->callbacks.rdm_command_received ||
      (!RDMNET_UID_IS_DYNAMIC_UID_REQUEST(&config->uid) && (config->uid.manu & 0x8000)))
  {
    return kEtcPalErrInvalid;
  }
  return kEtcPalErrOk;
}

etcpal_error_t create_new_target(const LlrpTargetConfig* config, llrp_target_t* handle)
{
  etcpal_error_t res = kEtcPalErrNoMem;

  LlrpTarget* new_target = rdmnet_alloc_llrp_target_instance();
  if (!new_target)
    return res;

  RCLlrpTarget* rc_target = &new_target->rc_target;

  rc_target->cid = config->cid;
  rc_target->uid = config->uid;
  rc_target->component_type = kLlrpCompNonRdmnet;
  rc_target->callbacks = kTargetCallbacks;
  rc_target->lock = &new_target->lock;
  res = rc_llrp_target_register(rc_target, config->netints, config->num_netints);
  if (res != kEtcPalErrOk)
  {
    rdmnet_free_struct_instance(new_target);
    return res;
  }

  new_target->callbacks = config->callbacks;
  new_target->response_buf = config->response_buf;
  *handle = new_target->id.handle;
  return res;
}

etcpal_error_t get_target(llrp_target_t handle, LlrpTarget** target)
{
  if (handle == LLRP_TARGET_INVALID)
    return kEtcPalErrInvalid;
  if (!rc_initialized())
    return kEtcPalErrNotInit;
  if (!rdmnet_readlock())
    return kEtcPalErrSys;

  LlrpTarget* found_target = (LlrpTarget*)rdmnet_find_struct_instance(handle, kRdmnetStructTypeLlrpTarget);
  if (!found_target)
  {
    rdmnet_readunlock();
    return kEtcPalErrNotFound;
  }

  if (!TARGET_LOCK(found_target))
  {
    rdmnet_readunlock();
    return kEtcPalErrSys;
  }

  *target = found_target;
  return kEtcPalErrOk;
}

void release_target(LlrpTarget* target)
{
  TARGET_UNLOCK(target);
  rdmnet_readunlock();
}

void handle_rdm_command_received(RCLlrpTarget*                rc_target,
                                 const LlrpRdmCommand*        cmd,
                                 RCLlrpTargetSyncRdmResponse* response)
{
  RDMNET_ASSERT(rc_target);
  LlrpTarget* target = GET_ENCOMPASSING_TARGET(rc_target);
  target->callbacks.rdm_command_received(target->id.handle, cmd, &response->resp, target->callbacks.context);
  response->response_buf = target->response_buf;
}

void handle_target_destroyed(RCLlrpTarget* rc_target)
{
  RDMNET_ASSERT(rc_target);
  LlrpTarget* target = GET_ENCOMPASSING_TARGET(rc_target);
  rdmnet_free_struct_instance(target);
}
