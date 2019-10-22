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

#pragma once

#include <string>
#include <vector>
#include <map>
#include "etcpal/cpp/lock.h"
#include "rdm/param_data.h"
#include "rdm/responder.h"
#include "rdmnet/defs.h"
#include "rdmnet/version.h"
#include "ControllerUtils.h"

#define CONTROLLER_HANDLER_ARRAY_SIZE 8

constexpr char kMyDeviceLabel[] = "ETC Example RDMnet Controller";
constexpr char kMyManufacturerLabel[] = "ETC";
constexpr char kMyDeviceModelDescription[] = "ETC Example RDMnet Controller";
constexpr char kMySoftwareVersionLabel[] = RDMNET_VERSION_STRING;

class ControllerResponderNotify
{
public:
  virtual void DeviceLabelChanged(const std::string& new_device_label){};
  virtual void IdentifyDeviceChanged(bool new_identify_device){};
  virtual void ComponentScopeChanged(uint16_t slot, const std::string& new_scope_string,
                                     const StaticBrokerConfig& new_static_broker){};
  virtual void SearchDomainChanged(const std::string& new_search_domain){};
  virtual void UnhealthyTcpEventsReset(const std::string& scope){};
};

class ControllerDefaultResponder
{
public:
  virtual ~ControllerDefaultResponder() = default;

  void InitResponder(ControllerResponderNotify* notify);

  etcpal_error_t ProcessCommand(const std::string& scope, const RdmCommand& cmd, RdmResponse& resp,
                                rdmresp_response_type_t& presp_type);

  etcpal_error_t ProcessGetRdmPdString(RdmPdString& string, const char* source, rdmresp_response_type_t& response_type,
                                       rdmpd_nack_reason_t& nack_reason);

  etcpal_error_t ProcessGetParameterDescription(uint16_t pid, RdmPdParameterDescription& description,
                                                rdmresp_response_type_t& response_type,
                                                rdmpd_nack_reason_t& nack_reason);
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
  etcpal_error_t ProcessGetSearchDomain(RdmPdSearchDomain& search_domain, rdmresp_response_type_t& response_type,
                                        rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessSetSearchDomain(const RdmPdSearchDomain& search_domain, rdmresp_response_type_t& response_type,
                                        rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessGetTcpCommsStatus(size_t overflow_index, RdmPdTcpCommsEntry& entry,
                                          rdmresp_response_type_t& response_type, rdmpd_nack_reason_t& nack_reason);
  etcpal_error_t ProcessSetTcpCommsStatus(const RdmPdScopeString& scope, rdmresp_response_type_t& response_type,
                                          rdmpd_nack_reason_t& nack_reason);

  bool Get(uint16_t pid, const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
           uint16_t& nack_reason);

  bool GetIdentifyDevice(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                         uint16_t& nack_reason) const;
  bool GetDeviceLabel(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                      uint16_t& nack_reason) const;
  bool GetComponentScope(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                         uint16_t& nack_reason) const;
  bool GetComponentScope(uint16_t slot, std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetSearchDomain(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                       uint16_t& nack_reason) const;
  bool GetTCPCommsStatus(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                         uint16_t& nack_reason) const;
  bool GetSupportedParameters(const uint8_t* param_data, uint8_t param_data_len,
                              std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetDeviceInfo(const uint8_t* param_data, uint8_t param_data_len, std::vector<RdmParamData>& resp_data_list,
                     uint16_t& nack_reason) const;
  bool GetManufacturerLabel(const uint8_t* param_data, uint8_t param_data_len,
                            std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetDeviceModelDescription(const uint8_t* param_data, uint8_t param_data_len,
                                 std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;
  bool GetSoftwareVersionLabel(const uint8_t* param_data, uint8_t param_data_len,
                               std::vector<RdmParamData>& resp_data_list, uint16_t& nack_reason) const;

  void UpdateSearchDomain(const std::string& new_search_domain);
  void AddScope(const std::string& new_scope, StaticBrokerConfig static_broker = StaticBrokerConfig());
  void RemoveScope(const std::string& scope_to_remove);
  void UpdateScopeConnectionStatus(const std::string& scope, bool connected,
                                   const EtcPalSockaddr& broker_addr = EtcPalSockaddr(),
                                   const RdmUid& controller_uid = RdmUid());
  void IncrementTcpUnhealthyCounter(const std::string& scope);
  void ResetTcpUnhealthyCounter(const std::string& scope);

private:
  struct ScopeEntry
  {
    ScopeEntry() {}
    ScopeEntry(StaticBrokerConfig sb) : static_broker(sb) {}

    std::string scope_string;
    uint16_t scope_slot{0};  // 0 is an invalid slot, and for this struct's purposes means "unassigned"

    uint16_t unhealthy_tcp_events{0};
    StaticBrokerConfig static_broker;
    bool connected{false};
    EtcPalSockaddr current_broker;
    RdmUid my_uid;
  };

  class ScopeMap
  {
  public:
    bool Find(const std::string& scope, ScopeEntry& entry) const;
    bool Find(uint16_t slot, ScopeEntry& entry) const;

    bool Remove(const std::string& scope);
    bool Remove(uint16_t slot);

    void Set(const ScopeEntry& entry);

    std::map<uint16_t, ScopeEntry>::const_iterator Begin() const;
    std::map<uint16_t, ScopeEntry>::const_iterator End() const;
    size_t Size() const;

  private:
    // Note that slot_map_ contains actual entries, string_map_ contains references.
    std::map<uint16_t, ScopeEntry> slot_map_;
    std::map<std::string, std::reference_wrapper<ScopeEntry>> string_map_;
  };

  // Property data lock
  mutable etcpal::RwLock prop_lock_;

  // Property data
  bool identifying_{false};
  std::string device_label_{kMyDeviceLabel};
  ScopeMap scopes_;
  std::string search_domain_{E133_DEFAULT_DOMAIN};
  static const std::vector<uint16_t> supported_parameters_;
  static const std::vector<uint8_t> device_info_;
  const std::string manufacturer_label_{kMyManufacturerLabel};
  const std::string device_model_description_{kMyDeviceModelDescription};
  const std::string software_version_label_{kMySoftwareVersionLabel};

  RdmResponderState rdm_responder_state_;
  RdmPidHandlerEntry handler_array_[CONTROLLER_HANDLER_ARRAY_SIZE];
  ControllerResponderNotify* notify_{nullptr};
};
