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

// bonjour_register: Test app which attempts to register a service with
// Bonjour.

#include "rdmnet/discovery.h"
#include <stdio.h>
#include <windows.h>

void
broker_found(const char *scope, const struct BrokerDiscInfo *broker_info,
  void *context)
{
  (void)scope;
  (void)context;
  printf("------broker_found------\n");

  printf("%s\n", broker_info->service_name);
  printf("%i\n", broker_info->port);
  printf("%s\n", broker_info->domain);
  printf("%s\n", broker_info->scope);
}

void
broker_lost(const char *service_name, void *context)
{
  (void)context;
  printf("------broker_lost------\n");

  printf("%s\n", service_name);
}

void
scope_monitor_error(const struct ScopeMonitorInfo *scope_info,
  int platform_error, void *context)
{
  (void)scope_info;
  (void)platform_error;
  (void)context;
  printf("------scope_monitor_error------\n");
}

void
broker_registered(const struct BrokerDiscInfo *broker_info,
  const char *assigned_service_name, void *context)
{
  (void)assigned_service_name;
  (void)context;
  printf("------broker_registered------\n");

  printf("%s\n", broker_info->service_name);
  printf("%i\n", broker_info->port);
  printf("%s\n", broker_info->domain);
  printf("%s\n", broker_info->scope);
}

void
broker_register_error(const struct BrokerDiscInfo *broker_info,
  int platform_error, void *context)
{
  (void)broker_info;
  (void)platform_error;
  (void)context;
  printf("------broker_register_error------\n");
}

void
set_callback_functions(struct RdmnetDiscCallbacks *callbacks)
{
  callbacks->broker_found = &broker_found;
  callbacks->broker_lost = &broker_lost;
  callbacks->scope_monitor_error = &scope_monitor_error;
  callbacks->broker_registered = &broker_registered;
  callbacks->broker_register_error = &broker_register_error;
}

int
main(int argc, char *argv[])
{
  struct RdmnetDiscCallbacks callbacks;
  struct ScopeMonitorInfo scope_monitor_info;
  struct BrokerDiscInfo broker_discovery_info;

  set_callback_functions(&callbacks);
  rdmnetdisc_init(&callbacks);

  fill_default_scope_info(&scope_monitor_info);
  fill_default_broker_info(&broker_discovery_info);

  uint8_t mac[6] = { 00, 0xc0, 0x16, 0xab, 0xbc, 0xcd };
  generate_cid(&broker_discovery_info.cid, "broker", mac, 1);
  strncpy(broker_discovery_info.service_name, "UNIQUE NAME TWO",
    E133_SERVICE_NAME_STRING_PADDED_LENGTH);
  strncpy(broker_discovery_info.model, "Broker prototype",
    E133_MODEL_STRING_PADDED_LENGTH);
  strncpy(broker_discovery_info.manufacturer, "ETC",
    E133_MANUFACTURER_STRING_PADDED_LENGTH);
  broker_discovery_info.port = 0x4567;

  int context = 12345;
  void *context_ptr = &context;
  int platform_specific_error;

  int flag = 0;
  int count = 0;

  if (argc == 2 && strcmp("broker", argv[1]) == 0)
  {
    /*make a broker*/
    rdmnetdisc_registerbroker(&broker_discovery_info, NULL);
  }
  else
  {
    /*start discovery*/
    rdmnetdisc_startmonitoring(&scope_monitor_info, &platform_specific_error, context_ptr);
    flag = 1;
  }

  //rdmnetdisc_registerbroker(&broker_discovery_info, NULL);

  while (true)
  {
    rdmnetdisc_tick(context_ptr);

    if (flag == 1)
    {
      count++;
      if (count == 10)
      {
        rdmnetdisc_deinit();
        break;
      }
    }

    Sleep(500);
  }

  printf("\n");
  getchar();
  return 0;
}

// printf("\ncalling poll - count: %d - %d %d %d\n", (int)nfds,
// outstanding_queries.count, outstanding_resolves.count,
// outstanding_addrs.count); printf("%#01x %#01x %s\n", fds[i].events,
// fds[i].revents, lwpa_strerror(fds[i].err));
