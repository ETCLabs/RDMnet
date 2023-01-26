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

#include "rdmnet/common.h"

#include "etcpal/common.h"
#include "etcpal/handle_manager.h"
#include "rdmnet/common_priv.h"
#include "rdmnet/core/common.h"
#include "rdmnet/core/opts.h"

#if RDMNET_DYNAMIC_MEM
#include <stdlib.h>
#else
#include "etcpal/mempool.h"
#endif

/**************************** Private constants ******************************/

#define MAX_RESPONDERS (RDMNET_MAX_DEVICES * RDMNET_MAX_RESPONDERS_PER_DEVICE)
#define MAX_RB_NODES \
  (RDMNET_MAX_CONTROLLERS + RDMNET_MAX_DEVICES + MAX_RESPONDERS + RDMNET_MAX_LLRP_TARGETS + RDMNET_MAX_EPT_CLIENTS)

#define DEVICE_INITIAL_BUFFER_CAPACITY 4

/***************************** Private macros ********************************/

// Macros for dynamic vs static allocation. Static allocation is done using etcpal_mempool.
#if RDMNET_DYNAMIC_MEM
#define ALLOC_RDMNET_CONTROLLER() (RdmnetController*)malloc(sizeof(RdmnetController))
#define ALLOC_RDMNET_DEVICE() (RdmnetDevice*)malloc(sizeof(RdmnetDevice))
#define ALLOC_ENDPOINT_RESPONDER() (EndpointResponder*)malloc(sizeof(EndpointResponder))
#define ALLOC_LLRP_MANAGER() (LlrpManager*)malloc(sizeof(LlrpManager))
#define ALLOC_LLRP_TARGET() (LlrpTarget*)malloc(sizeof(LlrpTarget))
#define ALLOC_RDMNET_EPT_CLIENT() (RdmnetEptClient*)malloc(sizeof(RdmnetEptClient))
#define FREE_RDMNET_CONTROLLER(ptr) \
  if (RDMNET_ASSERT_VERIFY(ptr))    \
  {                                 \
    free(ptr);                      \
  }
#define FREE_RDMNET_DEVICE(ptr)  \
  if (RDMNET_ASSERT_VERIFY(ptr)) \
  {                              \
    free(ptr);                   \
  }
#define FREE_ENDPOINT_RESPONDER(ptr) \
  if (RDMNET_ASSERT_VERIFY(ptr))     \
  {                                  \
    free(ptr);                       \
  }
#define FREE_LLRP_MANAGER(ptr)   \
  if (RDMNET_ASSERT_VERIFY(ptr)) \
  {                              \
    free(ptr);                   \
  }
#define FREE_LLRP_TARGET(ptr)    \
  if (RDMNET_ASSERT_VERIFY(ptr)) \
  {                              \
    free(ptr);                   \
  }
#define FREE_RDMNET_EPT_CLIENT(ptr) \
  if (RDMNET_ASSERT_VERIFY(ptr))    \
  {                                 \
    free(ptr);                      \
  }
#else
#if RDMNET_MAX_CONTROLLERS
#define ALLOC_RDMNET_CONTROLLER() (RdmnetController*)etcpal_mempool_alloc(rdmnet_controllers)
#define FREE_RDMNET_CONTROLLER(ptr)               \
  if (RDMNET_ASSERT_VERIFY(ptr))                  \
  {                                               \
    etcpal_mempool_free(rdmnet_controllers, ptr); \
  }
#else
#define ALLOC_RDMNET_CONTROLLER() NULL
#define FREE_RDMNET_CONTROLLER(ptr)
#endif

#if RDMNET_MAX_DEVICES
#define ALLOC_RDMNET_DEVICE() (RdmnetDevice*)etcpal_mempool_alloc(rdmnet_devices)
#define FREE_RDMNET_DEVICE(ptr)               \
  if (RDMNET_ASSERT_VERIFY(ptr))              \
  {                                           \
    etcpal_mempool_free(rdmnet_devices, ptr); \
  }
#else
#define ALLOC_RDMNET_DEVICE() NULL
#define FREE_RDMNET_DEVICE(ptr)
#endif

#if MAX_RESPONDERS
#define ALLOC_ENDPOINT_RESPONDER() (EndpointResponder*)etcpal_mempool_alloc(endpoint_responders)
#define FREE_ENDPOINT_RESPONDER(ptr)               \
  if (RDMNET_ASSERT_VERIFY(ptr))                   \
  {                                                \
    etcpal_mempool_free(endpoint_responders, ptr); \
  }
#else
#define ALLOC_ENDPOINT_RESPONDER() NULL
#define FREE_ENDPOINT_RESPONDER(ptr)
#endif

