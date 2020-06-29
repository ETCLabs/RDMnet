/******************************************************************************
 * Copyright 2020 ETC Inc.
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

#include "gadget_interface.h"

#include <map>
#include <vector>
#include "etcpal/cpp/lock.h"
#include "etcpal/cpp/thread.h"
#include "etcpal/cpp/timer.h"
#include "GadgetDLL.h"

#include <iostream>

bool operator<(const uid& a, const uid& b)
{
  if (a.manu == b.manu)
    return a.id < b.id;
  else
    return a.manu < b.manu;
}

struct GadgetRdmCommand
{
  RDM_CmdC      cmd;
  unsigned int  port_number;
  const void*   cookie;
  etcpal::Timer timeout;
};

struct RdmResponder
{
  RdmDeviceInfo info;
  unsigned int  gadget_id;

  RdmResponder(const RdmDeviceInfo& info_in, unsigned int gadget_id_in) : info(info_in), gadget_id(gadget_id_in) {}
};

struct Gadget
{
  unsigned int id;
  unsigned int num_ports;

  std::vector<GadgetRdmCommand> commands;

  Gadget(unsigned int id_in, unsigned int num_ports_in) : id(id_in), num_ports(num_ports_in) {}
};

class GadgetManager
{
public:
  bool Startup(GadgetNotify& notify, etcpal::Logger& logger);
  void Shutdown();
  void Run();

  void SendRdmCommand(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& cmd, const void* cookie);

private:
  GadgetNotify*  notify_{nullptr};
  bool           running_{false};
  etcpal::Thread thread_;

  etcpal::Mutex                  gadget_lock_;
  std::map<unsigned int, Gadget> gadgets_;
  std::map<uid, RdmResponder>    responders_;

  unsigned int previous_number_of_devices_{0};
  int          last_gadget_num_{-1};

  void ResolveGadgetChanges();
  void ResolveRdmResponderChanges();
  void CheckForRdmResponses();
  void CheckForUnsolicitedRdmResponses();
  bool GetResponseFromQueue(const void* cookie, RDM_CmdC& response);
};

static etcpal::Logger* log_instance{nullptr};

extern "C" void __stdcall GadgetLogCallback(const char* LogData)
{
  if (log_instance)
    log_instance->Info(LogData);
}

bool GadgetManager::Startup(GadgetNotify& notify, etcpal::Logger& logger)
{
  notify_ = &notify;
  log_instance = &logger;

  Gadget2_SetLogCallback(GadgetLogCallback);
  if (!Gadget2_Connect())
    return false;

  running_ = true;
  if (!thread_.Start(&GadgetManager::Run, this))
  {
    Gadget2_Disconnect();
    return false;
  }

  return true;
}

void GadgetManager::Shutdown()
{
  running_ = false;
  thread_.Join();
  Gadget2_Disconnect();
  log_instance = nullptr;
}

void GadgetManager::Run()
{
  while (running_)
  {
    ResolveGadgetChanges();

    ResolveRdmResponderChanges();
    CheckForRdmResponses();
    CheckForUnsolicitedRdmResponses();

    etcpal_thread_sleep(10);
  }
}

void GadgetManager::SendRdmCommand(unsigned int    gadget_id,
                                   unsigned int    port_number,
                                   const RDM_CmdC& cmd,
                                   const void*     cookie)
{
  etcpal::MutexGuard gadget_guard(gadget_lock_);

  auto gadget = gadgets_.find(gadget_id);
  if (gadget != gadgets_.end() && port_number <= gadget->second.num_ports)
  {
    GadgetRdmCommand gadget_cmd;
    gadget_cmd.cmd = cmd;
    gadget_cmd.port_number = port_number;
    gadget_cmd.cookie = cookie;
    gadget_cmd.timeout.Start(5000);

    Gadget2_SendRDMCommandWithContext(gadget_id, port_number, cmd.getCommand(), cmd.getParameter(), cmd.getSubdevice(),
                                      cmd.getLength(), reinterpret_cast<const char*>(cmd.getBuffer()),
                                      cmd.getManufacturerId(), cmd.getDeviceId(), cookie);
    gadget->second.commands.push_back(gadget_cmd);
  }
}

// This behavior of the Gadget DLL interface is currently undocumented - the gadget IDs are
// monotonically increasing, so each new gadget gets the next higher number. There might be gaps
// in the ID numbers.
//
// TODO make this better - requires asking for changes in the DLL interface.
void GadgetManager::ResolveGadgetChanges()
{
  etcpal::MutexGuard gadget_guard(gadget_lock_);

  auto num_devices = Gadget2_GetNumGadgetDevices();
  if (num_devices != previous_number_of_devices_)
  {
    auto gadget_iter = gadgets_.begin();
    while (gadget_iter != gadgets_.end())
    {
      if (Gadget2_GetPortCount(gadget_iter->second.id) == 0)
      {
        unsigned int removed_id = gadget_iter->second.id;

        // This gadget has gone away.
        notify_->HandleGadgetDisconnected(removed_id);
        gadget_iter = gadgets_.erase(gadget_iter);

        // Remove the RDM responders associated with this gadget.
        auto resp_iter = responders_.begin();
        while (resp_iter != responders_.end())
        {
          if (resp_iter->second.gadget_id == removed_id)
            resp_iter = responders_.erase(resp_iter);
          else
            ++resp_iter;
        }
      }
    }

    for (int current_id = last_gadget_num_ + 1; Gadget2_GetPortCount(current_id) != 0; ++current_id)
    {
      last_gadget_num_ = current_id;
      unsigned int port_count = Gadget2_GetPortCount(current_id);

      gadgets_.insert(std::make_pair(current_id, Gadget(current_id, port_count)));
      notify_->HandleGadgetConnected(current_id, port_count);

      for (unsigned int i = 1; i <= port_count; ++i)
      {
        Gadget2_SetRDMEnabled(current_id, i, 1);
        Gadget2_DoFullDiscovery(current_id, i);
      }
    }
    previous_number_of_devices_ = num_devices;
  }
}

void GadgetManager::ResolveRdmResponderChanges()
{
  etcpal::MutexGuard gadget_guard(gadget_lock_);

  // Make a copy of the current responder list
  auto responders_copy = responders_;

  unsigned int num_responders = Gadget2_GetDiscoveredDevices();
  for (unsigned int i = 0; i < num_responders; ++i)
  {
    unsigned int gadget_id = Gadget2_GetGadgetForDevice(i);
    if (gadgets_.find(gadget_id) == gadgets_.end())
      continue;

    RdmDeviceInfo* responder = Gadget2_GetDeviceInfo(i);
    if (responder)
    {
      uid  resp_id{responder->manufacturer_id, responder->device_id};
      auto existing_responder = responders_copy.find(resp_id);
      if (existing_responder != responders_copy.end())
      {
        // This responder is still here.
        responders_copy.erase(existing_responder);
      }
      else
      {
        auto new_resp = RdmResponder(*responder, gadget_id);
        responders_.insert(std::make_pair(resp_id, new_resp));
        notify_->HandleNewRdmResponderDiscovered(gadget_id, new_resp.info.port_number, new_resp.info);
      }
    }
  }

  // Remove the responders that have gone away from the map
  for (const auto& lost_responder : responders_copy)
  {
    notify_->HandleRdmResponderLost(lost_responder.second.gadget_id, lost_responder.second.info.port_number,
                                    lost_responder.first);
    responders_.erase(lost_responder.first);
  }
}

void GadgetManager::CheckForRdmResponses()
{
  etcpal::MutexGuard gadget_guard(gadget_lock_);

  for (auto& gadget : gadgets_)
  {
    auto command_iter = gadget.second.commands.begin();
    while (command_iter != gadget.second.commands.end())
    {
      RDM_CmdC response;
      if (GetResponseFromQueue(command_iter->cookie, response))
      {
        notify_->HandleRdmResponse(gadget.second.id, command_iter->port_number, response, command_iter->cookie);
        command_iter = gadget.second.commands.erase(command_iter);
      }
      else if (command_iter->timeout.IsExpired())
      {
        notify_->HandleRdmTimeout(gadget.second.id, command_iter->port_number, command_iter->cmd, command_iter->cookie);
        command_iter = gadget.second.commands.erase(command_iter);
      }
      else
      {
        // Else we haven't gotten a response yet. Keep waiting
        ++command_iter;
      }
    }
  }
}

bool GadgetManager::GetResponseFromQueue(const void* cookie, RDM_CmdC& response)
{
  unsigned int num_responses = Gadget2_GetNumResponses();
  for (unsigned int i = 0; i < num_responses; ++i)
  {
    auto resp_ref = Gadget2_GetResponse(i);
    auto context = Gadget2_GetResponseContext(i);
    if (context == cookie)
    {
      response = *resp_ref;
      Gadget2_ClearResponse(i);
      return true;
    }
  }
  return false;
}

void GadgetManager::CheckForUnsolicitedRdmResponses()
{
  etcpal::MutexGuard gadget_guard(gadget_lock_);

  std::vector<unsigned int> responses_handled;

  unsigned int num_responses = Gadget2_GetNumResponses();
  for (unsigned int i = 0; i < num_responses; ++i)
  {
    if (Gadget2_GetResponseContext(i) == nullptr)
    {
      auto resp_ref = Gadget2_GetResponse(i);
      auto responder = responders_.find(uid{resp_ref->getManufacturerId(), resp_ref->getDeviceId()});
      if (responder != responders_.end())
      {
        notify_->HandleRdmResponse(responder->second.gadget_id, responder->second.info.port_number, *resp_ref, nullptr);
        responses_handled.push_back(i);
      }
    }
  }

  for (auto to_clear : responses_handled)
  {
    Gadget2_ClearResponse(to_clear);
  }
}

GadgetInterface::GadgetInterface() : impl_(std::make_unique<GadgetManager>())
{
}

GadgetInterface::~GadgetInterface() = default;

std::string GadgetInterface::DllVersion()
{
  return Gadget2_GetDllVersion();
}

bool GadgetInterface::Startup(GadgetNotify& notify, etcpal::Logger& logger)
{
  return impl_->Startup(notify, logger);
}

void GadgetInterface::Shutdown()
{
  impl_->Shutdown();
}

void GadgetInterface::SendRdmCommand(unsigned int    gadget_id,
                                     unsigned int    port_number,
                                     const RDM_CmdC& cmd,
                                     const void*     cookie)
{
  impl_->SendRdmCommand(gadget_id, port_number, cmd, cookie);
}
