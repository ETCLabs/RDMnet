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

#include "broker/responder.h"
#include "estardm.h"
#include "rdmnet/connection.h"

void BrokerResponder::ProcessRDMMessage(int /*conn*/, const RPTMessageRef & /*msg*/)
{
  //  uint16_t pid =
  //      static_cast<uint16_t>(getParameterId(const_cast<rdmBuffer *>(&msg.msg)));
  //  int cmd_class = getCommandClass(const_cast<rdmBuffer *>(&msg.msg));

  //  switch (pid)
  //  {
  //    case E120_SUPPORTED_PARAMETERS:
  //      if (cmd_class == E120_GET_COMMAND)
  //        ProcessGetSupportedParameters(conn, msg);
  //      else
  //        SendNack(conn, msg, pid, E120_NR_UNSUPPORTED_COMMAND_CLASS, true);
  //      break;
  //    case E120_PARAMETER_DESCRIPTION:
  //      if (cmd_class == E120_GET_COMMAND)
  //        ProcessGetParameterDescription(conn, msg);
  //      else
  //        SendNack(conn, msg, pid, E120_NR_UNSUPPORTED_COMMAND_CLASS, true);
  //      break;
  //    case E120_SOFTWARE_VERSION_LABEL:
  //      if (cmd_class == E120_GET_COMMAND)
  //        ProcessGetSoftwareVersionLabel(conn, msg);
  //      else
  //        SendNack(conn, msg, pid, E120_NR_UNSUPPORTED_COMMAND_CLASS, true);
  //      break;
  //    case E133_COMPONENT_SCOPE:
  //      if (cmd_class == E120_GET_COMMAND)
  //        ProcessGetComponentScope(conn, msg);
  //      else if (cmd_class == E120_SET_COMMAND)
  //        ProcessSetComponentScope(conn, msg);
  //      else
  //        SendNack(conn, msg, pid, E120_NR_UNSUPPORTED_COMMAND_CLASS, true);
  //      break;
  //    default:
  //      SendNack(conn, msg, pid, E120_NR_UNKNOWN_PID, true);
  //  }
}

// void
// BrokerResponder::SendRDMResponse(int conn, const RPTMessageRef &msg,
//                                  uint8_t response_type, uint8_t
//                                  command_class, uint16_t param_id, uint8_t
//                                  packedlen, uint8_t *pdata)
// {
//   RptHeader resp_header = Broker::SwapHeaderData(msg.header);
//   uid               dest_uid = resp_end_data.dest_uid.Getuid();
//   uint16_t          sub_device =
//       static_cast<uint16_t>(getSubDeviceId(const_cast<rdmBuffer
//       *>(&msg.msg)));
//   rdmBuffer response;
//
//   if (NO_RDM_ERROR == RDMDvc_CreateResponse(&dest_uid, response_type, 0x00,
//                                             sub_device, command_class,
//                                             param_id, packedlen, pdata,
//                                             &m_port, &response))
//   {
//     std::vector<rdmBuffer> msgs;
//     msgs.push_back(response);
//
//     RDMnetSocket *psock = GetControllerSocket(cookie);
//     if (psock)
//     {
//       if (!RDMnet::SendRDMMessages(psock, m_settings.cid, resp_end_data,
//                                    msgs) &&
//           m_log)
//         lwpa_log(m_log, m_log_context, 1, LWPA_CAT_APP, LWPA_SEV_ERR,
//                  "Couldn't send a change notification -- how is psock
//                  null?");
//       ReleaseControllerSocket(psock);
//     }
//   }
// }

// void Broker::ProcessGetSupportedParameters(unsigned int cookie, const
// RPTMessageRef& msg)
//{
//  uint8_t packedlen = 0;
//  uint8_t pdata[min(RDM_MAX_BYTES, 0xFF)];