#define ALLOC_LLRP_MANAGER() NULL
#define FREE_LLRP_MANAGER(ptr)

#if RDMNET_MAX_LLRP_TARGETS
#define ALLOC_LLRP_TARGET() (LlrpTarget*)etcpal_mempool_alloc(llrp_targets)
#define FREE_LLRP_TARGET(ptr)               \
  if (RDMNET_ASSERT_VERIFY(ptr))            \
  {                                         \
    etcpal_mempool_free(llrp_targets, ptr); \
  }
#else
#define ALLOC_LLRP_TARGET() NULL
#define FREE_LLRP_TARGET(ptr)
#endif

#if RDMNET_MAX_EPT_CLIENTS
#define ALLOC_RDMNET_EPT_CLIENT() (RdmnetEptClient) etcpal_mempool_alloc(ept_clients)
#define FREE_RDMNET_EPT_CLIENT(ptr)        \
  if (RDMNET_ASSERT_VERIFY(ptr))           \
  {                                        \
    etcpal_mempool_free(ept_clients, ptr); \
  }
#else
#define ALLOC_RDMNET_EPT_CLIENT() NULL
#define FREE_RDMNET_EPT_CLIENT(ptr)
#endif
#endif

/**************************** Private variables ******************************/

static bool            tick_thread_running;
static etcpal_thread_t tick_thread;

#if !RDMNET_DYNAMIC_MEM
#if RDMNET_MAX_CONTROLLERS
ETCPAL_MEMPOOL_DEFINE(rdmnet_controllers, RdmnetController, RDMNET_MAX_CONTROLLERS);
#endif
#if RDMNET_MAX_DEVICES
ETCPAL_MEMPOOL_DEFINE(rdmnet_devices, RdmnetDevice, RDMNET_MAX_DEVICES);
#endif
#if MAX_RESPONDERS
ETCPAL_MEMPOOL_DEFINE(endpoint_responders, EndpointResponder, MAX_RESPONDERS);
#endif
#if RDMNET_MAX_LLRP_TARGETS
ETCPAL_MEMPOOL_DEFINE(llrp_targets, LlrpTarget, RDMNET_MAX_LLRP_TARGETS);
#endif
#if RDMNET_MAX_EPT_CLIENTS
ETCPAL_MEMPOOL_DEFINE(ept_clients, RdmnetEptClient, RDMNET_MAX_EPT_CLIENTS);
#endif
ETCPAL_MEMPOOL_DEFINE(rb_nodes, EtcPalRbNode, MAX_RB_NODES);
#endif

EtcPalRbTree            handles;
static IntHandleManager handle_manager;

/*********************** Private function prototypes *************************/

static void rdmnet_tick_thread(void* arg);

static int           handle_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static int           responder_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b);
static EtcPalRbNode* node_alloc(void);
static void          node_dealloc(EtcPalRbNode* node);
static bool          handle_in_use(int handle_val, void* context);

static void free_controller_resources(RdmnetController* controller);
static void free_device_resources(RdmnetDevice* device);
static void free_llrp_manager_resources(LlrpManager* manager);
static void free_llrp_target_resources(LlrpTarget* target);
static void free_ept_client_resources(RdmnetEptClient* ept_client);

static void tree_clear_cb(const EtcPalRbTree* self, EtcPalRbNode* node);
static void endpoint_responders_remove_cb(const EtcPalRbTree* self, EtcPalRbNode* node);

static etcpal_error_t add_static_responder(DeviceEndpoint* endpoint, const RdmUid* uid);
static etcpal_error_t add_dynamic_responder(DeviceEndpoint* endpoint, uint16_t manufacturer_id, const EtcPalUuid* rid);
static etcpal_error_t add_physical_responder(DeviceEndpoint*                        endpoint,
                                             const RdmnetPhysicalEndpointResponder* responder_config);

#if !RDMNET_DYNAMIC_MEM
static bool check_static_responder_limits(RdmnetDevice* device, DeviceEndpoint* endpoint, size_t num_additional);
#endif

/*************************** Function definitions ****************************/

/**
 * @brief Initialize the RDMnet library.
 *
 * Does all initialization required before the RDMnet API modules can be used. Starts the message
 * dispatch thread.
 *
 * @param[in] log_params Optional: log parameters for the RDMnet library to use to log messages. If
 *                       NULL, no logging will be performed.
 * @param[in] netint_config Optional: a set of network interfaces to which to restrict multicast
 *                          operation.
 * @return #kEtcPalErrOk: Initialization successful.
 * @return #kEtcPalErrInvalid: Invalid argument.
 * @return #kEtcPalErrNoNetints: No network interfaces found on the system.
 * @return #kEtcPalErrSys: An internal library or system call error occurred.
 * @return Other error codes are possible from the initialization of EtcPal.
 */
