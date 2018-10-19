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

#include "lwpa_int.h"
#include "rdmnet/broker/client.h"

// The Broker's RDM responder.
#ifndef _BROKER_RESPONDER_H_
#define _BROKER_RESPONDER_H_

class BrokerResponder
{
  void ProcessRDMMessage(int conn, const RPTMessageRef &msg);
  void SendRDMResponse(int conn, const RPTMessageRef &msg, uint8_t response_type, uint8_t command_class,
                       uint16_t param_id, uint8_t packed_len, uint8_t *pdata);

  // Returns packed length
  uint8_t PackGetParamDescResponsePD(uint8_t *pdata, uint16_t parameter, uint8_t pid_pdl_size, uint8_t param_cc,
                                     uint8_t param_data_type, const char *desc, uint32_t min_val, uint32_t max_val,
                                     uint32_t default_val);
  void ProcessGetSupportedParameters(int conn, const RPTMessageRef &msg);
  void ProcessGetParameterDescription(int conn, const RPTMessageRef &msg);
  void ProcessGetSoftwareVersionLabel(int conn, const RPTMessageRef &msg);
  void ProcessGetComponentScope(int conn, const RPTMessageRef &msg);
  void ProcessSetComponentScope(int conn, const RPTMessageRef &msg);
  void SendNack(int conn, const RPTMessageRef &msg, uint16_t pid, uint16_t reason, bool set_response);
};

#endif  // _BROKER_RESPONDER_H_
