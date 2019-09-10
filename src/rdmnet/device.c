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

#include "rdmnet/device.h"

#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif
#include "rdmnet/private/core.h"
#include "rdmnet/private/device.h"

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_device() malloc(sizeof(RdmnetDevice))
#define free_rdmnet_device(ptr) free(ptr)
#else
#define alloc_rdmnet_device() etcpal_mempool_alloc(rdmnet_devices)
#define free_rdmnet_device(ptr) etcpal_mempool_free(rdmnet_devices, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
ETCPAL_MEMPOOL_DEFINE(rdmnet_devices, RdmnetDevice, RDMNET_MAX_DEVICES);
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

etcpal_error_t rdmnet_device_init(const EtcPalLogParams* lparams)
{
#if !RDMNET_DYNAMIC_MEM
  etcpal_error_t res = etcpal_mempool_init(rdmnet_devices);
  if (res != kEtcPalErrOk)
    return res;
#endif

  return rdmnet_client_init(lparams);
}

void rdmnet_device_deinit()
{
  rdmnet_client_deinit();
}

etcpal_error_t rdmnet_device_create(const RdmnetDeviceConfig* config, rdmnet_device_t* handle)
{
  if (!config || !handle)
    return kEtcPalErrInvalid;

  RdmnetDevice* new_device = alloc_rdmnet_device();
  if (!new_device)
    return kEtcPalErrNoMem;

  RdmnetRptClientConfig client_config;
  client_config.type = kRPTClientTypeDevice;
  client_config.cid = config->cid;
  client_config.callbacks = client_callbacks;
  client_config.callback_context = new_device;
  client_config.optional = config->optional;
  client_config.llrp_optional = config->llrp_optional;

  etcpal_error_t res = rdmnet_rpt_client_create(&client_config, &new_device->client_handle);
  if (res == kEtcPalErrOk)
  {
    res = rdmnet_client_add_scope(new_device->client_handle, &config->scope_config, &new_device->scope_handle);
    if (res == kEtcPalErrOk)
    {
      // Do the rest of the initialization
      // rdmnet_safe_strncpy(new_device->scope, config->scope_config.scope, E133_SCOPE_STRING_PADDED_LENGTH);
      new_device->callbacks = config->callbacks;
      new_device->callback_context = config->callback_context;

      *handle = new_device;
    }
    else
    {
      rdmnet_client_destroy(new_device->client_handle);
      free_rdmnet_device(new_device);
    }
  }
  else
  {
    free_rdmnet_device(new_device);
  }
  return res;
}

etcpal_error_t rdmnet_device_destroy(rdmnet_device_t handle)
{
  if (!handle)
    return kEtcPalErrInvalid;

  etcpal_error_t res = rdmnet_client_destroy(handle->client_handle);
  if (res == kEtcPalErrOk)
    free_rdmnet_device(handle);

  return res;
}

etcpal_error_t rdmnet_device_send_rdm_response(rdmnet_device_t handle, const LocalRdmResponse* resp)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_rdm_response(handle->client_handle, handle->scope_handle, resp);
}

etcpal_error_t rdmnet_device_send_status(rdmnet_device_t handle, const LocalRptStatus* status)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_status(handle->client_handle, handle->scope_handle, status);
}

etcpal_error_t rdmnet_device_send_llrp_response(rdmnet_device_t handle, const LlrpLocalRdmResponse* resp)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_rpt_client_send_llrp_response(handle->client_handle, resp);
}

etcpal_error_t rdmnet_device_change_scope(rdmnet_device_t handle, const RdmnetScopeConfig* new_scope_config,
                                        rdmnet_disconnect_reason_t reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  rdmnet_client_remove_scope(handle->client_handle, handle->scope_handle, reason);
  return rdmnet_client_add_scope(handle->client_handle, new_scope_config, &handle->scope_handle);
}

etcpal_error_t rdmnet_device_change_search_domain(rdmnet_device_t handle, const char* new_search_domain,
                                                rdmnet_disconnect_reason_t reason)
{
  if (!handle)
    return kEtcPalErrInvalid;

  return rdmnet_client_change_search_domain(handle->client_handle, new_search_domain, reason);
}

void client_connected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RdmnetClientConnectedInfo* info,
                      void* context)
{
  (void)handle;

  RdmnetDevice* device = (RdmnetDevice*)context;
  if (device && scope_handle == device->scope_handle)
  {
    device->callbacks.connected(device, info, device->callback_context);
  }
}

void client_connect_failed(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                           const RdmnetClientConnectFailedInfo* info, void* context)
{
  (void)handle;

  RdmnetDevice* device = (RdmnetDevice*)context;
  if (device && scope_handle == device->scope_handle)
  {
    device->callbacks.connect_failed(device, info, device->callback_context);
  }
}

void client_disconnected(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetClientDisconnectedInfo* info, void* context)
{
  (void)handle;

  RdmnetDevice* device = (RdmnetDevice*)context;
  if (device && scope_handle == device->scope_handle)
  {
    device->callbacks.disconnected(device, info, device->callback_context);
  }
}

void client_broker_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const BrokerMessage* msg,
                                void* context)
{
  (void)handle;
  (void)scope_handle;
  (void)context;

  etcpal_log(rdmnet_log_params, ETCPAL_LOG_INFO, "Got Broker message with vector %d", msg->vector);
}

void client_llrp_msg_received(rdmnet_client_t handle, const LlrpRemoteRdmCommand* cmd, void* context)
{
  (void)handle;

  RdmnetDevice* device = (RdmnetDevice*)context;
  if (device)
  {
    device->callbacks.llrp_rdm_command_received(device, cmd, device->callback_context);
  }
}

void client_msg_received(rdmnet_client_t handle, rdmnet_client_scope_t scope_handle, const RptClientMessage* msg,
                         void* context)
{
  (void)handle;

  RdmnetDevice* device = (RdmnetDevice*)context;
  if (device && scope_handle == device->scope_handle)
  {
    if (msg->type == kRptClientMsgRdmCmd)
    {
      device->callbacks.rdm_command_received(device, &msg->payload.cmd, device->callback_context);
    }
    else
    {
      etcpal_log(rdmnet_log_params, ETCPAL_LOG_INFO, "Device incorrectly got non-RDM-command message.");
    }
  }
}
