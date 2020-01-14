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
 * \file rdmnet/core/llrp.h
 * \brief Functions and definitions common to LLRP Managers and Targets.
 */

#ifndef RDMNET_CORE_LLRP_H_
#define RDMNET_CORE_LLRP_H_

#include <stdint.h>
#include "etcpal/inet.h"
#include "etcpal/uuid.h"
#include "rdm/uid.h"
#include "rdmnet/defs.h"

/*!
 * \defgroup llrp LLRP
 * \ingroup rdmnet_core_lib
 * \brief Implement Low Level Recovery Protocol (LLRP) functionality.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/*! A handle for an instance of LLRP Manager functionality. */
typedef int llrp_manager_t;
/*! An invalid LLRP manager handle value. */
#define LLRP_MANAGER_INVALID -1

/*! A handle for an instance of LLRP Target functionality. */
typedef int llrp_target_t;
/*! An invalid LLRP target handle value. */
#define LLRP_TARGET_INVALID -1

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
typedef struct DiscoveredLlrpTarget
{
  /*! The LLRP Target's CID. */
  EtcPalUuid cid;
  /*! The LLRP Target's UID. */
  RdmUid uid;
  /*! The LLRP Target's hardware address (usually the MAC address) */
  EtcPalMacAddr hardware_address;
  /*! The type of RPT Component this LLRP Target is associated with. */
  llrp_component_t component_type;
} DiscoveredLlrpTarget;

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_CORE_LLRP_H_ */
