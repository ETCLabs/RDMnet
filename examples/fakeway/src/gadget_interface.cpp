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

#include "gadget_interface.h"
#include "GadgetDLL.h"

class Gadget
{
};
class GadgetImpl
{
private:
  GadgetNotify* notify_{nullptr};
};

bool GadgetInterface::Startup(GadgetNotify* notify)
{
  if (!Gadget2_Connect())
    return false;
}

void GadgetInterface::Shutdown()
{
  Gadget2_Disconnect();
}

void GadgetInterface::SendRdmCommand(unsigned int gadget_id, unsigned int port_number, const RDM_CmdC& cmd,
                                     const void* cookie)
{
}
