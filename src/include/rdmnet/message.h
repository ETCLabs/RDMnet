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

/*! \file rdmnet/message.h
 *  \brief Basic types for parsed RDMnet messages.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_MESSAGE_H_
#define _RDMNET_MESSAGE_H_

#include "lwpa_int.h"
#include "lwpa_rootlayerpdu.h"
#include "lwpa_cid.h"
#include "rdmnet/brokerprot.h"
#include "rdmnet/rptprot.h"
#include "rdmnet/eptprot.h"

/*! \defgroup rdmnet_message Message
 *  \ingroup rdmnet_core_lib
 *
 *  Types to represent RDMnet messages, and functions to pack and unpack them.
 *  LLRP Messages are excluded, as they are handled by separate logic.
 *
 *  @{
 */

/*! A received RDMnet message. */
typedef struct RdmnetMessage
{
  /*! The root layer vector. Compare to the vectors in
   *  \ref lwpa_rootlayerpdu. */
  uint32_t vector;
  /*! The CID of the Component that sent this message. */
  LwpaCid sender_cid;
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
#define is_broker_msg(msgptr) ((msgptr)->vector == VECTOR_ROOT_BROKER)
/*! \brief Get the encapsulated Broker message from an RdmnetMessage.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return Pointer to encapsulated Broker message (BrokerMessage *). */
#define get_broker_msg(msgptr) (&(msgptr)->data.broker)
/*! \brief Determine whether an RdmnetMessage contains a RPT message.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return (true or false) whether the message contains a RPT message. */
#define is_rpt_msg(msgptr) ((msgptr)->vector == VECTOR_ROOT_RPT)
/*! \brief Get the encapsulated RPT message from an RdmnetMessage.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return Pointer to encapsulated RPT message (RptMessage *). */
#define get_rpt_msg(msgptr) (&(msgptr)->data.rpt)
/*! \brief Determine whether an RdmnetMessage contains a EPT message.
 *  \param msgptr Pointer to RdmnetMessage.
 *  \return (true or false) whether the message contains a EPT message. */
#define is_ept_msg(msgptr) ((msgptr)->vector == VECTOR_ROOT_EPT)
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

#endif /* _RDMNET_MESSAGE_H_ */