etcpal_error_t rdmnet_init(const EtcPalLogParams* log_params, const RdmnetNetintConfig* netint_config)
{
  etcpal_error_t res = kEtcPalErrOk;

#if !RDMNET_DYNAMIC_MEM
#if RDMNET_MAX_CONTROLLERS
  res |= etcpal_mempool_init(rdmnet_controllers);
#endif
#if RDMNET_MAX_DEVICES
  res |= etcpal_mempool_init(rdmnet_devices);
#endif
#if MAX_RESPONDERS
  res |= etcpal_mempool_init(endpoint_responders);
#endif
#if RDMNET_MAX_LLRP_TARGETS
  res |= etcpal_mempool_init(llrp_targets);
#endif
#if RDMNET_MAX_EPT_CLIENTS
  res |= etcpal_mempool_init(ept_clients);
#endif
  res |= etcpal_mempool_init(rb_nodes);
  if (res != kEtcPalErrOk)
    return res;
#endif

  res = rc_init(log_params, netint_config);
  if (res != kEtcPalErrOk)
    return res;

  EtcPalThreadParams thread_params;
  thread_params.priority = RDMNET_TICK_THREAD_PRIORITY;
  thread_params.stack_size = RDMNET_TICK_THREAD_STACK;
  thread_params.thread_name = "RDMnet thread";
  thread_params.platform_data = NULL;
  tick_thread_running = true;
  res = etcpal_thread_create(&tick_thread, &thread_params, rdmnet_tick_thread, NULL);

  if (res == kEtcPalErrOk)
  {
    etcpal_rbtree_init(&handles, handle_compare, node_alloc, node_dealloc);
    init_int_handle_manager(&handle_manager, -1, handle_in_use, NULL);
  }
  else
  {
    rc_deinit();
  }
  return res;
}

/**
 * @brief Deinitialize the RDMnet library.
 *
 * Closes all connections, deallocates all resources and joins the background thread. No RDMnet API
 * functions are usable after this function is called.
 */
void rdmnet_deinit(void)
{
  tick_thread_running = false;
  etcpal_thread_join(&tick_thread);

  rc_deinit();

  etcpal_rbtree_clear_with_cb(&handles, tree_clear_cb);
}

// clang-format off
static const char* kRptStatusCodeStrings[] =
{
  "Invalid RPT Status code",
  "Destination RPT UID not found",
  "Timeout waiting for RDM response from responder",
  "Invalid RDM response received from responder",
  "Destination RDM UID not found",
  "Destination endpoint not found",
  "Broadcast complete",
  "Unknown RPT vector",
  "Malformed RPT message",
  "Invalid RDM command class"
};
#define NUM_RPT_STATUS_CODE_STRINGS (sizeof(kRptStatusCodeStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string representation of an RPT status code.
 */
const char* rdmnet_rpt_status_code_to_string(rpt_status_code_t code)
{
  if (code >= 0 && code < NUM_RPT_STATUS_CODE_STRINGS)
    return kRptStatusCodeStrings[code];
  return "Invalid RPT Status code";
}

// clang-format off
static const char* kEptStatusCodeStrings[] =
{
  "Destination CID not found",
  "Unknown EPT vector"
};
#define NUM_EPT_STATUS_CODE_STRINGS (sizeof(kEptStatusCodeStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string representation of an EPT status code.
 */
const char* rdmnet_ept_status_code_to_string(ept_status_code_t code)
{
  if (code >= 0 && code < NUM_EPT_STATUS_CODE_STRINGS)
    return kEptStatusCodeStrings[code];
  return "Invalid EPT Status code";
}

// clang-format off
static const char* kRdmnetConnectFailEventStrings[] =
{
  "Socket failure on connection initiation",
  "TCP connection failure",
  "No reply received to RDMnet handshake",
  "RDMnet connection rejected"
};
#define NUM_CONNECT_FAIL_EVENT_STRINGS (sizeof(kRdmnetConnectFailEventStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string description of an RDMnet connection failure event.
 *
 * An RDMnet connection failure event provides a high-level reason why an RDMnet connection failed.
 *
 * @param event Event code.
 * @return String, or a placeholder string if event is invalid.
 */
const char* rdmnet_connect_fail_event_to_string(rdmnet_connect_fail_event_t event)
{
  if (event >= 0 && event < NUM_CONNECT_FAIL_EVENT_STRINGS)
    return kRdmnetConnectFailEventStrings[event];
  return "Invalid connect fail event";
}

