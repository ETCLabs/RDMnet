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

#include "rdmnet/core.h"

#include "lwpa/socket.h"
#include "lwpa/timer.h"
#include "rdmnet/core/discovery.h"
#include "rdmnet/private/message.h"
#include "rdmnet/private/core.h"
#include "rdmnet/private/connection.h"
#include "rdmnet/private/discovery.h"
#include "rdmnet/private/llrp.h"
#include "rdmnet/private/opts.h"

/************************* The draft warning message *************************/

/* clang-format off */
#pragma message("************ THIS CODE IMPLEMENTS A DRAFT STANDARD ************")
#pragma message("*** PLEASE DO NOT INCLUDE THIS CODE IN ANY SHIPPING PRODUCT ***")
#pragma message("************* SEE THE README FOR MORE INFORMATION *************")
/* clang-format on */

/*************************** Private constants *******************************/

#define RDMNET_TICK_PERIODIC_INTERVAL 100 /* ms */
#define RDMNET_POLL_TIMEOUT 120           /* ms */

#define RDMNET_LWPA_FEATURES (LWPA_FEATURE_SOCKETS | LWPA_FEATURE_TIMERS)

/***************************** Private macros ********************************/

#define RDMNET_CREATE_LOCK_OR_DIE()       \
  if (!core_state.lock_initted)           \
  {                                       \
    if (lwpa_rwlock_create(&rdmnet_lock)) \
      core_state.lock_initted = true;     \
    else                                  \
      return kLwpaErrSys;                 \
  }

/***************************** Global variables ******************************/

lwpa_rwlock_t rdmnet_lock;
const LwpaLogParams *rdmnet_log_params;

/**************************** Private variables ******************************/

static struct CoreState
{
  bool lock_initted;
  bool initted;

  LwpaLogParams log_params;
  LwpaTimer tick_timer;
  LwpaPollContext poll_context;

#if RDMNET_USE_TICK_THREAD
  bool tickthread_run;
  lwpa_thread_t tick_thread;
#endif
} core_state;

/*********************** Private function prototypes *************************/

static void rdmnet_tick_thread(void *arg);

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_core_init(const LwpaLogParams *log_params)
{
  // The lock is created only the first call to this function.
  RDMNET_CREATE_LOCK_OR_DIE();

  lwpa_error_t res = kLwpaErrSys;
  if (rdmnet_writelock())
  {
    res = kLwpaErrOk;
    if (!core_state.initted)
    {
      bool lwpa_initted = false;
      bool poll_initted = false;
      bool conn_initted = false;
      bool disc_initted = false;
      bool llrp_initted = false;

      if (res == kLwpaErrOk)
        lwpa_initted = ((res = lwpa_init(RDMNET_LWPA_FEATURES)) == kLwpaErrOk);
      if (res == kLwpaErrOk)
        poll_initted = ((res = lwpa_poll_context_init(&core_state.poll_context)) == kLwpaErrOk);
      if (res == kLwpaErrOk)
        res = rdmnet_message_init();
      if (res == kLwpaErrOk)
        conn_initted = ((res = rdmnet_conn_init()) == kLwpaErrOk);
      if (res == kLwpaErrOk)
        disc_initted = ((res = rdmnetdisc_init()) == kLwpaErrOk);
      if (res == kLwpaErrOk)
        llrp_initted = ((res = rdmnet_llrp_init()) == kLwpaErrOk);

#if RDMNET_USE_TICK_THREAD
      if (res == kLwpaErrOk)
      {
        LwpaThreadParams thread_params;
        thread_params.thread_priority = RDMNET_TICK_THREAD_PRIORITY;
        thread_params.stack_size = RDMNET_TICK_THREAD_STACK;
        thread_params.thread_name = "rdmnet_tick";
        thread_params.platform_data = NULL;
        core_state.tickthread_run = true;
        if (!lwpa_thread_create(&core_state.tick_thread, &thread_params, rdmnet_tick_thread, NULL))
        {
          res = kLwpaErrSys;
        }
      }
#endif

      if (res == kLwpaErrOk)
      {
        // Do the initialization
        if (log_params)
        {
          core_state.log_params = *log_params;
          rdmnet_log_params = &core_state.log_params;
        }
        lwpa_timer_start(&core_state.tick_timer, RDMNET_TICK_PERIODIC_INTERVAL);
        core_state.initted = true;
      }
      else
      {
        // Clean up
        if (llrp_initted)
          rdmnet_llrp_deinit();
        if (disc_initted)
          rdmnetdisc_deinit();
        if (conn_initted)
          rdmnet_conn_deinit();
        if (poll_initted)
          lwpa_poll_context_deinit(&core_state.poll_context);
        if (lwpa_initted)
          lwpa_deinit(RDMNET_LWPA_FEATURES);
      }
    }
    rdmnet_writeunlock();
  }
  return res;
}

void rdmnet_core_deinit()
{
  if (rdmnet_writelock())
  {
    if (core_state.initted)
    {
      core_state.initted = false;
      rdmnet_log_params = NULL;

      rdmnet_llrp_deinit();
      rdmnetdisc_deinit();
      rdmnet_conn_deinit();
      lwpa_poll_context_deinit(&core_state.poll_context);
      lwpa_deinit(RDMNET_LWPA_FEATURES);
    }
    rdmnet_writeunlock();
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

lwpa_error_t rdmnet_core_add_polled_socket(lwpa_socket_t socket, lwpa_poll_events_t events, PolledSocketInfo *info)
{
  return lwpa_poll_add_socket(&core_state.poll_context, socket, events, info);
}

lwpa_error_t rdmnet_core_modify_polled_socket(lwpa_socket_t socket, lwpa_poll_events_t events, PolledSocketInfo *info)
{
  return lwpa_poll_modify_socket(&core_state.poll_context, socket, events, info);
}

void rdmnet_core_remove_polled_socket(lwpa_socket_t socket)
{
  lwpa_poll_remove_socket(&core_state.poll_context, socket);
}

#if RDMNET_USE_TICK_THREAD
void rdmnet_tick_thread(void *arg)
{
  (void)arg;
  while (core_state.tickthread_run)
  {
    rdmnet_core_tick();
  }
}
#endif

void rdmnet_core_tick()
{
  LwpaPollEvent event;
  lwpa_error_t poll_res = lwpa_poll_wait(&core_state.poll_context, &event, RDMNET_POLL_TIMEOUT);
  if (poll_res == kLwpaErrOk)
  {
    PolledSocketInfo *info = (PolledSocketInfo *)event.user_data;
    if (info)
    {
      info->callback(&event, info->data);
    }
  }
  else if (poll_res != kLwpaErrTimedOut)
  {
    if (poll_res != kLwpaErrNoSockets)
    {
      lwpa_log(rdmnet_log_params, LWPA_LOG_ERR, RDMNET_LOG_MSG("Error ('%s') while polling sockets."),
               lwpa_strerror(poll_res));
    }
    lwpa_thread_sleep(100);  // Sleep to avoid spinning on errors
  }

  if (lwpa_timer_isexpired(&core_state.tick_timer))
  {
    rdmnetdisc_tick();
    rdmnet_conn_tick();
    rdmnet_llrp_tick();

    lwpa_timer_reset(&core_state.tick_timer);
  }
}
