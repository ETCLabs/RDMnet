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

/*! A handle for an instance of LLRP Manager functionality. */
typedef int llrp_manager_t;
/*! An invalid LLRP manager handle value. */
#define LLRP_MANAGER_INVALID -1

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

/*! An RDM command received from a remote LLRP Manager. */
typedef struct LlrpRdmCommand
{
  /*! The CID of the LLRP Manager from which this command was received. */
  EtcPalUuid source_cid;
  /*! The sequence number received with this command, to be echoed in the corresponding response. */
  uint32_t seq_num;
  /*!
   * An ID for the network interface on which this command was received. This helps the LLRP
   * library send the response on the same interface on which it was received.
   */
  RdmnetMcastNetintId netint_id;
  /*! The header information from the encapsulated RDM command. */
  RdmCommandHeader rdm_header;
  /*! Pointer to buffer containing any associated RDM parameter data. */
  const uint8_t* data;
  /*! The length of any associated RDM parameter data. */
  uint8_t data_len;
} LlrpRdmCommand;

/*! An RDM command received from a remote LLRP Manager. */
typedef struct LlrpSavedRdmCommand
{
  /*! The CID of the LLRP Manager from which this command was received. */
  EtcPalUuid source_cid;
  /*! The sequence number received with this command, to be echoed in the corresponding response. */
  uint32_t seq_num;
  /*!
   * An ID for the network interface on which this command was received. This helps the LLRP
   * library send the response on the same interface on which it was received.
   */
  RdmnetMcastNetintId netint_id;
  /*! The header information from the encapsulated RDM command. */
  RdmCommandHeader rdm_header;
  /*! Pointer to buffer containing any associated RDM parameter data. */
  uint8_t data[RDM_MAX_PDL];
  /*! The length of any associated RDM parameter data. */
  uint8_t data_len;
} LlrpSavedRdmCommand;

/*! An RDM response received from a remote LLRP Target. */
typedef struct LlrpRdmResponse
{
  /*! The CID of the LLRP Target from which this command was received. */
  EtcPalUuid source_cid;
  /*! The sequence number of this response (to be associated with a previously-sent command). */
  uint32_t seq_num;
  /*! The header information from the encapsulated RDM response. */
  RdmResponseHeader rdm_header;
  /*! Any parameter data associated with the RDM response. */
  const uint8_t* rdm_data;
  /*! The length of the parameter data associated with the RDM response. */
  uint8_t rdm_data_len;
} LlrpRdmResponse;

/*! An RDM command received from a remote LLRP Manager. */
typedef struct LlrpSavedRdmResponse
{
  /*! The CID of the LLRP Target from which this command was received. */
  EtcPalUuid source_cid;
  /*! The sequence number of this response (to be associated with a previously-sent command). */
  uint32_t seq_num;
  /*! The header information from the encapsulated RDM response. */
  RdmResponseHeader rdm_header;
  /*! Any parameter data associated with the RDM response. */
  uint8_t rdm_data[RDM_MAX_PDL];
  /*! The length of the parameter data associated with the RDM response. */
  uint8_t rdm_data_len;
} LlrpSavedRdmResponse;

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
  /*! The LLRP Target's hardware address (usually the MAC address). */
  EtcPalMacAddr hardware_address;
  /*! The type of RPT Component this LLRP Target is associated with. */
  llrp_component_t component_type;
} DiscoveredLlrpTarget;

const char* llrp_component_type_to_string(llrp_component_t type);

etcpal_error_t llrp_save_rdm_command(const LlrpRdmCommand* command, LlrpSavedRdmCommand* saved_command);
etcpal_error_t llrp_save_rdm_response(const LlrpRdmResponse* response, LlrpSavedRdmResponse* saved_response);
etcpal_error_t llrp_copy_saved_rdm_response(const LlrpSavedRdmResponse* saved_resp_old,
                                            LlrpSavedRdmResponse* saved_resp_new);

#ifdef __cplusplus
}
#endif

/*!
 * @}
 */

#endif /* RDMNET_LLRP_H_ */