// clang-format off
static const char* kRdmnetDisconnectEventStrings[] =
{
  "Connection was closed abruptly",
  "No heartbeat message was received within the heartbeat timeout",
  "Connection was redirected to another Broker",
  "Remote component sent a disconnect message",
  "Local component sent a disconnect message"
};
#define NUM_DISCONNECT_EVENT_STRINGS (sizeof(kRdmnetDisconnectEventStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string description of an RDMnet disconnect event.
 *
 * An RDMnet disconnect event provides a high-level reason why an RDMnet connection was
 * disconnected.
 *
 * @param event Event code.
 * @return String, or NULL if event is invalid.
 */
const char* rdmnet_disconnect_event_to_string(rdmnet_disconnect_event_t event)
{
  if (event >= 0 && event < NUM_DISCONNECT_EVENT_STRINGS)
    return kRdmnetDisconnectEventStrings[event];
  return "Invalid disconnect event";
}

// clang-format off
static const char* kRdmnetConnectStatusStrings[] =
{
  "Successful connection",
  "Broker/Client scope mismatch",
  "Broker connection capacity exceeded",
  "Duplicate UID detected",
  "Invalid client entry",
  "Invalid UID"
};
#define NUM_CONNECT_STATUS_STRINGS (sizeof(kRdmnetConnectStatusStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string description of an RDMnet connect status code.
 *
 * Connect status codes are returned by a broker in a connect reply message after a client attempts
 * to connect.
 *
 * @param code Connect status code.
 * @return String, or NULL if code is invalid.
 */
const char* rdmnet_connect_status_to_string(rdmnet_connect_status_t code)
{
  if (code >= 0 && code < NUM_CONNECT_STATUS_STRINGS)
    return kRdmnetConnectStatusStrings[code];
  return "Invalid connect status code";
}

// clang-format off
static const char* kRdmnetDisconnectReasonStrings[] =
{
  "Component shutting down",
  "Component can no longer support this connection",
  "Hardware fault",
  "Software fault",
  "Software reset",
  "Incorrect scope",
  "Component reconfigured via RPT",
  "Component reconfigured via LLRP",
  "Component reconfigured by non-RDMnet method"
};
#define NUM_DISCONNECT_REASON_STRINGS (sizeof(kRdmnetDisconnectReasonStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string description of an RDMnet disconnect reason code.
 *
 * Disconnect reason codes are sent by a broker or client that is disconnecting.
 *
 * @param code Disconnect reason code.
 * @return String, or NULL if code is invalid.
 */
const char* rdmnet_disconnect_reason_to_string(rdmnet_disconnect_reason_t code)
{
  if (code >= 0 && code < NUM_DISCONNECT_REASON_STRINGS)
    return kRdmnetDisconnectReasonStrings[code];
  return "Invalid disconnect reason code";
}

// clang-format off
static const char* kRdmnetDynamicUidStatusStrings[] =
{
  "Dynamic UID fetched or assigned successfully",
  "The Dynamic UID request was malformed",
  "The requested Dynamic UID was not found",
  "This RID has already been assigned a Dynamic UID",
  "Dynamic UID capacity exhausted"
};
#define NUM_DYNAMIC_UID_STATUS_STRINGS (sizeof(kRdmnetDynamicUidStatusStrings) / sizeof(const char*))
// clang-format on

/**
 * @brief Get a string description of an RDMnet Dynamic UID status code.
 *
 * Dynamic UID status codes are returned by a broker in response to a request for dynamic UIDs by a
 * client.
 *
 * @param code Dynamic UID status code.
 * @return String, or NULL if code is invalid.
 */
const char* rdmnet_dynamic_uid_status_to_string(rdmnet_dynamic_uid_status_t code)
{
  if (code >= 0 && code < NUM_DYNAMIC_UID_STATUS_STRINGS)
    return kRdmnetDynamicUidStatusStrings[code];
  return "Invalid Dynamic UID status code";
}

/******************************************************************************
 * Internal definitions
 *****************************************************************************/

RdmnetController* rdmnet_alloc_controller_instance(void)
{
  int new_handle = get_next_int_handle(&handle_manager);
  if (new_handle == -1)
    return NULL;

  RdmnetController* new_controller = ALLOC_RDMNET_CONTROLLER();
  if (!new_controller)
    return NULL;

  memset(new_controller, 0, sizeof(RdmnetController));

  if (!etcpal_mutex_create(&new_controller->lock))
  {
    FREE_RDMNET_CONTROLLER(new_controller);
    return NULL;
  }

  new_controller->id.handle = new_handle;
  new_controller->id.type = kRdmnetStructTypeController;
  etcpal_error_t res = etcpal_rbtree_insert(&handles, new_controller);
  if (res != kEtcPalErrOk)
  {
    etcpal_mutex_destroy(&new_controller->lock);
    FREE_RDMNET_CONTROLLER(new_controller);
    return NULL;
  }
  return new_controller;
}

