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

#include "etcpal/int.h"
#include "rdm/param_data.h"
#include "rdm/responder.h"
#include "broker_client.h"

// The Broker's RDM responder.
#ifndef _BROKER_RESPONDER_H_
#define _BROKER_RESPONDER_H_

#define BROKER_HANDLER_ARRAY_SIZE 7

class BrokerResponder
{
public:
  void InitResponder(const RdmUid& uid);
  etcpal_error_t ProcessPacket(const RdmBufferConstRef& buffer_in, RdmBufferRef& buffer_out,
                               rdmresp_response_type_t& response_type);

  etcpal_error_t ProcessGetRdmPdString(RdmPdString& string, const char* source, rdmresp_response_type_t& response_type,
                                       rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetParameterDescription(uint16_t pid, RdmPdParameterDescription& description,
                                                rdmresp_response_type_t& response_type, rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetDeviceModelDescription(RdmPdString& description, rdmresp_response_type_t& response_type,
                                                  rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetDeviceLabel(RdmPdString& label, rdmresp_response_type_t& response_type,
                                       rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessSetDeviceLabel(const RdmPdString& label, rdmresp_response_type_t& response_type,
                                       rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetSoftwareVersionLabel(RdmPdString& label, rdmresp_response_type_t& response_type,
                                                rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetIdentifyDevice(bool& identify_state, rdmresp_response_type_t& response_type,
                                          rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessSetIdentifyDevice(bool identify, rdmresp_response_type_t& response_type,
                                          rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetComponentScope(uint16_t slot, RdmPdComponentScope& component_scope,
                                          rdmresp_response_type_t& response_type, rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessSetComponentScope(const RdmPdComponentScope& component_scope,
                                          rdmresp_response_type_t& response_type, rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetBrokerStatus(RdmPdBrokerStatus& status, rdmresp_response_type_t& response_type,
                                        rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessSetBrokerStatus(rdmpd_broker_state_t state, rdmresp_response_type_t& response_type,
                                        rdmpd_nack_reason_t& nack_reason);

  //void ProcessRDMMessage(int conn, const RPTMessageRef& msg);
  //void SendRDMResponse(int conn, const RPTMessageRef& msg, uint8_t response_type, uint8_t command_class,
  //                     uint16_t param_id, uint8_t packed_len, uint8_t* pdata);

  //// Returns packed length
  //uint8_t PackGetParamDescResponsePD(uint8_t* pdata, uint16_t parameter, uint8_t pid_pdl_size, uint8_t param_cc,
  //                                   uint8_t param_data_type, const char* desc, uint32_t min_val, uint32_t max_val,
  //                                   uint32_t default_val);
  //void ProcessGetSupportedParameters(int conn, const RPTMessageRef& msg);
  //void ProcessGetParameterDescription(int conn, const RPTMessageRef& msg);
  //void ProcessGetSoftwareVersionLabel(int conn, const RPTMessageRef& msg);
  //void ProcessGetComponentScope(int conn, const RPTMessageRef& msg);
  //void ProcessSetComponentScope(int conn, const RPTMessageRef& msg);
  //void SendNack(int conn, const RPTMessageRef& msg, uint16_t pid, uint16_t reason, bool set_response);

private:
  RdmResponderState rdm_responder_state_;
  RdmPidHandlerEntry handler_array_[BROKER_HANDLER_ARRAY_SIZE];

  // Property data lock
  mutable etcpal_rwlock_t prop_lock_;
};

#endif  // _BROKER_RESPONDER_H_
