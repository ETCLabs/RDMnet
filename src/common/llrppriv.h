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

/*! \file llrppriv.h
 *  \brief Private code used internally, including networking and protocol
 *         code.
 *  \author Sam Kearney, Christian Reese
 */
#ifndef _RDMNET_LLRPPRIV_H_
#define _RDMNET_LLRPPRIV_H_

#include "lwpa_uid.h"
#include "lwpa_cid.h"
#include "lwpa_bool.h"
#include "lwpa_int.h"
#include "lwpa_inet.h"
#include "lwpa_rbtree.h"
#include "lwpa_timer.h"
#include "lwpa_socket.h"
#include "lwpa_rootlayerpdu.h"
#include "estardmnet.h"
#include "rdmnet/llrp.h"
#include "llrpprotpriv.h"

typedef enum
{
  kLLRPSocketTypeNone,
  kLLRPSocketTypeTarget,
  kLLRPSocketTypeManager
} llrp_socket_type_t;

typedef struct LlrpTargetSocketData
{
  LlrpTarget target_info;

  bool connected_to_broker;
  bool reply_pending;
  LwpaCid pending_reply_cid;
  uint32_t pending_reply_trans_num;
  LwpaTimer reply_backoff;
} LlrpTargetSocketData;

typedef struct LlrpManagerSocketData
{
  uint32_t transaction_number;
  bool discovery_active;

  unsigned int num_clean_sends;
  LwpaTimer disc_timer;
  uint8_t disc_filter;

  LwpaRbTree known_uids;
  LwpaUid cur_range_low;
  LwpaUid cur_range_high;
} LlrpManagerSocketData;

typedef struct LlrpBaseSocket LlrpBaseSocket;
struct LlrpBaseSocket
{
  LwpaIpAddr net_int_addr;
  LwpaCid owner_cid;

  lwpa_socket_t sys_sock;

  bool data_received;
  uint8_t recv_buf[LLRP_MAX_MESSAGE_SIZE];
  uint8_t send_buf[LLRP_MAX_MESSAGE_SIZE];

  LlrpBaseSocket *next;

  llrp_socket_type_t socket_type;

  union
  {
    LlrpTargetSocketData target;
    LlrpManagerSocketData manager;
  } role;
};

#define get_manager_data(llrpsockptr) (&(llrpsockptr)->role.manager)
#define get_target_data(llrpsockptr) (&(llrpsockptr)->role.target)

#endif /* _RDMNET_LLRPPRIV_H_ */
