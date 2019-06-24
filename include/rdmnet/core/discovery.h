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

/*! \file rdmnet/core/discovery.h
 *  \brief Functions to discover a Broker and/or register a Broker for discovery. Uses mDNS and
 *         DNS-SD under the hood.
 */
#ifndef _RDMNET_DISCOVERY_H_
#define _RDMNET_DISCOVERY_H_

#include "lwpa/error.h"
#include "lwpa/uuid.h"
#include "lwpa/socket.h"
#include "rdmnet/defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RdmnetScopeMonitorRef *rdmnet_scope_monitor_t;
typedef struct RdmnetBrokerRegisterRef *rdmnet_registered_broker_t;

#define RDMNET_SCOPE_MONITOR_INVALID NULL
#define RDMNET_REGISTERED_BROKER_INVALID NULL

typedef struct BrokerListenAddr BrokerListenAddr;
struct BrokerListenAddr
{
  LwpaIpAddr addr;
  BrokerListenAddr *next;
};

typedef struct RdmnetBrokerDiscInfo
{
  LwpaUuid cid;
  char service_name[E133_SERVICE_NAME_STRING_PADDED_LENGTH];
  uint16_t port;
  BrokerListenAddr *listen_addr_list;
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char model[E133_MODEL_STRING_PADDED_LENGTH];
  char manufacturer[E133_MANUFACTURER_STRING_PADDED_LENGTH];
} RdmnetBrokerDiscInfo;

typedef struct RdmnetScopeMonitorCallbacks
{
  void (*broker_found)(rdmnet_scope_monitor_t handle, const RdmnetBrokerDiscInfo *broker_info, void *context);
  void (*broker_lost)(rdmnet_scope_monitor_t handle, const char *scope, const char *service_name, void *context);
  void (*scope_monitor_error)(rdmnet_scope_monitor_t handle, const char *scope, int platform_error, void *context);
} RdmnetScopeMonitorCallbacks;

typedef struct RdmnetScopeMonitorConfig
{
  char scope[E133_SCOPE_STRING_PADDED_LENGTH];
  char domain[E133_DOMAIN_STRING_PADDED_LENGTH];
  RdmnetScopeMonitorCallbacks callbacks;
  void *callback_context;
} RdmnetScopeMonitorConfig;

typedef struct RdmnetDiscBrokerCallbacks
{
  void (*broker_registered)(rdmnet_registered_broker_t handle, const char *assigned_service_name, void *context);
  void (*broker_register_error)(rdmnet_registered_broker_t handle, int platform_error, void *context);
  void (*broker_found)(rdmnet_registered_broker_t handle, const RdmnetBrokerDiscInfo *broker_info, void *context);
  void (*broker_lost)(rdmnet_registered_broker_t handle, const char *scope, const char *service_name, void *context);
  void (*scope_monitor_error)(rdmnet_registered_broker_t handle, const char *scope, int platform_error, void *context);
} RdmnetDiscBrokerCallbacks;

typedef struct RdmnetBrokerRegisterConfig
{
  RdmnetBrokerDiscInfo my_info;
  RdmnetDiscBrokerCallbacks callbacks;
  void *callback_context;
} RdmnetBrokerRegisterConfig;

void rdmnetdisc_fill_default_broker_info(RdmnetBrokerDiscInfo *broker_info);

lwpa_error_t rdmnetdisc_start_monitoring(const RdmnetScopeMonitorConfig *config, rdmnet_scope_monitor_t *handle,
                                         int *platform_specific_error);
lwpa_error_t rdmnetdisc_change_monitored_scope(rdmnet_scope_monitor_t handle,
                                               const RdmnetScopeMonitorConfig *new_config);
void rdmnetdisc_stop_monitoring(rdmnet_scope_monitor_t handle);
void rdmnetdisc_stop_monitoring_all();

lwpa_error_t rdmnetdisc_register_broker(const RdmnetBrokerRegisterConfig *config, rdmnet_registered_broker_t *handle);
void rdmnetdisc_unregister_broker(rdmnet_registered_broker_t handle);

void rdmnetdisc_tick();

#ifdef __cplusplus
}
#endif

#endif /* _RDMNET_DISCOVERY_H_ */