//  pack_16b((pdata + packedlen), E120_SUPPORTED_PARAMETERS);
//  packedlen += 2;
//  pack_16b((pdata + packedlen), E120_PARAMETER_DESCRIPTION);
//  packedlen += 2;
//  pack_16b((pdata + packedlen), E120_SOFTWARE_VERSION_LABEL);
//  packedlen += 2;
//  pack_16b((pdata + packedlen), E133_COMPONENT_SCOPE);
//  packedlen += 2;

//  SendRDMBrokerResponse(cookie, msg, E120_RESPONSE_TYPE_ACK,
//  E120_GET_COMMAND_RESPONSE, E120_SUPPORTED_PARAMETERS, packedlen,
//  pdata);
//}

// uint8_t Broker::PackGetParamDescResponsePD(uint8_t *pdata, uint16_t
// parameter, uint8_t pid_pdl_size, uint8_t param_cc, uint8_t
// param_data_type, const char *desc, uint32_t min_val, uint32_t max_val,
// uint32_t default_val)
//{
//  uint8_t packedlen = 0;
//  char description_str[32];
//  uint8_t description_str_len = 0;

//  description_str_len = static_cast<uint8_t>(strlen(desc));
//  memcpy(description_str, desc, description_str_len);

//  pack_16b((pdata + packedlen), parameter);
//  packedlen += 2;
//  Pack8B((pdata + packedlen), pid_pdl_size);
//  ++packedlen;
//  Pack8B((pdata + packedlen), param_data_type);
//  ++packedlen;
//  Pack8B((pdata + packedlen), param_cc);
//  ++packedlen;
//  Pack8B((pdata + packedlen), 0x00); // Meaningless Type field
//  ++packedlen;
//  Pack8B((pdata + packedlen), E120_UNITS_NONE);
//  ++packedlen;
//  Pack8B((pdata + packedlen), E120_PREFIX_NONE);
//  ++packedlen;
//  Pack32B((pdata + packedlen), min_val); // Min Valid Value
//  packedlen += 4;
//  Pack32B((pdata + packedlen), max_val); // Max Valid Value
//  packedlen += 4;
//  Pack32B((pdata + packedlen), default_val); // Default value
//  packedlen += 4;
//  memcpy((pdata + packedlen), description_str, description_str_len);
//  packedlen += description_str_len;

//  return packedlen;
//}

// void Broker::ProcessGetParameterDescription(unsigned int cookie, const
// RPTMessageRef& msg)
//{
//  uint8_t parameterData[RDM_MAX_BYTES];
//  uint16_t requestedParameterDescription;

//  uint8_t packedlen = 0;
//  uint8_t pdata[min(RDM_MAX_BYTES, 0xFF)];

//  getParameterData(const_cast<rdmBuffer*>(&msg.msg), parameterData);
//  requestedParameterDescription = Upack16B(parameterData);

//  switch (requestedParameterDescription)
//  {
//  case E120_SUPPORTED_PARAMETERS:
//    packedlen += PackGetParamDescResponsePD((pdata + packedlen),
//    requestedParameterDescription, 12,
//      E120_CC_GET, E120_DS_UNSIGNED_WORD, SUPPORTED_PARAMETERS_DESCSTR,
//      0x00000000, 0x0000FFFF, 0x00000000);
//    break;
//  case E120_PARAMETER_DESCRIPTION:
//    packedlen += PackGetParamDescResponsePD((pdata + packedlen),
//    requestedParameterDescription, 43,
//      E120_CC_GET, E120_DS_ASCII, PARAMETER_DESCRIPTION_DESCSTR,
//      0x00000000, 0x00000000, 0x00000000);
//    break;
//  case E120_SOFTWARE_VERSION_LABEL:
//    packedlen += PackGetParamDescResponsePD((pdata + packedlen),
//    requestedParameterDescription, 32,
//      E120_CC_GET, E120_DS_ASCII, SOFTWARE_VERSION_LABEL_DESCSTR,
//      0x00000000, 0x00000000, 0x00000000);
//    break;
//  case E133_COMPONENT_SCOPE:
//    packedlen += PackGetParamDescResponsePD((pdata + packedlen),
//    requestedParameterDescription, 63,
//      E120_CC_GET_SET, E120_DS_ASCII, RDMNET_CLIENT_SCOPE_DESCSTR,
//      0x00000000, 0x00000000, 0x00000000);
//    break;
//  default:
//    SendNack(cookie, msg, E120_PARAMETER_DESCRIPTION,
//    E120_NR_DATA_OUT_OF_RANGE, true); return;
//  }