RdmnetDevice* rdmnet_alloc_device_instance(void)
{
  int new_handle = get_next_int_handle(&handle_manager);
  if (new_handle == -1)
    return NULL;

  RdmnetDevice* new_device = ALLOC_RDMNET_DEVICE();
  if (new_device)
  {
    memset(new_device, 0, sizeof(RdmnetDevice));

    if (etcpal_mutex_create(&new_device->lock))
    {
      if (DEVICE_INIT_ENDPOINTS(new_device, DEVICE_INITIAL_BUFFER_CAPACITY))
      {
        new_device->id.handle = new_handle;
        new_device->id.type = kRdmnetStructTypeDevice;

        if (etcpal_rbtree_insert(&handles, new_device) == kEtcPalErrOk)
          return new_device;
      }
      etcpal_mutex_destroy(&new_device->lock);
    }
    FREE_RDMNET_DEVICE(new_device);
  }

  return NULL;
}

LlrpManager* rdmnet_alloc_llrp_manager_instance(void)
{
  int new_handle = get_next_int_handle(&handle_manager);
  if (new_handle == -1)
    return NULL;

  LlrpManager* new_manager = ALLOC_LLRP_MANAGER();
  if (!new_manager)
    return NULL;

  memset(new_manager, 0, sizeof(LlrpManager));

  if (!etcpal_mutex_create(&new_manager->lock))
  {
    FREE_LLRP_MANAGER(new_manager);
    return NULL;
  }

  new_manager->id.handle = new_handle;
  new_manager->id.type = kRdmnetStructTypeLlrpManager;
  etcpal_error_t res = etcpal_rbtree_insert(&handles, new_manager);
  if (res != kEtcPalErrOk)
  {
    etcpal_mutex_destroy(&new_manager->lock);
    FREE_LLRP_MANAGER(new_manager);
    return NULL;
  }
  return new_manager;
}

LlrpTarget* rdmnet_alloc_llrp_target_instance(void)
{
  int new_handle = get_next_int_handle(&handle_manager);
  if (new_handle == -1)
    return NULL;

  LlrpTarget* new_target = ALLOC_LLRP_TARGET();
  if (!new_target)
    return NULL;

  memset(new_target, 0, sizeof(LlrpTarget));

  if (!etcpal_mutex_create(&new_target->lock))
  {
    FREE_LLRP_TARGET(new_target);
    return NULL;
  }

  new_target->id.handle = new_handle;
  new_target->id.type = kRdmnetStructTypeLlrpTarget;
  etcpal_error_t res = etcpal_rbtree_insert(&handles, new_target);
  if (res != kEtcPalErrOk)
  {
    etcpal_mutex_destroy(&new_target->lock);
    FREE_LLRP_TARGET(new_target);
    return NULL;
  }
  return new_target;
}

void rdmnet_unregister_struct_instance(void* instance)
{
  if (!RDMNET_ASSERT_VERIFY(instance))
    return;

  etcpal_rbtree_remove(&handles, instance);
}

void rdmnet_free_struct_instance(void* instance)
{
  if (!RDMNET_ASSERT_VERIFY(instance))
    return;

  RdmnetStructId* id = (RdmnetStructId*)instance;
  switch (id->type)
  {
    case kRdmnetStructTypeController:
      free_controller_resources((RdmnetController*)id);
      break;
    case kRdmnetStructTypeDevice:
      free_device_resources((RdmnetDevice*)id);
      break;
    case kRdmnetStructTypeLlrpManager:
      free_llrp_manager_resources((LlrpManager*)id);
      break;
    case kRdmnetStructTypeLlrpTarget:
      free_llrp_target_resources((LlrpTarget*)id);
      break;
    case kRdmnetStructTypeEptClient:
      free_ept_client_resources((RdmnetEptClient*)id);
    default:
      break;
  }
}

void rdmnet_init_endpoints(DeviceEndpoint* endpoints, size_t num_endpoints)
{
  for (DeviceEndpoint* endpoint = endpoints; endpoint < (endpoints + num_endpoints); ++endpoint)
  {
    if (!RDMNET_ASSERT_VERIFY(endpoint))
      return;

    etcpal_rbtree_init(&endpoint->responders, responder_compare, node_alloc, node_dealloc);
  }
}

