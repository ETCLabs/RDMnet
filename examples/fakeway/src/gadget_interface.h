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

#ifndef GADGET_INTERFACE_H_
#define GADGET_INTERFACE_H_

#include <memory>
#include <string>
#include "RdmDeviceInfo.h"
#include "uid.h"
#include "RDM_CmdC.h"
#include "fakeway_log.h"

class GadgetNotify
{
public:
  virtual void HandleGadgetConnected(unsigned int gadget_id, unsigned int num_ports) = 0;
  virtual void HandleGadgetDisconnected(unsigned int gadget_id) = 0;
  virtual void HandleNewRdmResponderDiscovered(unsigned int gadget_id, unsigned int port_number,
                                               const RdmDeviceInfo& info) = 0;
  virtual void HandleRdmResponderLost(unsigned int gadget_id, unsigned int port_number, uid id) = 0;
  virtual void HandleRdmResponse(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& response,
                                 const void* cookie) = 0;
  virtual void HandleRdmTimeout(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& orig_cmd,
                                const void* cookie) = 0;

  virtual void HandleGadgetLogMsg(const char* str) = 0;
};

class GadgetManager;

class GadgetInterface
{
public:
  GadgetInterface();
  virtual ~GadgetInterface();

  static std::string DllVersion();

  bool Startup(GadgetNotify& notify, FakewayLog& log);
  void Shutdown();

  void SendRdmCommand(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& cmd, const void* cookie);

private:
  std::unique_ptr<GadgetManager> impl_;
};

#endif  // GADGET_INTERFACE_H_
