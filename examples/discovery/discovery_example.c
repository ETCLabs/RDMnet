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

#include "etcpal/thread.h"
#include "rdmnet/discovery.h"
#include <stdio.h>
#include <string.h>

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

void monitorcb_broker_found(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  (void)handle;
  (void)context;

  printf("A Broker was found on scope %s\n", broker_info->scope);
  printf("Service Name: %s\n", broker_info->service_instance_name);
  for (EtcPalIpAddr* listen_addr = broker_info->listen_addrs;
       listen_addr < broker_info->listen_addrs + broker_info->num_listen_addrs; ++listen_addr)
  {
    char addr_str[ETCPAL_INET6_ADDRSTRLEN];
    etcpal_inet_ntop(listen_addr, addr_str, ETCPAL_INET6_ADDRSTRLEN);
    printf("Address: %s:%d\n", addr_str, broker_info->port);
  }
}

void monitorcb_broker_lost(rdmnet_scope_monitor_t handle, const char* scope, const char* service_instance_name,
                           void* context)
{
  (void)handle;
  (void)context;
  printf("Previously found Broker on scope %s with service instance name %s has been lost.\n", scope,
         service_instance_name);
}

void regcb_other_broker_found(rdmnet_registered_broker_t handle, const RdmnetBrokerDiscInfo* broker_info, void* context)
{
  (void)handle;
  (void)context;
  printf("A conflicting Broker was found on scope %s\n", broker_info->scope);
  printf("Service Name: %s\n", broker_info->service_instance_name);
  printf("Port: %d\n", broker_info->port);
}

void regcb_other_broker_lost(rdmnet_registered_broker_t handle, const char* scope, const char* service_name,
                             void* context)
{
  (void)handle;
  (void)context;
  printf("Previously found conflicting Broker on scope %s with service name %s has been lost.\n", scope, service_name);
}

void regcb_broker_registered(rdmnet_registered_broker_t handle, const char* assigned_service_name, void* context)
{
  (void)context;
  printf("Broker %p registered, assigned service name %s\n", handle, assigned_service_name);
}

void regcb_broker_register_failed(rdmnet_registered_broker_t handle, int platform_error, void* context)
{
  (void)context;
  printf("Broker %p register error %d!\n", handle, platform_error);
}

int main(int argc, char* argv[])
{
  rdmnet_init(NULL, NULL);

  if (argc == 2 && strcmp("broker", argv[1]) == 0)
  {
    // make a broker
    RdmnetBrokerRegisterConfig config = RDMNET_BROKER_REGISTER_CONFIG_DEFAULT_INIT;

    etcpal_generate_v4_uuid(&config.my_info.cid);
    config.my_info.service_instance_name = "UNIQUE NAME";
    config.my_info.model = "Broker prototype";
    config.my_info.manufacturer = "ETC";
    config.my_info.port = 0x4567;
    rdmnet_broker_register_config_set_callbacks(&config, regcb_broker_registered, regcb_broker_register_failed,
                                                regcb_other_broker_found, regcb_other_broker_lost, NULL);

    rdmnet_registered_broker_t handle;
    if (kEtcPalErrOk == rdmnet_disc_register_broker(&config, &handle))
    {
      printf("RDMnet Broker registration started; assigned handle %p\n", handle);
      printf("  Service Name: %s\n", config.my_info.service_instance_name);
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

    strcpy(config.scope, E133_DEFAULT_SCOPE);
    strcpy(config.domain, E133_DEFAULT_DOMAIN);
    set_monitor_callback_functions(&config.callbacks);
    config.callback_context = NULL;

    // start discovery
    rdmnet_scope_monitor_t handle;
    int platform_specific_error;
    if (kEtcPalErrOk == rdmnet_disc_start_monitoring(&config, &handle, &platform_specific_error))
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

  rdmnet_deinit();
  return 0;
}