void rdmnet_deinit_endpoints(DeviceEndpoint* endpoints, size_t num_endpoints)
{
  for (DeviceEndpoint* endpoint = endpoints; endpoint < (endpoints + num_endpoints); ++endpoint)
  {
    if (!RDMNET_ASSERT_VERIFY(endpoint))
      return;

    etcpal_rbtree_clear_with_cb(&endpoint->responders, endpoint_responders_remove_cb);
  }
}

etcpal_error_t rdmnet_add_static_responders(RdmnetDevice*   device,
                                            DeviceEndpoint* endpoint,
                                            const RdmUid*   uids,
                                            size_t          num_uids)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

#if RDMNET_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(device);
#else
  if (!RDMNET_ASSERT_VERIFY(device))
    return kEtcPalErrSys;

  if (!check_static_responder_limits(device, endpoint, num_uids))
    return kEtcPalErrNoMem;
#endif

  size_t num_added = 0;
  while ((num_added < num_uids) && (res == kEtcPalErrOk))
  {
    if (!RDMNET_ASSERT_VERIFY(uids))
      return kEtcPalErrSys;

    res = add_static_responder(endpoint, &uids[num_added]);
#if !RDMNET_DYNAMIC_MEM
    if (!RDMNET_ASSERT_VERIFY(res != kEtcPalErrNoMem))
      return kEtcPalErrSys;
#endif
    if (res == kEtcPalErrOk)
      ++num_added;
  }

  if (res != kEtcPalErrOk)
    rdmnet_remove_responders_by_uid(endpoint, uids, num_added);

  return res;
}

etcpal_error_t rdmnet_add_dynamic_responders(RdmnetDevice*     device,
                                             DeviceEndpoint*   endpoint,
                                             uint16_t          manufacturer_id,
                                             const EtcPalUuid* rids,
                                             size_t            num_rids)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

#if RDMNET_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(device);
#else
  if (!RDMNET_ASSERT_VERIFY(device))
    return kEtcPalErrSys;

  if (!check_static_responder_limits(device, endpoint, num_rids))
    return kEtcPalErrNoMem;
#endif

  size_t num_added = 0;
  while ((num_added < num_rids) && (res == kEtcPalErrOk))
  {
    if (!RDMNET_ASSERT_VERIFY(rids))
      return kEtcPalErrSys;

    res = add_dynamic_responder(endpoint, manufacturer_id, &rids[num_added]);
#if !RDMNET_DYNAMIC_MEM
    if (!RDMNET_ASSERT_VERIFY(res != kEtcPalErrNoMem))
      return kEtcPalErrSys;
#endif
    if (res == kEtcPalErrOk)
      ++num_added;
  }

  if (res != kEtcPalErrOk)
    rdmnet_remove_responders_by_rid(endpoint, rids, num_added);

  return res;
}

etcpal_error_t rdmnet_add_physical_responders(RdmnetDevice*                          device,
                                              DeviceEndpoint*                        endpoint,
                                              const RdmnetPhysicalEndpointResponder* responders,
                                              size_t                                 num_responders)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint))
    return kEtcPalErrSys;

  etcpal_error_t res = kEtcPalErrOk;

#if RDMNET_DYNAMIC_MEM
  ETCPAL_UNUSED_ARG(device);
#else
  if (!RDMNET_ASSERT_VERIFY(device))
    return kEtcPalErrSys;

  if (!check_static_responder_limits(device, endpoint, num_responders))
    return kEtcPalErrNoMem;
#endif

  size_t num_added = 0;
  while ((num_added < num_responders) && (res == kEtcPalErrOk))
  {
    if (!RDMNET_ASSERT_VERIFY(responders))
      return kEtcPalErrSys;

    res = add_physical_responder(endpoint, &responders[num_added]);
#if !RDMNET_DYNAMIC_MEM
    if (!RDMNET_ASSERT_VERIFY(res != kEtcPalErrNoMem))
      return kEtcPalErrSys;
#endif
    if (res == kEtcPalErrOk)
      ++num_added;
  }

  if (res != kEtcPalErrOk)
  {
    for (size_t i = 0; i < num_added; ++i)
    {
      if (!RDMNET_ASSERT_VERIFY(responders))
        return kEtcPalErrSys;

      rdmnet_remove_responders_by_uid(endpoint, &responders[i].uid, 1);
    }
  }

  return res;
}

