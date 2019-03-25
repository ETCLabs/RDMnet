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

#include "rdmnet/device.h"

#include "rdmnet/private/opts.h"
#if RDMNET_DYNAMIC_MEM
#include "lwpa/mempool.h"
#else
#include <stdlib.h>
#endif
#include "rdmnet/private/core.h"
#include "rdmnet/private/device.h"

/***************************** Private macros ********************************/

/* Macros for dynamic vs static allocation. Static allocation is done using lwpa_mempool. */
#if RDMNET_DYNAMIC_MEM
#define alloc_rdmnet_device() malloc(sizeof(RdmnetDeviceInternal))
#define free_rdmnet_device(ptr) free(ptr)
#else
#define alloc_rdmnet_device() lwpa_mempool_alloc(rdmnet_devices)
#define free_rdmnet_device(ptr) lwpa_mempool_free(rdmnet_devices, ptr)
#endif

/**************************** Private variables ******************************/

#if !RDMNET_DYNAMIC_MEM
LWPA_MEMPOOL_DEFINE(rdmnet_devices, RdmnetDeviceInternal, RDMNET_MAX_DEVICES);
#endif

/*********************** Private function prototypes *************************/

static void client_connected(rdmnet_client_t handle, const char *scope, void *context);
static void client_disconnected(rdmnet_client_t handle, const char *scope, void *context);
static void client_broker_msg_received(rdmnet_client_t handle, const char *scope, const BrokerMessage *msg,
                                       void *context);
static void client_msg_received(rdmnet_client_t handle, const char *scope, const RptClientMessage *msg, void *context);

static const RptClientCallbacks client_callbacks = {client_connected, client_disconnected};

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_device_create(const RdmnetDeviceConfig *config, rdmnet_device_t *handle)
{
  if (!config || !handle)
    return LWPA_INVALID;

  RdmnetDeviceInternal *new_device = alloc_rdmnet_device();
  if (!new_device)
    return LWPA_NOMEM;

  new_device->callbacks = config->callbacks;
  new_device->callback_context = config->callback_context;

  RdmnetRptClientConfig client_config;
  client_config.type = kRPTClientTypeDevice;
  client_config.has_static_uid = config->has_static_uid;
  client_config.static_uid = config->static_uid;
  client_config.cid = config->cid;
  client_config.scope_list = &config->scope_config;
  client_config.num_scopes = 1;
  client_config.callbacks = client_callbacks;
  client_config.callback_context = new_device;

  lwpa_error_t res = rdmnet_rpt_client_create(&client_config, &new_device->client_handle);
  if (res == LWPA_OK)
  {
    *handle = new_device;
  }
  else
  {
    free_rdmnet_device(new_device);
  }
  return res;
}

void rdmnet_device_destroy(rdmnet_device_t handle)
{
  if (!handle)
    return;

  rdmnet_rpt_client_destroy(handle->client_handle);
  free_rdmnet_device(handle);
}

lwpa_error_t rdmnet_device_send_rdm_response(rdmnet_device_t handle, const DeviceRdmResponse *resp)
{
  return rdmnet_rpt_client_send_rdm_response(handle->client_handle, resp);
}

lwpa_error_t rdmnet_device_send_status(rdmnet_device_t handle, const RptStatusMsg *status)
{
  return rdmnet_rpt_client_send_status(handle->client_handle, status);
}

void client_connected(rdmnet_client_t handle, const char *scope, void *context)
{
  RdmnetDeviceInternal *device = (RdmnetDeviceInternal *)context;
  if (device)
  {
    device->callbacks.connected(device, scope, device->callback_context);
  }
}

void client_disconnected(rdmnet_client_t handle, const char *scope, void *context)
{
  RdmnetDeviceInternal *device = (RdmnetDeviceInternal *)context;
  if (device)
  {
    device->callbacks.disconnected(device, scope, device->callback_context);
  }
}

void client_broker_msg_received(rdmnet_client_t handle, const char *scope, const BrokerMessage *msg, void *context)
{
  lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, "Got Broker message with vector %d", msg->vector);
}

void client_msg_received(rdmnet_client_t handle, const char *scope, const RptClientMessage *msg, void *context)
{
  RdmnetDeviceInternal *device = (RdmnetDeviceInternal *)context;
  if (device)
  {
    if (msg->type == kRptClientMsgRdmCmd)
    {
      device->callbacks.rdm_cmd_received(device, scope, &msg->payload.cmd, device->callback_context);
    }
    else
    {
      lwpa_log(rdmnet_log_params, LWPA_LOG_INFO, "Device incorrectly got non-RDM-command message.");
    }
  }
}
