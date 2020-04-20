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

/*!
 * \file rdmnet/llrp.h
 * \brief Functions and definitions common to LLRP Managers and Targets.
 */

#ifndef RDMNET_LLRP_H_
#define RDMNET_LLRP_H_

#include <stdint.h>
#include "etcpal/inet.h"
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"

/*!
 * \addtogroup rdmnet_api_common
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \brief A destination address for an RDM command in LLRP.
 * \brief See \ref llrp for more information.
 */
typedef struct LlrpDestinationAddr
{
  /*! The CID of the LLRP Target to which this command is addressed. */
  EtcPalUuid dest_cid;
  /*! The UID of the LLRP Target to which this command is addressed. */
  RdmUid dest_uid;
  /*! The sub-device to which this command is addressed, or 0 for the root device. */
  uint16_t subdevice;
} LlrpDestinationAddr;

/*! Identifies the type of RPT Component with which an LLRP Target is associated. */
typedef enum
{
  /*! This LLRP Target is associated with an RPT Device. */
  kLlrpCompRptDevice = LLRP_COMPONENT_TYPE_RPT_DEVICE,
  /*! This LLRP Target is associated with an RPT Controller. */
  kLlrpCompRptController = LLRP_COMPONENT_TYPE_RPT_CONTROLLER,
  /*! This LLRP Target is associated with a Broker. */
  kLlrpCompBroker = LLRP_COMPONENT_TYPE_BROKER,
  /*! This LLRP Target does not implement any RDMnet protocol other than LLRP. */
  kLlrpCompNonRdmnet = LLRP_COMPONENT_TYPE_NONRDMNET
} llrp_component_t;

/*! A set of information associated with an LLRP Target. */
typedef struct LlrpDiscoveredTarget
{
  /*! The LLRP Target's CID. */
  EtcPalUuid cid;
  /*! The LLRP Target's UID. */
  RdmUid uid;
  /*! The LLRP Target's hardware address (usually the MAC address). */
  EtcPalMacAddr hardware_address;
  /*! The type of RPT Component this LLRP Target is associated with. */
  llrp_component_t component_type;
} LlrpDiscoveredTarget;

const char* llrp_component_type_to_string(llrp_component_t type);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_LLRP_H_ */
