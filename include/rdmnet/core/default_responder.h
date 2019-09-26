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

/*! \file rdmnet/core/default_responder.h
 *  \brief Functions to help implement the default responder, to be used with the responder code in RDM.
 *  \author Christian Reese
 */

#ifndef _RDMNET_CORE_DEFAULT_RESPONDER_H_
#define _RDMNET_CORE_DEFAULT_RESPONDER_H_

#include "etcpal/bool.h"
#include "etcpal/int.h"

const int NUMBER_OF_BROKER_RDM_RESPONDER_HANDLERS = 8;
const int NUMBER_OF_CONTROLLER_RDM_RESPONDER_HANDLERS = 9;
const int NUMBER_OF_DEVICE_RDM_RESPONDER_HANDLERS = 9;

typedef struct DefaultResponderCallbacks
{
  /* RDM PID Callbacks */
  bool (*get_identify_state)(bool* state_out, uint16_t* nr_out_opt, void* context);
  bool (*set_identify_state)(bool state, uint16_t* nr_out_opt, void* context);
  bool (*get_parameter_description)(uint16_t pid_number_requested, ParameterDescription* description_out,
                                    uint16_t* nr_out_opt, void* context);
  bool (*get_manufacturer_label)(char* label_out, uint16_t* nr_out_opt, void* context);
  bool (*get_device_model_description)(char* description_out, uint16_t* nr_out_opt, void* context);
  bool (*get_software_version_label)(char* label_out, uint16_t* nr_out_opt, void* context);
  bool (*get_device_label)(char* label_out, uint16_t* nr_out_opt, void* context);
  bool (*set_device_label)(const char* label, uint16_t* nr_out_opt, void* context);

  /* RDMnet PID Callbacks */
  bool (*get_component_scope)(uint16_t scope_slot, ComponentScope* scope_out, uint16_t* nr_out_opt, void* context);
  bool (*set_component_scope)(const ComponentScope* scope, uint16_t* nr_out_opt, void* context);
  union
  {
    struct
    {
      bool (*get_broker_status)(bool* set_allowed_out, uint8_t* broker_state_out, uint16_t* nr_out_opt, void* context);
      bool (*set_broker_status)(uint8_t broker_state, uint16_t* nr_out_opt, void* context);
    } broker;

    struct
    {
      bool (*get_search_domain)(char* domain_out, uint16_t* nr_out_opt, void* context);
      bool (*set_search_domain)(const char* domain, uint16_t* nr_out_opt, void* context);
      bool (*get_tcp_comms_status)(uint16_t sequence, TcpCommsEntry* entry_out, bool* more_out, uint16_t* nr_out_opt,
                                   void* context);
      bool (*set_tcp_comms_status)(const char* scope, uint16_t* nr_out_opt, void* context);
    } controller;

    struct
    {
      bool (*get_search_domain)(char* domain_out, uint16_t* nr_out_opt, void* context);
      bool (*set_search_domain)(const char* domain, uint16_t* nr_out_opt, void* context);
      bool (*get_tcp_comms_status)(TcpCommsEntry* entry_out, uint16_t* nr_out_opt, void* context);
      bool (*set_tcp_comms_status)(const char* scope, uint16_t* nr_out_opt, void* context);
    } device;
  } component;
} DefaultResponderCallbacks;

bool rdmnet_defresp_init_broker(const DefaultResponderCallbacks* callbacks, void* callback_context,
                                RdmPidHandlerEntry* rdmresp_handler_array_out);
bool rdmnet_defresp_init_controller(const DefaultResponderCallbacks* callbacks, void* callback_context,
                                    RdmPidHandlerEntry* rdmresp_handler_array_out);
bool rdmnet_defresp_init_device(const DefaultResponderCallbacks* callbacks, void* callback_context,
                                RdmPidHandlerEntry* rdmresp_handler_array_out);

#endif /* _RDMNET_CORE_DEFAULT_RESPONDER_H_ */
