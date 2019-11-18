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

#include "rdmnet/core.h"

#include "etcpal/common.h"
#include "etcpal/socket.h"
#include "etcpal/timer.h"
#include "rdmnet/private/discovery.h"
#include "rdmnet/private/message.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/connection.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/opts.h"

/*************************** Private constants *******************************/

#define RDMNET_TICK_PERIODIC_INTERVAL 100 /* ms */
#define RDMNET_POLL_TIMEOUT 120           /* ms */

#define RDMNET_ETCPAL_FEATURES \
  (ETCPAL_FEATURE_SOCKETS | ETCPAL_FEATURE_TIMERS | ETCPAL_FEATURE_NETINTS | ETCPAL_FEATURE_LOGGING)

/***************************** Private macros ********************************/

#define RDMNET_CREATE_LOCK_OR_DIE()         \
  if (!core_state.lock_initted)             \
  {                                         \
    if (etcpal_rwlock_create(&rdmnet_lock)) \
      core_state.lock_initted = true;       \
    else                                    \
      return kEtcPalErrSys;                 \
  }

/***************************** Global variables ******************************/

const EtcPalLogParams* rdmnet_log_params;

/**************************** Private variables ******************************/

static struct CoreState
{
  bool lock_initted;
  bool initted;

  EtcPalLogParams log_params;
  EtcPalTimer tick_timer;
  EtcPalPollContext poll_context;

#if RDMNET_USE_TICK_THREAD
  bool tickthread_run;
  etcpal_thread_t tick_thread;
#endif
} core_state;

static etcpal_rwlock_t rdmnet_lock;

/*********************** Private function prototypes *************************/

static void rdmnet_tick_thread(void* arg);

/*************************** Function definitions ****************************/

etcpal_error_t rdmnet_core_init(const EtcPalLogParams* log_params)
{
  // The lock is created only the first call to this function.
  RDMNET_CREATE_LOCK_OR_DIE();

  etcpal_error_t res = kEtcPalErrSys;
  if (rdmnet_writelock())
  {
    res = kEtcPalErrOk;
    if (!core_state.initted)
    {
      bool etcpal_initted = false;
      bool poll_initted = false;
      bool conn_initted = false;
      bool disc_initted = false;
      bool llrp_initted = false;

      // Init the log params early so the other modules can log things on initialization
      if (log_params)
      {
        core_state.log_params = *log_params;
        rdmnet_log_params = &core_state.log_params;
      }

      if (res == kEtcPalErrOk)
        etcpal_initted = ((res = etcpal_init(RDMNET_ETCPAL_FEATURES)) == kEtcPalErrOk);
      if (res == kEtcPalErrOk)
        poll_initted = ((res = etcpal_poll_context_init(&core_state.poll_context)) == kEtcPalErrOk);
      if (res == kEtcPalErrOk)
        res = rdmnet_message_init();
      if (res == kEtcPalErrOk)
        conn_initted = ((res = rdmnet_conn_init()) == kEtcPalErrOk);
      if (res == kEtcPalErrOk)
        disc_initted = ((res = rdmnet_disc_init()) == kEtcPalErrOk);
      if (res == kEtcPalErrOk)
        llrp_initted = ((res = rdmnet_llrp_init()) == kEtcPalErrOk);

#if RDMNET_USE_TICK_THREAD
      if (res == kEtcPalErrOk)
      {
        EtcPalThreadParams thread_params;
        thread_params.thread_priority = RDMNET_TICK_THREAD_PRIORITY;
        thread_params.stack_size = RDMNET_TICK_THREAD_STACK;
        thread_params.thread_name = "rdmnet_tick";
        thread_params.platform_data = NULL;
        core_state.tickthread_run = true;
        if (!etcpal_thread_create(&core_state.tick_thread, &thread_params, rdmnet_tick_thread, NULL))
        {
          res = kEtcPalErrSys;
        }
      }
#endif

      if (res == kEtcPalErrOk)
      {
        // Do the rest of the initialization
        etcpal_timer_start(&core_state.tick_timer, RDMNET_TICK_PERIODIC_INTERVAL);
        core_state.initted = true;
      }
      else
      {
        // Clean up. Starting the thread is the last thing with a failure condition, so if we get
        // here it has not been started.
        if (llrp_initted)
          rdmnet_llrp_deinit();
        if (disc_initted)
          rdmnet_disc_deinit();
        if (conn_initted)
          rdmnet_conn_deinit();
        if (poll_initted)
          etcpal_poll_context_deinit(&core_state.poll_context);
        if (etcpal_initted)
          etcpal_deinit(RDMNET_ETCPAL_FEATURES);

        rdmnet_log_params = NULL;
      }
    }
    rdmnet_writeunlock();
  }
  return res;
}