EndpointResponder* rdmnet_find_responder_by_rid(DeviceEndpoint* endpoint, const EtcPalUuid* rid)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint) || !RDMNET_ASSERT_VERIFY(rid))
    return NULL;

  EndpointResponder search_key;
  memcpy(search_key.rid.data, rid->data, ETCPAL_UUID_BYTES);  // Non-NULL RID means UIDs are ignored
  return (EndpointResponder*)etcpal_rbtree_find(&endpoint->responders, &search_key);
}

EndpointResponder* rdmnet_find_responder_by_uid(DeviceEndpoint* endpoint, const RdmUid* uid)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint) || !RDMNET_ASSERT_VERIFY(uid))
    return NULL;

  EndpointResponder search_key;
  search_key.rid = kEtcPalNullUuid;  // Set RID to NULL to cause a search by UID
  search_key.uid = *uid;
  return (EndpointResponder*)etcpal_rbtree_find(&endpoint->responders, &search_key);
}

void rdmnet_remove_responders_by_rid(DeviceEndpoint* endpoint, const EtcPalUuid* rids, size_t num_rids)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint))
    return;

  for (const EtcPalUuid* rid = rids; rid < rids + num_rids; ++rid)
  {
    if (!RDMNET_ASSERT_VERIFY(rid))
      return;

    EndpointResponder search_key;
    memcpy(search_key.rid.data, rid->data, ETCPAL_UUID_BYTES);  // Non-NULL RID means UIDs are ignored
    etcpal_rbtree_remove_with_cb(&endpoint->responders, &search_key, endpoint_responders_remove_cb);
  }
}

void rdmnet_remove_responders_by_uid(DeviceEndpoint* endpoint, const RdmUid* uids, size_t num_uids)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint))
    return;

  for (const RdmUid* uid = uids; uid < uids + num_uids; ++uid)
  {
    if (!RDMNET_ASSERT_VERIFY(uid))
      return;

    EndpointResponder search_key;
    search_key.rid = kEtcPalNullUuid;  // Set RID to NULL to cause a search by UID
    search_key.uid = *uid;
    etcpal_rbtree_remove_with_cb(&endpoint->responders, &search_key, endpoint_responders_remove_cb);
  }
}

void* rdmnet_find_struct_instance(int handle, rdmnet_struct_type_t type)
{
  RdmnetStructId* id = (RdmnetStructId*)etcpal_rbtree_find(&handles, &handle);
  if (id && id->type == type)
    return id;
  return NULL;
}

void rdmnet_tick_thread(void* arg)
{
  ETCPAL_UNUSED_ARG(arg);
  while (tick_thread_running)
  {
    rc_tick();
  }
}

int handle_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  if (!RDMNET_ASSERT_VERIFY(value_a) || !RDMNET_ASSERT_VERIFY(value_b))
    return 0;

  const RdmnetStructId* a = (const RdmnetStructId*)value_a;
  const RdmnetStructId* b = (const RdmnetStructId*)value_b;
  return (a->handle > b->handle) - (a->handle < b->handle);
}

int responder_compare(const EtcPalRbTree* self, const void* value_a, const void* value_b)
{
  ETCPAL_UNUSED_ARG(self);

  if (!RDMNET_ASSERT_VERIFY(value_a) || !RDMNET_ASSERT_VERIFY(value_b))
    return 0;

  const EndpointResponder* a = (const EndpointResponder*)value_a;
  const EndpointResponder* b = (const EndpointResponder*)value_b;

  int res = memcmp(a->rid.data, b->rid.data, ETCPAL_UUID_BYTES);

  // Only compare UIDs when both RIDs are NULL.
  if ((res == 0) && ETCPAL_UUID_IS_NULL(&a->rid))
  {
    res = (a->uid.manu > b->uid.manu) - (a->uid.manu < b->uid.manu);
    if ((res == 0))
      res = (a->uid.id > b->uid.id) - (a->uid.id < b->uid.id);
  }

  return res;
}

EtcPalRbNode* node_alloc(void)
{
#if RDMNET_DYNAMIC_MEM
  return (EtcPalRbNode*)malloc(sizeof(EtcPalRbNode));
#else
  return etcpal_mempool_alloc(rb_nodes);
#endif
}

void node_dealloc(EtcPalRbNode* node)
{
  if (!RDMNET_ASSERT_VERIFY(node))
    return;

#if RDMNET_DYNAMIC_MEM
  free(node);
#else
  etcpal_mempool_free(rb_nodes, node);
#endif
}

bool handle_in_use(int handle_val, void* context)
{
  ETCPAL_UNUSED_ARG(context);
  RdmnetStructId id;
  id.handle = handle_val;
  return (etcpal_rbtree_find(&handles, &id) != NULL);
}

