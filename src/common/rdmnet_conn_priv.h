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

/*! \file rdmnet_conn_priv.h
 *  \brief The internal definition for an RDMnet connection.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CONN_PRIV_H_
#define _RDMNET_CONN_PRIV_H_

#include "lwpa/bool.h"
#include "lwpa/int.h"
#include "lwpa/lock.h"
#include "lwpa/timer.h"
#include "lwpa/inet.h"
#include "rdmnet/common/opts.h"
#include "rdmnet/common/connection.h"
#include "rdmnet_msg_buf.h"

#define rdmnet_data_set_nodata(rdmnetdataptr) ((rdmnetdataptr)->type = kRDMnetNoData)
#define rdmnet_data_set_code(rdmnetdataptr, code_to_set) \
  do                                                     \
  {                                                      \
    (rdmnetdataptr)->type = kRDMnetDataTypeCode;         \
    (rdmnetdataptr)->data.code = code_to_set;            \
  } while (0)
#define rdmnet_data_set_msg(rdmnetdataptr, msg_to_set) \
  do                                                   \
  {                                                    \
    (rdmnetdataptr)->type = kRDMnetDataTypeMessage;    \
    (rdmnetdataptr)->data.msg = msg_to_set;            \
  } while (0)
#define rdmnet_data_set_addr(rdmnetdataptr, addr_to_set) \
  do                                                     \
  {                                                      \
    (rdmnetdataptr)->type = kRDMnetDataTypeAddress;      \
    (rdmnetdataptr)->data.addr = addr_to_set;            \
  } while (0)

typedef enum
{
  kCSNotConnected,
  kCSBackoff,
  kCSTCPConnPending,
  kCSRDMnetConnPending,
  kCSHeartbeat
} conn_state_t;

typedef struct ConnPoll ConnPoll;
struct ConnPoll
{
  lwpa_signal_t sig;
  RdmnetPoll *poll_arr;
  size_t poll_arr_size;
  ConnPoll *next;
};

typedef struct RdmnetConnection
{
  /* Identification */
  int handle;
  LwpaUuid local_cid;
  lwpa_socket_t sock;
  LwpaSockaddr remote_addr;
  bool is_blocking;

  /* Connection state */
  conn_state_t state;
  ConnPoll *poll_list;
  ClientConnectMsg conn_data;
  LwpaTimer send_timer;
  LwpaTimer hb_timer;
  LwpaTimer backoff_timer;
  bool rdmnet_conn_failed;

  /* Send tracking */
  lwpa_mutex_t send_lock;

  /* Receive tracking */
  bool recv_waiting;
  RdmnetMsgBuf recv_buf;
  lwpa_error_t recv_disconn_err;

  /* Synchronization */
  lwpa_mutex_t lock;
} RdmnetConnection;

#endif /* _RDMNET_CONN_PRIV_H_ */
