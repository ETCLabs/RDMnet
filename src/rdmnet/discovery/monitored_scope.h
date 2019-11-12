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

#ifndef MONITORED_SCOPE_H_
#define MONITORED_SCOPE_H_

typedef struct RdmnetScopeMonitorRef RdmnetScopeMonitorRef;

struct RdmnetScopeMonitorRef
{
  // The configuration data that the user provided.
  RdmnetScopeMonitorConfig config;
  // If this ScopeMonitorRef is associated with a registered Broker, that is tracked here. Otherwise
  // NULL.
  rdmnet_registered_broker_t broker_handle;
  // The list of Brokers discovered or being discovered on this scope.
  DiscoveredBroker* broker_list;
  // Platform-specific data stored with this monitor ref
  RdmnetScopeMonitorPlatformData platform_data;
  // The next ref in the list of scopes being monitored.
  RdmnetScopeMonitorRef* next;
};

#endif /* MONITORED_SCOPE_H_ */
