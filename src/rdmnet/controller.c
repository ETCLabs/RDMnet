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

#include <string.h>
#include "rdmnet/private/opts.h"
#include "rdmnet/private/controller.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if RDMNET_DYNAMIC_MEM
#define ALLOC_RDMNET_CONTROLLER() malloc(sizeof(RdmnetController))
#define FREE_RDMNET_CONTROLLER(ptr) free(ptr)
#else
#define ALLOC_RDMNET_CONTROLLER() etcpal_mempool_alloc(rdmnet_controllers)
#define FREE_RDMNET_CONTROLLER(ptr) etcpal_mempool_free(rdmnet_controllers, ptr)
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

/*!
 * \brief Deinitialize the RDMnet Controller library.
 *
 * Only one call to this function can be made per application. No RDMnet API functions are usable
 * after this function is called.
 */
void rdmnet_controller_deinit()
{
  rdmnet_client_deinit();
}

/*!
 * \brief Initialize an RdmnetControllerConfig with default values for the optional config options.
 *
 * The config struct members not marked 'optional' are not meaningfully initialized by this
 * function. Those members do not have default values and must be initialized manually before
 * passing the config struct to an API function.
 *
 * Usage example:
 * \code
 * RdmnetControllerConfig config;
 * rdmnet_controller_config_init(&config, 0x6574);
 * \endcode
 *
 * \param[out] config Pointer to RdmnetControllerConfig to init.
 * \param[in] manufacturer_id ESTA manufacturer ID. All RDMnet Controllers must have one.
 */
void rdmnet_controller_config_init(RdmnetControllerConfig* config, uint16_t manufacturer_id)
{
  if (config)
  {
    memset(config, 0, sizeof(RdmnetControllerConfig));
    RPT_CLIENT_INIT_OPTIONAL_CONFIG_VALUES(&config->optional, manufacturer_id);
  }
}

/*!
 * \brief Create a new instance of RDMnet controller functionality.
 *
 * Each controller is identified by a single component ID (CID). Typical controller applications
 * will only need one controller instance. RDMnet connection will not be attempted until at least
 * one scope is added using rdmnet_controller_add_scope().
 *
 * \param[in] config Configuration parameters to use for this controller instance.
 * \param[out] handle Filled in on success with a handle to the new controller instance.
 * \return #kEtcPalErrOk: Controller created successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return #kEtcPalErrNoMem: No memory to allocate new controller instance.
 * \return Other errors forwarded from rdmnet_rpt_client_create().
 */
etcpal_error_t rdmnet_controller_create(const RdmnetControllerConfig* config, rdmnet_controller_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;

  RdmnetController* new_controller = ALLOC_RDMNET_CONTROLLER();
  if (!new_controller)
    return kEtcPalErrNoMem;

  RdmnetRptClientConfig client_config;
  client_config.type = kRPTClientTypeController;
  client_config.cid = config->cid;
  client_config.callbacks = client_callbacks;
  client_config.callback_context = new_controller;
  client_config.optional = config->optional;

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
    FREE_RDMNET_CONTROLLER(new_controller);
  }
  return res;
}

/*!
 * \brief Destroy a controller instance.
 *
 * Will disconnect from all brokers to which this controller is currently connected, sending the
 * disconnect reason provided in the disconnect_reason parameter.
 *
 * \param[in] handle Handle to controller to destroy, no longer valid after this function returns.
 * \param[in] disconnect_reason Disconnect reason code to send on all connected scopes.
 * \return #kEtcPalErrOk: Controller destroyed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_client_destroy().
 */
etcpal_error_t rdmnet_controller_destroy(rdmnet_controller_t handle, rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  etcpal_error_t res = rdmnet_client_destroy(handle->client_handle, disconnect_reason);
  if (res == kEtcPalErrOk)
    FREE_RDMNET_CONTROLLER(handle);

  return res;
}

/*!
 * \brief Add a new scope to a controller instance.
 *
 * The library will attempt to discover and connect to a broker for the scope (or just connect if a
 * static broker address is given); the status of these attempts will be communicated via the
 * callbacks associated with the controller instance.
 *
 * \param[in] handle Handle to controller to which to add a new scope.
 * \param[in] scope_config Configuration parameters for the new scope.
 * \param[out] scope_handle Filled in on success with a handle to the new scope.
 * \return #kEtcPalErrOk: New scope added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_client_add_scope().
 */
etcpal_error_t rdmnet_controller_add_scope(rdmnet_controller_t handle, const RdmnetScopeConfig* scope_config,
                                           rdmnet_client_scope_t* scope_handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_add_scope(handle->client_handle, scope_config, scope_handle);
}

/*!
 * \brief Add a new scope representing the default RDMnet scope to a controller instance.
 *
 * This is a shortcut to easily add the default RDMnet scope to a controller. The default behavior
 * is to not use a statically-configured broker. If a static broker is needed on the default scope,
 * rdmnet_controller_add_scope() must be used.
 *
 * \param[in] handle Handle to controller to which to add the default scope.
 * \param[out] scope_handle Filled in on success with a handle to the new scope.
 * \return #kEtcPalErrOk: New scope added successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_client_add_scope().
 */
