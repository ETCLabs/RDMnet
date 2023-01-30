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

#include "rdmnet/core/common.h"

#include "etcpal/common.h"
#include "etcpal/rwlock.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "rdmnet/discovery.h"
#include "rdmnet/core/message.h"
#include "rdmnet/core/client.h"
#include "rdmnet/core/connection.h"
#include "rdmnet/core/llrp.h"
#include "rdmnet/core/llrp_manager.h"
#include "rdmnet/core/llrp_target.h"
#include "rdmnet/core/mcast.h"
#include "rdmnet/core/opts.h"
#include "rdmnet/disc/common.h"

/*************************** Private constants *******************************/

#define RDMNET_TICK_PERIODIC_INTERVAL 100 /* ms */
#define RDMNET_POLL_TIMEOUT 120           /* ms */

#define RDMNET_ETCPAL_FEATURES \
  (ETCPAL_FEATURE_SOCKETS | ETCPAL_FEATURE_TIMERS | ETCPAL_FEATURE_NETINTS | ETCPAL_FEATURE_LOGGING)

/***************************** Private types ********************************/

typedef struct RdmnetCoreModule
{
  etcpal_error_t (*init_fn)(const RdmnetNetintConfig* netint_config);
  void (*deinit_fn)(void);
  void (*tick_fn)(void);
  bool initted;
} RdmnetCoreModule;

/***************************** Private macros ********************************/

#define RDMNET_CREATE_LOCK_OR_DIE()         \
  if (!core_state.lock_initted)             \
  {                                         \
    if (etcpal_rwlock_create(&rdmnet_lock)) \
      core_state.lock_initted = true;       \
    else                                    \
      return kEtcPalErrSys;                 \
  }

#define RDMNET_CORE_MODULE(init_fn, deinit_fn, tick_fn) \
  {                                                     \
    init_fn, deinit_fn, tick_fn, false                  \
  }

/***************************** Global variables ******************************/

const EtcPalLogParams* rdmnet_log_params;

/**************************** Private variables ******************************/

static struct CoreState
{
  bool initted;

  EtcPalLogParams   log_params;
  EtcPalTimer       tick_timer;
  EtcPalPollContext poll_context;
} core_state;

static etcpal_rwlock_t rdmnet_lock;

/*********************** Private function prototypes *************************/

static etcpal_error_t init_etcpal_dependencies(const RdmnetNetintConfig* netint_config);
static void           deinit_etcpal_dependencies(void);

/*************************** Function definitions ****************************/

// clang-format off
static RdmnetCoreModule modules[] = {
  RDMNET_CORE_MODULE(init_etcpal_dependencies, deinit_etcpal_dependencies, NULL),
  RDMNET_CORE_MODULE(rc_mcast_module_init, rc_mcast_module_deinit, NULL),
  RDMNET_CORE_MODULE(rc_conn_module_init, rc_conn_module_deinit, rc_conn_module_tick),
  RDMNET_CORE_MODULE(rdmnet_disc_module_init, rdmnet_disc_module_deinit, rdmnet_disc_module_tick),
  RDMNET_CORE_MODULE(rc_llrp_module_init, rc_llrp_module_deinit, NULL),
  RDMNET_CORE_MODULE(rc_llrp_target_module_init, rc_llrp_target_module_deinit, rc_llrp_target_module_tick),
#if RDMNET_DYNAMIC_MEM
  RDMNET_CORE_MODULE(rc_llrp_manager_module_init, rc_llrp_manager_module_deinit, rc_llrp_manager_module_tick),
#endif
  RDMNET_CORE_MODULE(rc_client_module_init, rc_client_module_deinit, NULL)
};
#define NUM_RDMNET_CORE_MODULES (sizeof(modules) / sizeof(modules[0]))
// clang-format on

/*
 * Initialize the RDMnet core library.
 *
 * Initializes the core modules of the RDMnet library, including LLRP, discovery, connections, and
 * the message dispatch thread.
 *
 * log_params: (optional) log parameters for the RDMnet library to use to log messages. If NULL, no
 *             logging will be performed.
 * netint_config: (optional) a set of network interfaces to which to restrict multicast operation.
 */
etcpal_error_t rc_init(const EtcPalLogParams* log_params, const RdmnetNetintConfig* netint_config)
{
  if (!etcpal_rwlock_create(&rdmnet_lock))
    return kEtcPalErrSys;

  if (core_state.initted)
    return kEtcPalErrAlready;

  // Init the log params early so the other modules can log things on initialization
  if (log_params)
  {
    core_state.log_params = *log_params;
    rdmnet_log_params = &core_state.log_params;
  }

  etcpal_error_t res = kEtcPalErrOk;
  for (RdmnetCoreModule* module = modules; module < modules + NUM_RDMNET_CORE_MODULES; ++module)
  {
    if (!RDMNET_ASSERT_VERIFY(module->init_fn))
      return kEtcPalErrSys;

    if (module->init_fn)
      res = module->init_fn(netint_config);

    if (res == kEtcPalErrOk)
      module->initted = true;
    else
      break;
  }

  if (res == kEtcPalErrOk)
  {
    // Do the rest of the initialization
    etcpal_timer_start(&core_state.tick_timer, RDMNET_TICK_PERIODIC_INTERVAL);
    core_state.initted = true;
  }
  else
  {
    // Clean up in reverse order of initialization.
    for (RdmnetCoreModule* module = modules; module < modules + NUM_RDMNET_CORE_MODULES; ++module)
    {
      if (!RDMNET_ASSERT_VERIFY(module->deinit_fn))
        return kEtcPalErrSys;

      if (module->initted)
      {
        module->deinit_fn();
        module->initted = false;
      }
    }
    rdmnet_log_params = NULL;
  }
  return res;
}