void rdmnet_core_deinit()
{
  if (core_state.initted)
  {
    core_state.initted = false;
#if RDMNET_USE_TICK_THREAD
    core_state.tickthread_run = false;
    etcpal_thread_join(&core_state.tick_thread);
#endif
    if (rdmnet_writelock())
    {
      rdmnet_log_params = NULL;

      rdmnet_llrp_deinit();
      rdmnet_disc_deinit();
      rdmnet_conn_deinit();
      etcpal_poll_context_deinit(&core_state.poll_context);
      etcpal_deinit(RDMNET_ETCPAL_FEATURES);
      rdmnet_writeunlock();
    }
  }
}

bool rdmnet_core_initialized()
{
  bool result = false;

  if (core_state.lock_initted)
  {
    if (rdmnet_readlock())
    {
      result = core_state.initted;
      rdmnet_readunlock();
    }
  }
  return result;
}

etcpal_error_t rdmnet_core_add_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events,
                                             PolledSocketInfo* info)
{
  return etcpal_poll_add_socket(&core_state.poll_context, socket, events, info);
}

etcpal_error_t rdmnet_core_modify_polled_socket(etcpal_socket_t socket, etcpal_poll_events_t events,
                                                PolledSocketInfo* info)
{
  return etcpal_poll_modify_socket(&core_state.poll_context, socket, events, info);
}

void rdmnet_core_remove_polled_socket(etcpal_socket_t socket)
{
  etcpal_poll_remove_socket(&core_state.poll_context, socket);
}

#if RDMNET_USE_TICK_THREAD
void rdmnet_tick_thread(void* arg)
{
  RDMNET_UNUSED_ARG(arg);
  while (core_state.tickthread_run)
  {
    rdmnet_core_tick();
  }
}
#endif

void rdmnet_core_tick()
{
  EtcPalPollEvent event;
  etcpal_error_t poll_res = etcpal_poll_wait(&core_state.poll_context, &event, RDMNET_POLL_TIMEOUT);
  if (poll_res == kEtcPalErrOk)
  {
    PolledSocketInfo* info = (PolledSocketInfo*)event.user_data;
    if (info)
    {
      info->callback(&event, info->data);
    }
  }
  else if (poll_res != kEtcPalErrTimedOut)
  {
    if (poll_res != kEtcPalErrNoSockets)
    {
      etcpal_log(rdmnet_log_params, ETCPAL_LOG_ERR, RDMNET_LOG_MSG("Error ('%s') while polling sockets."),
                 etcpal_strerror(poll_res));
    }
    etcpal_thread_sleep(100);  // Sleep to avoid spinning on errors
  }

  if (etcpal_timer_is_expired(&core_state.tick_timer))
  {
    rdmnet_disc_tick();
    rdmnet_conn_tick();
    rdmnet_llrp_tick();

    etcpal_timer_reset(&core_state.tick_timer);
  }
}

bool rdmnet_readlock()
{
  return etcpal_rwlock_readlock(&rdmnet_lock);
}

void rdmnet_readunlock()
{
  etcpal_rwlock_readunlock(&rdmnet_lock);
}

bool rdmnet_writelock()
{
  return etcpal_rwlock_writelock(&rdmnet_lock);
}

void rdmnet_writeunlock()
{
  etcpal_rwlock_writeunlock(&rdmnet_lock);
}