etcpal_error_t rdmnet_controller_add_default_scope(rdmnet_controller_t handle, rdmnet_client_scope_t* scope_handle)
{
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Remove a previously-added scope from a controller instance.
 *
 * After this call completes, scope_handle will no longer be valid.
 *
 * \param[in] handle Handle to the controller from which to remove a scope.
 * \param[in] scope_handle Handle to scope to remove.
 * \param[in] disconnect_reason RDMnet protocol disconnect reason to send to the connected broker.
 * \return #kEtcPalErrOk: Scope removed successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_client_remove_scope().
 */
etcpal_error_t rdmnet_controller_remove_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                              rdmnet_disconnect_reason_t disconnect_reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_remove_scope(handle->client_handle, scope_handle, disconnect_reason);
}

/*!
 * \brief Retrieve information about a previously-added scope.
 *
 * \param[in] handle Handle to the controller from which to retrieve scope information.
 * \param[in] scope_handle Handle to the scope for which to retrieve the configuration.
 * \param[out] scope_config Filled in with information about the scope.
 * \return #kEtcPalErrOk: Scope information retrieved successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_client_get_scope().
 */
etcpal_error_t rdmnet_controller_get_scope(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                           RdmnetScopeConfig* scope_config)
{
  // TODO
  return kEtcPalErrNotImpl;
}

/*!
 * \brief Send an RDM command from a controller on a scope.
 *
 * The response will be delivered via the \ref RdmnetControllerCallbacks::rdm_response_received
 * "rdm_response_received" callback.
 *
 * \param[in] handle Handle to the controller from which to send the RDM command.
 * \param[in] scope_handle Handle to the scope on which to send the RDM command.
 * \param[in] cmd The RDM command data to send, including its addressing information.
 * \param[out] seq_num Filled in on success with a sequence number which can be used to match the
 *                     command with a response.
 * \return #kEtcPalErrOk: Command sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_rpt_client_send_rdm_command().
 */
etcpal_error_t rdmnet_controller_send_rdm_command(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                  const RdmnetLocalRdmCommand* cmd, uint32_t* seq_num)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_command(handle->client_handle, scope_handle, cmd, seq_num);
}

/*!
 * \brief Send an RDM response from a controller on a scope.
 *
 * \param[in] handle Handle to the controller from which to send the RDM response.
 * \param[in] scope_handle Handle to the scope on which to send the RDM response.
 * \param[in] resp The RDM response data to send, including its addressing information.
 * \return #kEtcPalErrOk: Response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_rpt_client_send_rdm_response().
 */
etcpal_error_t rdmnet_controller_send_rdm_response(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                                   const RdmnetLocalRdmResponse* resp)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_response(handle->client_handle, scope_handle, resp);
}

/*!
 * \brief Send an LLRP RDM response from a controller.
 *
 * \param[in] handle Handle to the controller from which to send the LLRP RDM response.
 * \param[in] resp The RDM response data to send, including its addressing information.
 * \return #kEtcPalErrOk: Response sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_rpt_client_send_llrp_response().
 */
etcpal_error_t rdmnet_controller_send_llrp_response(rdmnet_controller_t handle, const LlrpLocalRdmResponse* resp)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_llrp_response(handle->client_handle, resp);
}

/*!
 * \brief Request a client list from a broker.
 *
 * The response will be delivered via the \ref RdmnetControllerCallbacks::client_list_update_received
 * "client_list_update_received" callback.
 *
 * \param[in] handle Handle to the controller from which to request the client list.
 * \param[in] scope_handle Handle to the scope on which to request the client list.
 * \return #kEtcPalErrOk: Request sent successfully.
 * \return #kEtcPalErrInvalid: Invalid argument.
 * \return Other errors forwarded from rdmnet_client_request_client_list().
 */
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
        RDMNET_ASSERT(BROKER_GET_CLIENT_LIST(msg)->client_protocol == kClientProtocolRPT);
        controller->callbacks.client_list_update_received(controller, scope_handle, (client_list_action_t)msg->vector,
                                                          BROKER_GET_RPT_CLIENT_LIST(BROKER_GET_CLIENT_LIST(msg)),
                                                          controller->callback_context);
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
    if (controller->rdm_handle_method == kRdmHandleMethodUseCallbacks)
    {
      controller->rdm_handler.callbacks.llrp_rdm_command_received(controller, cmd, controller->callback_context);
    }
    else
    {
      // TODO
    }
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
        if (controller->rdm_handle_method == kRdmHandleMethodUseCallbacks)
        {
          controller->rdm_handler.callbacks.rdm_command_received(controller, scope_handle, GET_REMOTE_RDM_COMMAND(msg),
                                                                 controller->callback_context);
        }
        else
        {
          // TODO
        }
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