void free_controller_resources(RdmnetController* controller)
{
  if (!RDMNET_ASSERT_VERIFY(controller))
    return;

  etcpal_mutex_destroy(&controller->lock);
  FREE_RDMNET_CONTROLLER(controller);
}

void free_device_resources(RdmnetDevice* device)
{
  if (!RDMNET_ASSERT_VERIFY(device))
    return;

  etcpal_mutex_destroy(&device->lock);
  rdmnet_deinit_endpoints(device->endpoints, device->num_endpoints);

  DEVICE_DEINIT_ENDPOINTS(device);
  FREE_RDMNET_DEVICE(device);
}

void free_llrp_manager_resources(LlrpManager* manager)
{
  if (!RDMNET_ASSERT_VERIFY(manager))
    return;

  etcpal_mutex_destroy(&manager->lock);
  FREE_LLRP_MANAGER(manager);
}

void free_llrp_target_resources(LlrpTarget* target)
{
  if (!RDMNET_ASSERT_VERIFY(target))
    return;

  etcpal_mutex_destroy(&target->lock);
  FREE_LLRP_TARGET(target);
}

void free_ept_client_resources(RdmnetEptClient* ept_client)
{
  if (!RDMNET_ASSERT_VERIFY(ept_client))
    return;

  etcpal_mutex_destroy(&ept_client->lock);
  FREE_RDMNET_EPT_CLIENT(ept_client);
}

void tree_clear_cb(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!RDMNET_ASSERT_VERIFY(node))
    return;

  rdmnet_free_struct_instance(node->value);
  node_dealloc(node);
}

void endpoint_responders_remove_cb(const EtcPalRbTree* self, EtcPalRbNode* node)
{
  ETCPAL_UNUSED_ARG(self);

  if (!RDMNET_ASSERT_VERIFY(node))
    return;

  FREE_ENDPOINT_RESPONDER(node->value);
  node_dealloc(node);
}

etcpal_error_t add_static_responder(DeviceEndpoint* endpoint, const RdmUid* uid)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint) || !RDMNET_ASSERT_VERIFY(uid))
    return kEtcPalErrSys;

  EndpointResponder* responder = ALLOC_ENDPOINT_RESPONDER();
  if (!responder)
    return kEtcPalErrNoMem;

  responder->rid = kEtcPalNullUuid;
  responder->uid = *uid;
  return etcpal_rbtree_insert(&endpoint->responders, responder);
}

etcpal_error_t add_dynamic_responder(DeviceEndpoint* endpoint, uint16_t manufacturer_id, const EtcPalUuid* rid)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint) || !RDMNET_ASSERT_VERIFY(rid))
    return kEtcPalErrSys;

  EndpointResponder* responder = ALLOC_ENDPOINT_RESPONDER();
  if (!responder)
    return kEtcPalErrNoMem;

  responder->rid = *rid;
  RDMNET_INIT_DYNAMIC_UID_REQUEST(&responder->uid, manufacturer_id);
  return etcpal_rbtree_insert(&endpoint->responders, responder);
}

etcpal_error_t add_physical_responder(DeviceEndpoint* endpoint, const RdmnetPhysicalEndpointResponder* responder_config)
{
  if (!RDMNET_ASSERT_VERIFY(endpoint) || !RDMNET_ASSERT_VERIFY(responder_config))
    return kEtcPalErrSys;

  EndpointResponder* responder = ALLOC_ENDPOINT_RESPONDER();
  if (!responder)
    return kEtcPalErrNoMem;

  responder->rid = kEtcPalNullUuid;
  responder->uid = responder_config->uid;
  responder->binding_uid = responder_config->binding_uid;
  responder->control_field = responder_config->control_field;

  return etcpal_rbtree_insert(&endpoint->responders, responder);
}

#if !RDMNET_DYNAMIC_MEM
bool check_static_responder_limits(RdmnetDevice* device, DeviceEndpoint* endpoint, size_t num_additional)
{
  ETCPAL_UNUSED_ARG(endpoint);

  if (!RDMNET_ASSERT_VERIFY(device))
    return false;

  size_t combined_num_responders = 0;
  for (DeviceEndpoint* endpt = device->endpoints; endpt < device->endpoints + device->num_endpoints; ++endpt)
  {
    if (!RDMNET_ASSERT_VERIFY(endpt))
      return false;

    combined_num_responders += etcpal_rbtree_size(&endpt->responders);
  }

  return (combined_num_responders + num_additional) <= RDMNET_MAX_RESPONDERS_PER_DEVICE;
}
#endif  // !RDMNET_DYNAMIC_MEM
