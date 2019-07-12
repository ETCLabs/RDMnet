/******************************************************************************
************************* IMPORTANT NOTE -- READ ME!!! ************************
*******************************************************************************
* THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 REV. 77. UNDER NO
* CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR ANY PRODUCT AVAILABLE FOR
* GENERAL SALE TO THE PUBLIC. DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL
* VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE WILL **NOT**
* BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.
*******************************************************************************
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
*******************************************************************************
* This file is a part of RDMnet. For more information, go to:
* https://github.com/ETCLabs/RDMnet
******************************************************************************/

/*! \file rdmnet/core/ept_prot.h
 *  \brief Functions to pack, send, and parse EPT PDUs and their encapsulated messages.
 *  \author Sam Kearney
 */
#ifndef _RDMNET_CORE_EPT_PROT_H_
#define _RDMNET_CORE_EPT_PROT_H_

#include "lwpa/int.h"
#include "rdmnet/defs.h"

/*! \addtogroup rdmnet_message
 *  @{
 */

typedef struct EptDataMsg
{
  uint16_t manufacturer_id;
  uint16_t protocol_id;
  const uint8_t* data;
  size_t data_len;
} EptDataMsg;

typedef enum
{
  kEptStatusUnknownCid = VECTOR_EPT_STATUS_UNKNOWN_CID,
  kEptStatusUnknownVector = VECTOR_EPT_STATUS_UNKNOWN_VECTOR
} ept_status_code_t;

typedef struct EptStatusMsg
{
  /*! A status code that indicates the specific error or status condition. */
  ept_status_code_t status_code;
  /*! An optional implementation-defined status string to accompany this status message. */
  const char* status_string;
} EptStatusMsg;

/*! An EPT message. */
typedef struct EptMessage
{
  /*! The vector indicates which type of message is present in the data section. Valid values are
   *  indicated by VECTOR_EPT_* in rdmnet/defs.h. */
  uint32_t vector;
  union
  {
    EptDataMsg ept_data;
    EptStatusMsg ept_status;
  } data;
} EptMessage;

/*!@}*/

#endif /* _RDMNET_CORE_EPT_PROT_H_ */
