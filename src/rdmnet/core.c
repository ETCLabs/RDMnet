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

#include "rdmnet/private/core.h"

/************************* The draft warning message *************************/

/* clang-format off */
#pragma message("************ THIS CODE IMPLEMENTS A DRAFT STANDARD ************")
#pragma message("*** PLEASE DO NOT INCLUDE THIS CODE IN ANY SHIPPING PRODUCT ***")
#pragma message("************* SEE THE README FOR MORE INFORMATION *************")
/* clang-format on */

/***************************** Private macros ********************************/

#define rdmnet_create_lock_or_die()       \
  if (!rdmnet_lock_initted)               \
  {                                       \
    if (lwpa_rwlock_create(&rdmnet_lock)) \
      rdmnet_lock_initted = true;         \
    else                                  \
      return LWPA_SYSERR;                 \
  }

/***************************** Global variables ******************************/

lwpa_rwlock_t rdmnet_lock;
const LwpaLogParams *rdmnet_log_params;

/**************************** Private variables ******************************/

static bool rdmnet_lock_initted = false;
static bool rdmnet_initted = false;
static LwpaLogParams rdmnet_log_params_cache;

/*************************** Function definitions ****************************/

lwpa_error_t rdmnet_core_init(const LwpaLogParams *log_params)
{
  // The lock is created only the first call to this function.
  rdmnet_create_lock_or_die();

  lwpa_error_t res = LWPA_SYSERR;
  if (rdmnet_writelock())
  {
    res = LWPA_OK;
    if (!rdmnet_initted)
    {
      if (res == LWPA_OK)
        res = rdmnet_message_init();
      if (res == LWPA_OK)
        res = lwpa_socket_init(NULL);
      if (res == LWPA_OK)
      {
        res = rdmnetdisc_init();
        if (res == LWPA_OK)
        {
          // Do the initialization
          if (log_params)
          {
            rdmnet_log_params_cache = *log_params;
            rdmnet_log_params = &rdmnet_log_params_cache;
          }
          rdmnet_initted = true;
        }
        else
        {
          lwpa_log(log_params, LWPA_LOG_ERR, "Couldn't initialize RDMnet discovery due to error: '%s'.",
                   lwpa_strerror(res));
          return res;
        }
      }
    }
  }
  return res;
}

void rdmnet_core_deinit()
{
  if (rdmnet_writelock())
  {
    rdmnet_log_params = NULL;
    rdmnet_initted = false;
    rdmnet_writeunlock();
  }
}

bool rdmnet_core_initialized()
{
  bool result = false;

  if (rdmnet_lock_initted)
  {
    if (rdmnet_readlock())
    {
      result = rdmnet_initted;
      rdmnet_readunlock();
    }
  }
  return result;
}