/*
 * Deinitialize the RDMnet core library.
 *
 * Set the RDMnet core library back to an uninitialized state, freeing all resources. Subsequent
 * calls to rc_ APIs will fail until rdmnet_core_init() is called again.
 */
void rc_deinit(void)
{
  if (core_state.initted)
  {
    core_state.initted = false;

    if (rdmnet_writelock())
    {
      for (RdmnetCoreModule* module = &modules[NUM_RDMNET_CORE_MODULES - 1]; module >= modules; --module)
      {
        if (!RDMNET_ASSERT_VERIFY(module->deinit_fn))
          return;

        module->deinit_fn();
        module->initted = false;
      }
      rdmnet_log_params = NULL;
      rdmnet_writeunlock();
    }

    etcpal_rwlock_destroy(&rdmnet_lock);
  }
}

/* Returns whether the RDMnet core library is currently initialized. */
bool rc_initialized(void)
{
  return core_state.initted;
}

etcpal_error_t rc_add_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events, RCPolledSocketInfo* info)
{
  if (!RDMNET_ASSERT_VERIFY(info))
    return kEtcPalErrSys;

  return etcpal_poll_add_socket(&core_state.poll_context, socket, events, info);
}

etcpal_error_t rc_modify_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events, RCPolledSocketInfo* info)
{
  if (!RDMNET_ASSERT_VERIFY(info))
    return kEtcPalErrSys;

  return etcpal_poll_modify_socket(&core_state.poll_context, socket, events, info);
}

void rc_remove_polled_socket(etcpal_socket_t socket)
{
  etcpal_poll_remove_socket(&core_state.poll_context, socket);
}

/*
 * Since all RDMnet sockets need to be non-blocking for receiving, this function provides a blocking send in order to
 * support TCP throttling.
 */
int rc_send(etcpal_socket_t id, const void* message, size_t length, int flags)
{
  if (!RDMNET_ASSERT_VERIFY(message))
    return (int)kEtcPalErrSys;

  int res = etcpal_send(id, message, length, flags);
  while ((etcpal_error_t)res == kEtcPalErrWouldBlock)
  {
    etcpal_thread_sleep(10);
    res = etcpal_send(id, message, length, flags);
  }

  return res;
}

/*
 * Process RDMnet background tasks.
 *
 * This includes polling for data on incoming network connections, checking various timeouts, and
 * delivering notification callbacks.
 */
void rc_tick(void)
{
  EtcPalPollEvent event;
  etcpal_error_t  poll_res = etcpal_poll_wait(&core_state.poll_context, &event, RDMNET_POLL_TIMEOUT);
  if (poll_res == kEtcPalErrOk)
  {
    RCPolledSocketInfo* info = (RCPolledSocketInfo*)event.user_data;
    if (info)
    {
      if (RDMNET_ASSERT_VERIFY(info->callback))
        info->callback(&event, info->data);
    }
  }
  else if (poll_res != kEtcPalErrTimedOut)
  {
    if (poll_res != kEtcPalErrNoSockets)
    {
      RDMNET_LOG_ERR("Error ('%s') while polling sockets.", etcpal_strerror(poll_res));
    }
    etcpal_thread_sleep(100);  // Sleep to avoid spinning on errors
  }

  if (etcpal_timer_is_expired(&core_state.tick_timer))
  {
    for (size_t i = 0; i < NUM_RDMNET_CORE_MODULES; ++i)
    {
      RdmnetCoreModule* module_struct = &modules[i];
      if (module_struct->tick_fn)
        module_struct->tick_fn();
    }
    etcpal_timer_reset(&core_state.tick_timer);
  }
}

bool rdmnet_readlock(void)
{
  return etcpal_rwlock_readlock(&rdmnet_lock);
}

void rdmnet_readunlock(void)
{
  etcpal_rwlock_readunlock(&rdmnet_lock);
}

bool rdmnet_writelock(void)
{
  return etcpal_rwlock_writelock(&rdmnet_lock);
}

void rdmnet_writeunlock(void)
{
  etcpal_rwlock_writeunlock(&rdmnet_lock);
}

bool rdmnet_assert_verify_fail(const char* exp, const char* file, const char* func, const int line)
{
#if !RDMNET_LOGGING_ENABLED
  ETCPAL_UNUSED_ARG(exp);
  ETCPAL_UNUSED_ARG(file);
  ETCPAL_UNUSED_ARG(func);
  ETCPAL_UNUSED_ARG(line);
#endif
  RDMNET_LOG_CRIT("ASSERTION \"%s\" FAILED (FILE: \"%s\" FUNCTION: \"%s\" LINE: %d)", exp ? exp : "", file ? file : "",
                  func ? func : "", line);
  RDMNET_ASSERT(false);
  return false;
}

etcpal_error_t init_etcpal_dependencies(const RdmnetNetintConfig* netint_config)
{
  ETCPAL_UNUSED_ARG(netint_config);

  etcpal_error_t res = etcpal_init(RDMNET_ETCPAL_FEATURES);
  if (res == kEtcPalErrOk)
  {
    res = etcpal_poll_context_init(&core_state.poll_context);
    if (res != kEtcPalErrOk)
      etcpal_deinit(RDMNET_ETCPAL_FEATURES);
  }
  return res;
}

void deinit_etcpal_dependencies(void)
{
  etcpal_poll_context_deinit(&core_state.poll_context);
  etcpal_deinit(RDMNET_ETCPAL_FEATURES);
}
