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

#include "etcpal/thread.h"
#include "rdmnet/core.h"
#include "rdmnet/core/discovery.h"
#include "rdmnet/core/util.h"
#include <stdio.h>

void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  (void)handle;
  (void)context;

  printf("A Broker was found on scope %s\n", broker_info->scope);
  printf("Service Name: %s\n", broker_info->service_name);
  for (BrokerListenAddr* listen_addr = broker_info->listen_addr_list; listen_addr; listen_addr = listen_addr->next)
  {
    char addr_str[ETCPAL_INET6_ADDRSTRLEN];
    etcpal_inet_ntop(&listen_addr->addr, addr_str, ETCPAL_INET6_ADDRSTRLEN);
    printf("Address: %s:%d\n", addr_str, broker_info->port);
  }
}

void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char* scope, const char* service_name, void* context)
{
  (void)handle;
  (void)context;
  printf("Previously found Broker on scope %s with service name %s has been lost.\n", scope, service_name);
}

void monitorcb_scope_monitor_error(rdmnet_scope_monitor_t handle, const char* scope, int platform_error, void* context)
{
  (void)handle;
  (void)context;
  printf("Scope monitor error %d on scope %s\n", platform_error, scope);
}

void regcb_broker_found(rdmnet_registered_broker_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  (void)handle;
  (void)context;
  printf("A conflicting Broker was found on scope %s\n", broker_info->scope);
  printf("Service Name: %s\n", broker_info->service_name);
  printf("Port: %d\n", broker_info->port);
}

void regcb_broker_lost(rdmnet_registered_broker_t handle, const char* scope, const char* service_name, void* context)
{
  (void)handle;
  (void)context;
  printf("Previously found conflicting Broker on scope %s with service name %s has been lost.\n", scope, service_name);
}

void regcb_scope_monitor_error(rdmnet_registered_broker_t handle, const char* scope, int platform_error, void* context)
{
  (void)handle;
  (void)context;
  printf("Scope monitor error %d on scope %s\n", platform_error, scope);
}

void regcb_broker_registered(rdmnet_registered_broker_t handle, const char* assigned_service_name, void* context)
{
  (void)context;
  printf("Broker %p registered, assigned service name %s\n", handle, assigned_service_name);
}

void regcb_broker_register_error(rdmnet_registered_broker_t handle, int platform_error, void* context)
{
  (void)platform_error;
  (void)context;
  printf("Broker %p register error %d!\n", handle, platform_error);
}

void set_monitor_callback_functions(RdmnetScopeMonitorCallbacks* callbacks)
{
  callbacks->broker_found = monitorcb_broker_found;
  callbacks->broker_lost = monitorcb_broker_lost;
  callbacks->scope_monitor_error = monitorcb_scope_monitor_error;
}

void set_reg_callback_functions(RdmnetDiscBrokerCallbacks* callbacks)
{
  callbacks->broker_found = regcb_broker_found;
  callbacks->broker_lost = regcb_broker_lost;
  callbacks->scope_monitor_error = regcb_scope_monitor_error;
  callbacks->broker_registered = regcb_broker_registered;
  callbacks->broker_register_error = regcb_broker_register_error;
}

int main(int argc, char* argv[])
{
  rdmnet_core_init(NULL);

  if (argc == 2 && strcmp("broker", argv[1]) == 0)
  {
    // make a broker
    RdmnetBrokerRegisterConfig config;

    rdmnetdisc_fill_default_broker_info(&config.my_info);
    etcpal_generate_v4_uuid(&config.my_info.cid);
    rdmnet_safe_strncpy(config.my_info.service_name, "UNIQUE NAME", E133_SERVICE_NAME_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(config.my_info.model, "Broker prototype", E133_MODEL_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(config.my_info.manufacturer, "ETC", E133_MANUFACTURER_STRING_PADDED_LENGTH);
    config.my_info.port = 0x4567;
    set_reg_callback_functions(&config.callbacks);
    config.callback_context = NULL;

    rdmnet_registered_broker_t handle;
    if (kEtcPalErrOk == rdmnetdisc_register_broker(&config, &handle))
    {
      printf("RDMnet Broker registration started; assigned handle %p\n", handle);
      printf("  Service Name: %s\n", config.my_info.service_name);
      printf("  Port: %hu\n", config.my_info.port);
      printf("  Scope: %s\n", config.my_info.scope);
    }
    else
    {
      printf("Error during initial registration of RDMnet Broker.\n");
    }
  }
  else
  {
    RdmnetScopeMonitorConfig config;

    rdmnet_safe_strncpy(config.scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
    rdmnet_safe_strncpy(config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
    set_monitor_callback_functions(&config.callbacks);
    config.callback_context = NULL;

    // start discovery
    rdmnet_scope_monitor_t handle;
    int platform_specific_error;
    if (kEtcPalErrOk == rdmnetdisc_start_monitoring(&config, &handle, &platform_specific_error))
    {
      printf("Monitoring of scope %s started.\n", config.scope);
    }
    else
    {
      printf("Error (%d) during initial monitoring of scope %s.\n", platform_specific_error, config.scope);
    }
  }

  while (true)
  {
    etcpal_thread_sleep(500);
  }

  rdmnet_core_deinit();
  return 0;
}
