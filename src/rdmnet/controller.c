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

#include "rdmnet/controller.h"

#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif
#include "rdmnet/private/controller.h"

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_controller() malloc(sizeof(RdmnetController))
#define free_rdmnet_controller(ptr) free(ptr)
#else
#define alloc_rdmnet_controller() etcpal_mempool_alloc(rdmnet_controllers)
#define free_rdmnet_controller(ptr) etcpal_mempool_free(rdmnet_controllers, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(rdmnet_controllers, RdmnetController, RDMNET_MAX_CONTROLLERS);
#endif

/*********************** Private function prototypes *************************/

static void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                             const RdmnetClientConnectedInfo* info, void* context);
static void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                  const RdmnetClientConnectFailedInfo* info, void* context);
static void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                const RdmnetClientDisconnectedInfo* info, void* context);
static void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                                       const BrokerMessage* msg, void* context);
static void client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd, void* context);
static void client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RptClientMessage* msg,
                                void* context);

// clang-format off
static const RptClientCallbacks client_callbacks =
{
  client_connected,
  client_connect_failed,
  client_disconnected,
  client_broker_msg_received,
  client_llrp_msg_received,
  client_msg_received
};
// clang-format on

/*************************** Function definitions ****************************/

/*!
 * \brief Initialize the RDMnet Controller library.
 *
 * Only one call to this function can be made per application.
 *
 * \param[in] lparams Optional: log parameters to pass to the underlying library.
 * \param[in] netint_config Optional: set of network interfaces to which to restrict multicast
 *                          operation.
 * \return #kEtcPalErrOk: Initialization successful.
 * \return Errors forwarded from rdmnet_client_init()
 */
etcpal_error_t rdmnet_controller_init(const EtcPalLogParams* lparams, const RdmnetNetintConfig* netint_config)
{
#if !RDMNET_DYNAMIC_MEM
  etcpal_error_t res = etcpal_mempool_init(rdmnet_controllers);
  if (res != kEtcPalErrOk)
    return res;
#endif

  return rdmnet_client_init(lparams, netint_config);
}

void rdmnet_controller_deinit()
{
  rdmnet_client_deinit();
}

/*!
 * \brief Create a new instance of RDMnet controller functionality.
 *
 * Each controller is identified by a single component ID (CID). Typical controller applications
 * will only need one controller instance.
 *
 * \param[in] config Configuration parameters to use for this controller instance.
 * \param[out] handle Filled in on success with a handle to the new controller instance.
 * \return #kEtcPalErrOk: Controller created successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNoMem: No memory to allocate new controller instance.
 * \return Errors forwarded from rdmnet_rpt_client_create().
 */
etcpal_error_t rdmnet_controller_create(const RdmnetControllerConfig* config, rdmnet_controller_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;

  RdmnetController* new_controller = alloc_rdmnet_controller();
  if (!new_controller)
    return kEtcPalErrNoMem;

  RdmnetRptClientConfig client_config;
  client_config.type = kRPTClientTypeController;
  client_config.cid = config->cid;
  client_config.callbacks = client_callbacks;
  client_config.callback_context = new_controller;
  client_config.optional = config->optional;
  client_config.llrp_optional = config->llrp_optional;

  etcpal_error_t res = rdmnet_rpt_client_create(&client_config, &new_controller->client_handle);
  if (res == kEtcPalErrOk)
  {
    // Do the rest of the initialization
    new_controller->callbacks = config->callbacks;
    new_controller->callback_context = config->callback_context;

    *handle = new_controller;
  }
  else
  {
    free_rdmnet_controller(new_controller);
  }
  return res;
}

/*!
 * \brief Destroy a controller instance.
 *
 * Will disconnect all scopes to which this controller is currently connected, sending the
 * disconnect reason provided in the reason parameter.
 *
 * \param[in] handle Handle to controller to destroy, no longer valid after this function returns.
 * \param[in] reason Disconnect reason code to send on all connected scopes.
 * \return #kEtcPalErrOk: Controller destroyed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Errors forwarded from rdmnet_client_destroy().
 */
etcpal_error_t rdmnet_controller_destroy(rdmnet_controller_t handle, rdmnet_disconnect_reason_t reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  etcpal_error_t res = rdmnet_client_destroy(handle->client_handle, reason);
  if (res == kEtcPalErrOk)
    free_rdmnet_controller(handle);

  return res;
}

/*!
 */
etcpal_error_t rdmnet_controller_add_scope(rdmnet_controller_t handle, const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t* scope_handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_add_scope(handle->client_handle, scope_config, scope_handle);
}

etcpal_error_t rdmnet_controller_remove_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              rdmnet_disconnect_reason_t reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_remove_scope(handle->client_handle, scope_handle, reason);
}

etcpal_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const LocalRdmCommand* cmd, uint32_t* seq_num)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_command(handle->client_handle, scope_handle, cmd, seq_num);
}

etcpal_error_t rdmnet_controller_send_rdm_response(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                   const LocalRdmResponse* resp)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_response(handle->client_handle, scope_handle, resp);
}

etcpal_error_t rdmnet_controller_send_llrp_response(rdmnet_controller_t handle, const LlrpLocalRdmResponse* resp)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_llrp_response(handle->client_handle, resp);
}

etcpal_error_t rdmnet_controller_request_client_list(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_request_client_list(handle->client_handle, scope_handle);
}

void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo* info,
                      void* context)
{
  RDMNET_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.connected(controller, scope_handle, info, controller->callback_context);
  }
}

void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                           const RdmnetClientConnectFailedInfo* info, void* context)
{
  RDMNET_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.connect_failed(controller, scope_handle, info, controller->callback_context);
  }
}

void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientDisconnectedInfo* info, void* context)
{
  RDMNET_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.disconnected(controller, scope_handle, info, controller->callback_context);
  }
}

void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg,
                                void* context)
{
  RDMNET_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    switch (msg->vector)
    {
      case VECTOR_BROKER_CONNECTED_CLIENT_LIST:
      case VECTOR_BROKER_CLIENT_ADD:
      case VECTOR_BROKER_CLIENT_REMOVE:
      case VECTOR_BROKER_CLIENT_ENTRY_CHANGE:
        controller->callbacks.client_list_update(controller, scope_handle, (client_list_action_t)msg->vector,
                                                 GET_CLIENT_LIST(msg), controller->callback_context);
        break;
      default:
        break;
    }
  }
}

void client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd, void* context)
{
  RDMNET_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    controller->callbacks.llrp_rdm_command_received(controller, cmd, controller->callback_context);
  }
}

void client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RptClientMessage* msg,
                         void* context)
{
  RDMNET_UNUSED_ARG(handle);

  RdmnetController* controller = (RdmnetController*)context;
  if (controller)
  {
    switch (msg->type)
    {
      case kRptClientMsgRdmCmd:
        controller->callbacks.rdm_command_received(controller, scope_handle, GET_REMOTE_RDM_COMMAND(msg),
                                                   controller->callback_context);
        break;
      case kRptClientMsgRdmResp:
        controller->callbacks.rdm_response_received(controller, scope_handle, GET_REMOTE_RDM_RESPONSE(msg),
                                                    controller->callback_context);
        break;
      case kRptClientMsgStatus:
        controller->callbacks.status_received(controller, scope_handle, GET_REMOTE_RPT_STATUS(msg),
                                              controller->callback_context);
        break;
      default:
        break;
    }
  }
}
