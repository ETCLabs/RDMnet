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

/*! \file rdmnet/rdmtypes.h
 *  \brief Contains type definitions for use with RDM (E1.20) messages.
 *  \author Sam Kearney */
#ifndef _RDMNET_RDMTYPES_H_
#define _RDMNET_RDMTYPES_H_

#include <stddef.h>
#include "lwpa_int.h"
#include "lwpa_bool.h"
#include "lwpa_uid.h"

/*! \defgroup rdm RDM
 *  \ingroup rdmnet_core_lib
 *  \brief Basic RDM message packing and unpacking.
 *
 *  @{
 */

/*! The maximum size for an RDM packet, including the two checksum bytes. */
#define RDM_MAX_BYTES 257

/*! The minimum size for an RDM packet. */
#define RDM_MIN_BYTES 26

/*! The maximum length of the Parameter Data in an RDM packet. */
#define RDM_MAX_PDL 231

#define RDM_HEADER_SIZE 24

/*! \name RDM packet offsets
 *  @{
 */
#define RDM_OFFSET_STARTCODE 0
#define RDM_OFFSET_SUBSTART 1
#define RDM_OFFSET_LENGTH 2
#define RDM_OFFSET_DEST_MANUFACTURER 3
#define RDM_OFFSET_DEST_DEVICE 5
#define RDM_OFFSET_SRC_MANUFACTURER 9
#define RDM_OFFSET_SRC_DEVICE 11
#define RDM_OFFSET_TRANSACTION 15
#define RDM_OFFSET_PORTID_RESPTYPE 16
#define RDM_OFFSET_MSGCOUNT 17
#define RDM_OFFSET_SUBDEVICE 18
#define RDM_OFFSET_COMMAND_CLASS 20
#define RDM_OFFSET_PARAM_ID 21
#define RDM_OFFSET_PARAM_DATA_LEN 23
#define RDM_OFFSET_PARAM_DATA 24
/*! @} */

/*! A structure that represents a packed RDM message. */
typedef struct RdmBuffer
{
  uint8_t data[RDM_MAX_BYTES]; /*!< The RDM message bytes. */
  size_t datalen;              /*!< The length of the RDM message. */
} RdmBuffer;

/*! \brief Get the command class value from a packed RDM command.
 *  \param rdmbufptr Pointer to RdmBuffer containing the packed RDM
 *         command.
 *  \return Command class (uint8_t).
 */
#define get_command_class(rdmbufptr) ((rdmbufptr)->data[RDM_OFFSET_COMMAND_CLASS])
/*! \brief Get the RDM transaction number from a packed RDM command.
 *  \param rdmbufptr Pointer to RdmBuffer containing the packed RDM
 *         command.
 *  \return Transaction number (uint8_t).
 */
#define get_transaction_num(rdmbufptr) ((rdmbufptr)->data[RDM_OFFSET_TRANSACTION])

/*! A structure that represents an RDM command message. */
typedef struct RdmCommand
{
  /*! UID of the controller generating this command. */
  LwpaUid src_uid;
  /*! UID of the responder to which this command is addressed. */
  LwpaUid dest_uid;
  /*! Transaction number, monotonically incrementing */
  uint8_t transaction_num;
  /*! Port ID of the port on which this command is being sent. */
  uint8_t port_id;
  /*! The sub-device to which this command is addressed, or 0 for the root
   *  device. */
  uint16_t subdevice;
  /*! The command class for this command, one of the values from E1.20 Table
   *  A-1. */
  uint8_t command_class;
  /*! The RDM Parameter ID of this command. One of the values from E1.20
   *  Table A-3, or any of the relevant extension standards. */
  uint16_t param_id;
  /*! The length of the parameter data. */
  uint8_t datalen;
  /*! The parameter data. */
  uint8_t data[RDM_MAX_PDL];
} RdmCommand;

/*! A structure that represents an RDM response message. */
typedef struct RdmResponse
{
  /*! UID of the responder generating this response. */
  LwpaUid src_uid;
  /*! UID of the controller to which this response is addressed. */
  LwpaUid dest_uid;
  /*! Transaction number, copied from the corresponding command. */
  uint8_t transaction_num;
  /*! Response type, one of the values from E1.20 Table A-2. */
  uint8_t resp_type;
  /*! Current count of queued messages waiting to be retrieved. */
  uint8_t msg_count;
  /*! The sub-device generating this response, or 0 for the root device. */
  uint16_t subdevice;
  /*! The command class for this response, one of the values from E1.20 Table
   *  A-1. */
  uint8_t command_class;
  /*! The RDM Parameter ID of this response. One of the values from E1.20
   *  Table A-3, or any of the relevant extension standards. */
  uint16_t param_id;
  /*! The length of the parameter data. */
  uint8_t datalen;
  /*! The parameter data. */
  uint8_t data[RDM_MAX_PDL];
} RdmResponse;

#ifdef __cplusplus
extern "C" {
#endif

void rdm_pack_checksum(uint8_t *buffer, size_t datalen_without_checksum);
bool rdm_validate_msg(const RdmBuffer *buffer);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _RDMNET_RDMTYPES_H_ */