//  SendRDMBrokerResponse(cookie, msg, E120_RESPONSE_TYPE_ACK,
//  E120_GET_COMMAND_RESPONSE, E120_PARAMETER_DESCRIPTION, packedlen,
//  pdata);
//}

// void Broker::ProcessGetSoftwareVersionLabel(unsigned int cookie, const
// RPTMessageRef& msg)
//{
//  uint8_t pdata[min(RDM_MAX_BYTES, 0xFF)];
//  uint8_t version_str_len =
//  static_cast<uint8_t>(strlen(BROKER_SOFTWARE_VERSION_LABEL));

//  memcpy(pdata, BROKER_SOFTWARE_VERSION_LABEL, version_str_len);
//  SendRDMBrokerResponse(cookie, msg, E120_RESPONSE_TYPE_ACK,
//  E120_GET_COMMAND_RESPONSE, E120_SOFTWARE_VERSION_LABEL,
//  version_str_len, pdata);
//}

// void Broker::ProcessGetRDMnetClientScope(unsigned int cookie, const
// RPTMessageRef& msg)
//{
//  uint8_t packedlen = 0;
//  uint8_t pdata[min(RDM_MAX_BYTES, 0xFF)];

//  broker_settings settings;
//  uint8_t scope_str_len = 0;

//  GetSettings(settings);
//  scope_str_len = static_cast<uint8_t>(strlen(settings.scope.c_str()));

//  memcpy((pdata + packedlen), settings.scope.c_str(), scope_str_len);
//  packedlen += scope_str_len;

//  SendRDMBrokerResponse(cookie, msg, E120_RESPONSE_TYPE_ACK,
//  E120_GET_COMMAND_RESPONSE, E133_RDMNET_CLIENT_SCOPES, packedlen,
//  pdata);
//}

// void Broker::ProcessSetRDMnetClientScope(unsigned int cookie, const
// RPTMessageRef& msg)
//{
//  const uint8_t pdlen =
//  static_cast<uint8_t>(getParameterDataLength(const_cast<rdmBuffer*>(&msg.msg)));

//  uint8_t parameterData[63];
//  std::string new_scope;

//  getParameterData(const_cast<rdmBuffer*>(&msg.msg), parameterData);
//  new_scope = std::string(parameterData, (parameterData + pdlen));

//  //Set the new scope as requested.
//  if (m_notify)
//  {
//    m_notify->ScopeChanged(new_scope);
//  }

//  SendRDMBrokerResponse(cookie, msg, E120_RESPONSE_TYPE_ACK,
//  E120_SET_COMMAND_RESPONSE, E133_RDMNET_CLIENT_SCOPES, 0, nullptr);
//}

// void Broker::SendNack(unsigned int cookie, const RPTMessageRef& msg,
// uint16_t pid, uint16_t reason, bool set_response)
//{
//  uint8_t packedlen = 0;
//  uint8_t pdata[min(RDM_MAX_BYTES, 0xFF)];

//  pack_16b(pdata, reason);
//  packedlen += 2;

//  SendRDMBrokerResponse(cookie, msg, E120_RESPONSE_TYPE_NACK_REASON,
//    (set_response ? E120_SET_COMMAND_RESPONSE :
//    E120_GET_COMMAND_RESPONSE), pid, packedlen, pdata);
//}
