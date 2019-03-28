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

/*! \file rdmnet/core/message.h
 *  \brief Basic types for parsed RDMnet messages.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CORE_MESSAGE_H_
#define _RDMNET_CORE_MESSAGE_H_

#include <stddef.h>
#include "lwpa/int.h"
#include "lwpa/bool.h"
#include "lwpa/root_layer_pdu.h"
#include "lwpa/uuid.h"
#include "rdmnet/core/broker_prot.h"
#include "rdmnet/core/rpt_prot.h"
#include "rdmnet/core/ept_prot.h"

/*! \defgroup rdmnet_message Message
 *  \ingroup rdmnet_core_lib
 *
 *  Types to represent RDMnet messages, and functions to pack and unpack them. LLRP Messages are
 *  excluded, as they are handled by separate logic.
 *
 *  @{
 */

typedef enum
{
  kRptClientMsgRdmCmd,
  kRptClientMsgRdmResp,
  kRptClientMsgStatus
} rpt_client_msg_t;

typedef struct RemoteRdmCommand
{
  RdmUid source_uid;
  uint16_t dest_endpoint;
  uint32_t seq_num;
  RdmCommand rdm;
} RemoteRdmCommand;

typedef struct LocalRdmResponse
{
  RdmUid dest_uid;
  uint16_t source_endpoint;
  uint32_t seq_num;
  RdmResponse *rdm_arr;
  size_t num_responses;
} LocalRdmResponse;

typedef struct LocalRdmCommand
{
  RdmUid dest_uid;
  uint16_t dest_endpoint;
  RdmCommand rdm;
} ControllerRdmCommand;

typedef struct RemoteRdmRespListEntry RemoteRdmRespListEntry;
struct RemoteRdmRespListEntry
{
  RdmResponse msg;
  RemoteRdmRespListEntry *next;
};

typedef struct RemoteRdmResponseList
{
  /*! This message contains a partial list. This can be set when the library runs out of static
   *  memory in which to store RDM Commands and must deliver the partial list before continuing.
   *  The application should store the entries in the list but should not act on the list until
   *  another RdmCmdList is received with partial set to false. */
  bool partial;
  /*! The head of a linked list of packed RDM Commands. */
  RemoteRdmRespListEntry *list;
} RemoteRdmResponseList;

typedef struct RemoteRdmResponse
{
  RdmUid source_uid;
  uint16_t source_endpoint;
  uint32_t seq_num;
  bool have_command;
  RdmCommand cmd;
  RemoteRdmResponseList resp_list;
} RemoteRdmResponse;

typedef struct LocalRptStatus
{
  RdmUid dest_uid;
  uint16_t source_endpoint;
  uint32_t seq_num;
  RptStatusMsg msg;
} LocalRptStatus;

typedef struct RemoteRptStatus
{
  RdmUid source_uid;
  uint16_t source_endpoint;
  uint32_t seq_num;
  RptStatusMsg msg;
} RemoteRptStatus;

typedef struct RptClientMessage
{
  rpt_client_msg_t type;
  union
  {
    RemoteRdmCommand cmd;
    RemoteRdmResponse resp;
    RemoteRptStatus status;
  } payload;
} RptClientMessage;

typedef enum
{
  kEptClientMsgData,
  kEptClientMsgStatus
} ept_client_msg_t;

typedef struct EptClientMessage
{
  ept_client_msg_t type;
  union
  {
    EptStatusMsg status;
    EptDataMsg data;
  } payload;
} EptClientMessage;

typedef struct RdmnetClientMessage
{
  rpt_client_type_t msg_type;
  union
  {
    RptClientMessage rpt;
    EptClientMessage ept;
  } prot_msg;
} RdmnetClientMessage;

/*! A received RDMnet message. */
typedef struct RdmnetMessage
{
  /*! The root layer vector. Compare to the vectors in \ref lwpa_rootlayerpdu. */
  uint32_t vector;
  /*! The CID of the Component that sent this message. */
  LwpaUuid sender_cid;
  /*! The encapsulated message; use the helper macros to access it. */
  union
  {
    BrokerMessage broker;
    RptMessage rpt;
    EptMessage ept;
  } data;
} RdmnetMessage;

/*! \brief Determine whether an RdmnetMessage contains a Broker message.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return (true or false) whether the message contains a Broker message. */
#define is_broker_msg(msgptr) ((msgptr)->vector == ACN_VECTOR_ROOT_BROKER)
/*! \brief Get the encapsulated Broker message from an RdmnetMessage.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return Pointer to encapsulated Broker message (BrokerMessage *). */
#define get_broker_msg(msgptr) (&(msgptr)->data.broker)
/*! \brief Determine whether an RdmnetMessage contains a RPT message.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return (true or false) whether the message contains a RPT message. */
#define is_rpt_msg(msgptr) ((msgptr)->vector == ACN_VECTOR_ROOT_RPT)
/*! \brief Get the encapsulated RPT message from an RdmnetMessage.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return Pointer to encapsulated RPT message (RptMessage *). */
#define get_rpt_msg(msgptr) (&(msgptr)->data.rpt)
/*! \brief Determine whether an RdmnetMessage contains a EPT message.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return (true or false) whether the message contains a EPT message. */
#define is_ept_msg(msgptr) ((msgptr)->vector == ACN_VECTOR_ROOT_EPT)
/*! \brief Get the encapsulated EPT message from an RdmnetMessage.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return Pointer to encapsulated EPT message (EptMessage *). */
#define get_ept_msg(msgptr) (&(msgptr)->data.ept)

#ifdef __cplusplus
extern "C" {
#endif

void free_rdmnet_message(RdmnetMessage *msg);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_CORE_MESSAGE_H_ */
