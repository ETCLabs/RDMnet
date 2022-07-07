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

#ifndef RDMNET_COMMON_PRIV_H_
#define RDMNET_COMMON_PRIV_H_

#include "rdmnet/controller.h"
#include "rdmnet/core/client.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/core/util.h"
#include "rdmnet/device.h"
#include "rdmnet/ept_client.h"
#include "rdmnet/llrp_manager.h"
#include "rdmnet/llrp_target.h"

/** @cond Internal definitions for RDMnet API */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*RdmnetStructCleanupFunction)(void* instance);

typedef enum
{
  kRdmnetStructTypeController,
  kRdmnetStructTypeDevice,
  kRdmnetStructTypeLlrpManager,
  kRdmnetStructTypeLlrpTarget,
  kRdmnetStructTypeEptClient,
} rdmnet_struct_type_t;

typedef struct RdmnetStructId
{
  int                         handle;
  rdmnet_struct_type_t        type;
  RdmnetStructCleanupFunction cleanup_fn;
} RdmnetStructId;

/******************************************************************************
 * Controller
 *****************************************************************************/

typedef enum
{
  kRdmHandleMethodUseCallbacks,
  kRdmHandleMethodUseData
} rdm_handle_method_t;

#define CONTROLLER_RDM_LABEL_BUF_LENGTH 33

typedef struct ControllerRdmDataInternal
{
  uint16_t model_id;
  uint16_t product_category;
  uint32_t software_version_id;
  char     manufacturer_label[CONTROLLER_RDM_LABEL_BUF_LENGTH];
  char     device_model_description[CONTROLLER_RDM_LABEL_BUF_LENGTH];
  char     software_version_label[CONTROLLER_RDM_LABEL_BUF_LENGTH];
  char     device_label[CONTROLLER_RDM_LABEL_BUF_LENGTH];
  bool     device_label_settable;
} ControllerRdmDataInternal;

typedef struct RdmnetController
{
  RdmnetStructId            id;
  etcpal_mutex_t            lock;
  RdmnetControllerCallbacks callbacks;

  rdm_handle_method_t rdm_handle_method;
  union
  {
    RdmnetControllerRdmCmdHandler handler;
    ControllerRdmDataInternal     data;
  } rdm_handler;

  RCClient client;
} RdmnetController;

#define CONTROLLER_RDM_DATA(controller_ptr) (&(controller_ptr)->rdm_handler.data)

/******************************************************************************
 * Device
 *****************************************************************************/

typedef enum
{
  kDeviceEndpointTypeVirtual = 0,
  kDeviceEndpointTypePhysical = 1
} device_endpoint_type_t;

typedef struct EndpointResponder
{
  EtcPalUuid rid;
  RdmUid     uid;
  RdmUid     binding_uid;
  uint16_t   control_field;
} EndpointResponder;

typedef EndpointResponder* EndpointResponderRef;

typedef struct DeviceEndpoint
{
  uint16_t               id;
  device_endpoint_type_t type;
  uint32_t               responder_list_change_number;
  RC_DECLARE_BUF(EndpointResponderRef, responder_refs, RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT);
} DeviceEndpoint;

#define DEVICE_ENDPOINT_INIT_RESPONDER_REFS(endpoint_ptr, initial_capacity)         \
  RC_INIT_BUF(endpoint_ptr, EndpointResponderRef, responder_refs, initial_capacity, \
              RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT)
#define DEVICE_ENDPOINT_DEINIT_RESPONDER_REFS(endpoint_ptr) RC_DEINIT_BUF(endpoint_ptr, responder_refs)

typedef struct RdmnetDevice
{
  RdmnetStructId        id;
  etcpal_mutex_t        lock;
  RdmnetDeviceCallbacks callbacks;
  rdmnet_client_scope_t scope_handle;

  uint8_t* response_buf;

  uint32_t endpoint_list_change_number;
  RC_DECLARE_BUF(DeviceEndpoint, endpoints, RDMNET_MAX_ENDPOINTS_PER_DEVICE);
  RC_DECLARE_BUF(EndpointResponder, responders, RDMNET_MAX_RESPONDERS_PER_DEVICE);

  RCClient client;
  bool     connected_to_broker;
  uint16_t manufacturer_id;
} RdmnetDevice;

#define DEVICE_INIT_ENDPOINTS(device_ptr, initial_capacity) \
  RC_INIT_BUF(device_ptr, DeviceEndpoint, endpoints, initial_capacity, RDMNET_MAX_ENDPOINTS_PER_DEVICE)
#define DEVICE_DEINIT_ENDPOINTS(device_ptr) RC_DEINIT_BUF(device_ptr, endpoints)
#define DEVICE_CHECK_ENDPOINTS_CAPACITY(device_ptr, num_additional) \
  RC_CHECK_BUF_CAPACITY(device_ptr, DeviceEndpoint, endpoints, RDMNET_MAX_ENDPOINTS_PER_DEVICE, num_additional)

#define DEVICE_INIT_RESPONDERS(device_ptr, initial_capacity) \
  RC_INIT_BUF(device_ptr, EndpointResponder, responders, initial_capacity, RDMNET_MAX_RESPONDERS_PER_DEVICE)
#define DEVICE_DEINIT_RESPONDERS(device_ptr) RC_DEINIT_BUF(device_ptr, responders)
#define DEVICE_CHECK_RESPONDERS_CAPACITY(device_ptr, endpoint_ptr, num_additional)                    \
  (RC_CHECK_BUF_CAPACITY(device_ptr, EndpointResponder, responders, RDMNET_MAX_RESPONDERS_PER_DEVICE, \
                         num_additional) &&                                                           \
   RC_CHECK_BUF_CAPACITY(endpoint_ptr, EndpointResponderRef, responder_refs,                          \
                         RDMNET_MAX_RESPONDERS_PER_DEVICE_ENDPOINT, num_additional))

/******************************************************************************
 * LLRP Manager
 *****************************************************************************/

typedef struct LlrpManager
{
  RdmnetStructId       id;
  etcpal_mutex_t       lock;
  LlrpManagerCallbacks callbacks;

  RCLlrpManager rc_manager;
} LlrpManager;

/******************************************************************************
 * LLRP Target
 *****************************************************************************/

typedef struct LlrpTarget
{
  RdmnetStructId      id;
  etcpal_mutex_t      lock;
  LlrpTargetCallbacks callbacks;
  uint8_t*            response_buf;

  RCLlrpTarget rc_target;
} LlrpTarget;

/******************************************************************************
 * EPT client
 *****************************************************************************/

typedef struct RdmnetEptClient
{
  RdmnetStructId           id;
  etcpal_mutex_t           lock;
  RdmnetEptClientCallbacks callbacks;

  RCClient client;
  bool     connected_to_broker;
} RdmnetEptClient;

RdmnetController* rdmnet_alloc_controller_instance(void);
RdmnetDevice*     rdmnet_alloc_device_instance(void);
LlrpManager*      rdmnet_alloc_llrp_manager_instance(void);
LlrpTarget*       rdmnet_alloc_llrp_target_instance(void);
RdmnetEptClient*  rdmnet_alloc_ept_client_instance(void);

void* rdmnet_find_struct_instance(int handle, rdmnet_struct_type_t type);
void  rdmnet_unregister_struct_instance(void* instance);
void  rdmnet_free_struct_instance(void* instance);

#ifdef __cplusplus
}
#endif

/** @endcond */

#endif /* RDMNET_COMMON_PRIV_H_ */
