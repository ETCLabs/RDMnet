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

#include <string>
#include <algorithm>

#include "etcpal_mock/socket.h"
#include "etcpal/cpp/inet.h"
#include "rdmnet/core/util.h"
#include "rdmnet_mock/core.h"
#include "rdmnet_mock/private/core.h"
#include "disc_common.h"
#include "dns_sd.h"
#include "test_operators.h"

#include "gtest/gtest.h"
#include "fff.h"

DEFINE_FFF_GLOBALS;

// Mocking the dns_sd.h interface
FAKE_VALUE_FUNC(dnssd_sock_t, DNSServiceRefSockFD, DNSServiceRef);
FAKE_VALUE_FUNC(DNSServiceErrorType, DNSServiceProcessResult, DNSServiceRef);
FAKE_VOID_FUNC(DNSServiceRefDeallocate, DNSServiceRef);
FAKE_VALUE_FUNC(DNSServiceErrorType, DNSServiceRegister, DNSServiceRef*, DNSServiceFlags, uint32_t, const char*,
                const char*, const char*, const char*, uint16_t, uint16_t, const void*, DNSServiceRegisterReply, void*);
FAKE_VALUE_FUNC(DNSServiceErrorType, DNSServiceBrowse, DNSServiceRef*, DNSServiceFlags, uint32_t, const char*,
                const char*, DNSServiceBrowseReply, void*);
FAKE_VALUE_FUNC(DNSServiceErrorType, DNSServiceResolve, DNSServiceRef*, DNSServiceFlags, uint32_t, const char*,
                const char*, const char*, DNSServiceResolveReply, void*);
FAKE_VALUE_FUNC(DNSServiceErrorType, DNSServiceGetAddrInfo, DNSServiceRef*, DNSServiceFlags, uint32_t,
                DNSServiceProtocol, const char*, DNSServiceGetAddrInfoReply, void*);

// Mocking the C callback function pointers
FAKE_VOID_FUNC(regcb_broker_registered, rdmnet_registered_broker_t, const char*, void*);
FAKE_VOID_FUNC(regcb_broker_register_error, rdmnet_registered_broker_t, int, void*);
FAKE_VOID_FUNC(regcb_broker_found, rdmnet_registered_broker_t, const RdmnetBrokerDiscInfo*, void*);
FAKE_VOID_FUNC(regcb_broker_lost, rdmnet_registered_broker_t, const char*, const char*, void*);
FAKE_VOID_FUNC(regcb_scope_monitor_error, rdmnet_registered_broker_t, const char*, int, void*);

FAKE_VOID_FUNC(monitorcb_broker_found, rdmnet_scope_monitor_t, const RdmnetBrokerDiscInfo*, void*);
FAKE_VOID_FUNC(monitorcb_broker_lost, rdmnet_scope_monitor_t, const char*, const char*, void*);
FAKE_VOID_FUNC(monitorcb_scope_monitor_error, rdmnet_scope_monitor_t, const char*, int, void*);

static void set_reg_callbacks(RdmnetDiscBrokerCallbacks* callbacks)
{
  callbacks->broker_found = regcb_broker_found;
  callbacks->broker_lost = regcb_broker_lost;
  callbacks->scope_monitor_error = regcb_scope_monitor_error;
  callbacks->broker_registered = regcb_broker_registered;
  callbacks->broker_register_error = regcb_broker_register_error;
}

static void set_monitor_callbacks(RdmnetScopeMonitorCallbacks* callbacks)
{
  callbacks->broker_found = monitorcb_broker_found;
  callbacks->broker_lost = monitorcb_broker_lost;
  callbacks->scope_monitor_error = monitorcb_scope_monitor_error;
}

class TestDiscoveryBonjour : public ::testing::Test
{
public:
  // clang-format off
  RdmnetBrokerDiscInfo default_discovered_broker_ =
  {
    {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}},
    "Test Service Name",
    8888,
    nullptr,
    0,
    "default",
    "Test Broker",
    "ETC"
  };
  // clang-format on
protected:
  std::string default_full_service_name_;
  etcpal_error_t init_result_;
  TXTRecordRef txt_record_;
  rdmnet_scope_monitor_t monitor_handle_;
  std::unique_ptr<EtcPalIpAddr> default_listen_addr_;

  void SetUp() override
  {
    // Reset fff state
    RESET_FAKE(DNSServiceRefSockFD);
    RESET_FAKE(DNSServiceProcessResult);
    RESET_FAKE(DNSServiceRefDeallocate);
    RESET_FAKE(DNSServiceRegister);
    RESET_FAKE(DNSServiceBrowse);
    RESET_FAKE(DNSServiceResolve);
    RESET_FAKE(DNSServiceGetAddrInfo);

    RESET_FAKE(regcb_broker_registered);
    RESET_FAKE(regcb_broker_register_error);
    RESET_FAKE(regcb_broker_found);
    RESET_FAKE(regcb_broker_lost);
    RESET_FAKE(regcb_scope_monitor_error);
    RESET_FAKE(monitorcb_broker_found);
    RESET_FAKE(monitorcb_broker_lost);
    RESET_FAKE(monitorcb_scope_monitor_error);

    etcpal_socket_reset_all_fakes();

    FFF_RESET_HISTORY();

    init_result_ = rdmnet_disc_init(nullptr);
    rdmnet_core_initialized_fake.return_val = true;

    CreateDefaultBroker();
  }

  void TearDown() override
  {
    TXTRecordDeallocate(&txt_record_);
    rdmnet_disc_deinit();
  }

  void CreateDefaultBroker();
  DNSServiceBrowseReply MonitorDefaultScope();
  DNSServiceResolveReply DriveBrowseCallback(DNSServiceBrowseReply browse_cb);
  DNSServiceGetAddrInfoReply DriveResolveCallback(DNSServiceResolveReply resolve_cb);
  void DriveGetAddrInfoCallback(DNSServiceGetAddrInfoReply gai_cb);
};

void TestDiscoveryBonjour::CreateDefaultBroker()
{
  default_listen_addr_ = std::make_unique<EtcPalIpAddr>(etcpal::IpAddr::FromString("10.101.1.1").get());
  default_discovered_broker_.listen_addrs = default_listen_addr_.get();
  default_discovered_broker_.num_listen_addrs = 1;

  TXTRecordCreate(&txt_record_, 0, nullptr);
  std::string txtvers = std::to_string(E133_DNSSD_TXTVERS);
  ASSERT_EQ(kDNSServiceErr_NoError,
            TXTRecordSetValue(&txt_record_, "TxtVers", static_cast<uint8_t>(txtvers.length()), txtvers.c_str()));
  std::string e133vers = std::to_string(E133_DNSSD_E133VERS);
  ASSERT_EQ(kDNSServiceErr_NoError,
            TXTRecordSetValue(&txt_record_, "E133Vers", static_cast<uint8_t>(e133vers.length()), e133vers.c_str()));

  // CID with the hyphens removed
  char cid_buf[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&default_discovered_broker_.cid, cid_buf);
  std::string cid_str(cid_buf);
  cid_str.erase(std::remove(cid_str.begin(), cid_str.end(), '-'), cid_str.end());
  ASSERT_EQ(kDNSServiceErr_NoError,
            TXTRecordSetValue(&txt_record_, "CID", static_cast<uint8_t>(cid_str.length()), cid_str.c_str()));

  ASSERT_EQ(kDNSServiceErr_NoError,
            TXTRecordSetValue(&txt_record_, "ConfScope", static_cast<uint8_t>(strlen(default_discovered_broker_.scope)),
                              default_discovered_broker_.scope));
  ASSERT_EQ(kDNSServiceErr_NoError,
            TXTRecordSetValue(&txt_record_, "Model", static_cast<uint8_t>(strlen(default_discovered_broker_.model)),
                              default_discovered_broker_.model));
  ASSERT_EQ(
      kDNSServiceErr_NoError,
      TXTRecordSetValue(&txt_record_, "Manuf", static_cast<uint8_t>(strlen(default_discovered_broker_.manufacturer)),
                        default_discovered_broker_.manufacturer));

  default_full_service_name_ = "Test Service Name.";
  default_full_service_name_ += E133_DNSSD_SRV_TYPE;
  default_full_service_name_ += E133_DEFAULT_DOMAIN;
}

// These need to be macros because of the way we are using non-capturing lambdas in various tests
#define DEFAULT_MONITOR_SOCKET_VAL 1
// This one needs to be a macro because of C function pointer/lambda restrictions
#define DEFAULT_MONITOR_DNS_REF reinterpret_cast<DNSServiceRef>(2)

DNSServiceBrowseReply TestDiscoveryBonjour::MonitorDefaultScope()
{
  RdmnetScopeMonitorConfig config;
  rdmnet_safe_strncpy(config.scope, E133_DEFAULT_SCOPE, E133_SCOPE_STRING_PADDED_LENGTH);
  rdmnet_safe_strncpy(config.domain, E133_DEFAULT_DOMAIN, E133_DOMAIN_STRING_PADDED_LENGTH);
  set_monitor_callbacks(&config.callbacks);
  config.callback_context = this;

  // Assign a socket value to our service browse operation

  auto previous_call_count = DNSServiceBrowse_fake.call_count;

  // Set up the fakes called by rdmnet_disc_start_monitoring
  DNSServiceRefSockFD_fake.return_val = DEFAULT_MONITOR_SOCKET_VAL;
  DNSServiceBrowse_fake.custom_fake = [](DNSServiceRef* ref, DNSServiceFlags, uint32_t, const char*, const char*,
                                         DNSServiceBrowseReply, void*) -> DNSServiceErrorType {
    *ref = DEFAULT_MONITOR_DNS_REF;
    return kDNSServiceErr_NoError;
  };

  int platform_specific_err;
  EXPECT_EQ(kEtcPalErrOk, rdmnet_disc_start_monitoring(&config, &monitor_handle_, &platform_specific_err));
  EXPECT_EQ(DNSServiceBrowse_fake.call_count, previous_call_count + 1);

  return DNSServiceBrowse_fake.arg5_val;
}

DNSServiceResolveReply TestDiscoveryBonjour::DriveBrowseCallback(DNSServiceBrowseReply browse_cb)
{
  auto previous_call_count = DNSServiceResolve_fake.call_count;

  DNSServiceResolve_fake.custom_fake = [](DNSServiceRef* ref, DNSServiceFlags, uint32_t, const char*, const char*,
                                          const char*, DNSServiceResolveReply, void*) -> DNSServiceErrorType {
    *ref = DEFAULT_MONITOR_DNS_REF;
    return kDNSServiceErr_NoError;
  };
  browse_cb(DEFAULT_MONITOR_DNS_REF, kDNSServiceFlagsAdd, 0, kDNSServiceErr_NoError,
            default_discovered_broker_.service_name, E133_DNSSD_SRV_TYPE, E133_DEFAULT_DOMAIN,
            DNSServiceBrowse_fake.arg6_val);
  EXPECT_EQ(DNSServiceResolve_fake.call_count, previous_call_count + 1);
  return DNSServiceResolve_fake.arg6_val;
}

DNSServiceGetAddrInfoReply TestDiscoveryBonjour::DriveResolveCallback(DNSServiceResolveReply resolve_cb)
{
  auto previous_call_count = DNSServiceGetAddrInfo_fake.call_count;

  DNSServiceGetAddrInfo_fake.custom_fake = [](DNSServiceRef* ref, DNSServiceFlags, uint32_t, DNSServiceProtocol,
                                              const char*, DNSServiceGetAddrInfoReply, void*) -> DNSServiceErrorType {
    *ref = DEFAULT_MONITOR_DNS_REF;
    return kDNSServiceErr_NoError;
  };
  resolve_cb(DEFAULT_MONITOR_DNS_REF, 0, 0, kDNSServiceErr_NoError, default_full_service_name_.c_str(), "testhost",
             htons(default_discovered_broker_.port), TXTRecordGetLength(&txt_record_),
             reinterpret_cast<const unsigned char*>(TXTRecordGetBytesPtr(&txt_record_)),
             DNSServiceResolve_fake.arg7_val);
  EXPECT_EQ(DNSServiceGetAddrInfo_fake.call_count, previous_call_count + 1);
  return DNSServiceGetAddrInfo_fake.arg5_val;
}

void TestDiscoveryBonjour::DriveGetAddrInfoCallback(DNSServiceGetAddrInfoReply gai_cb)
{
  struct sockaddr address;
  EtcPalSockAddr discovered_addr;
  discovered_addr.ip = *default_listen_addr_;
  discovered_addr.port = 0;
  sockaddr_etcpal_to_os(&discovered_addr, &address);
  gai_cb(DEFAULT_MONITOR_DNS_REF, 0, 0, kDNSServiceErr_NoError, "testhost", &address, 10,
         DNSServiceGetAddrInfo_fake.arg6_val);
}

TEST_F(TestDiscoveryBonjour, InitWorks)
{
  EXPECT_EQ(init_result_, kEtcPalErrOk);
}

// Test that rdmnet_disc_register_broker() behaves properly with invalid input data.
TEST_F(TestDiscoveryBonjour, RegisterBrokerInvalidCallsFail)
{
  RdmnetBrokerRegisterConfig config;

  config.my_info.cid = kEtcPalNullUuid;
  config.my_info.service_name[0] = '\0';
  config.my_info.scope[0] = '\0';
  config.my_info.listen_addrs = nullptr;
  config.my_info.num_listen_addrs = 0;
  set_reg_callbacks(&config.callbacks);
  config.callback_context = this;

  rdmnet_registered_broker_t handle;
  EXPECT_NE(kEtcPalErrOk, rdmnet_disc_register_broker(&config, &handle));
  EXPECT_EQ(regcb_broker_registered_fake.call_count, 0u);
  EXPECT_EQ(DNSServiceRegister_fake.call_count, 0u);
}

// Test that rdmnet_disc_tick() functions properly in the presence of various states of monitored
// scopes.
TEST_F(TestDiscoveryBonjour, TickHandlesSocketActivity)
{
  MonitorDefaultScope();

  EXPECT_GE(DNSServiceRefSockFD_fake.call_count, 1u);
  EXPECT_EQ(etcpal_poll_add_socket_fake.call_count, 1u);
  EXPECT_EQ(etcpal_poll_add_socket_fake.arg2_history[0], ETCPAL_POLL_IN);

  // Tick should call etcpal_poll_wait
  etcpal_poll_wait_fake.return_val = kEtcPalErrTimedOut;
  DNSServiceProcessResult_fake.return_val = kDNSServiceErr_NoError;

  rdmnet_disc_tick();
  EXPECT_EQ(etcpal_poll_wait_fake.call_count, 1u);
  EXPECT_EQ(DNSServiceProcessResult_fake.call_count, 0u);

  // If a socket has activity, DNSServiceProcessResult should be called with that socket.
  etcpal_poll_wait_fake.custom_fake = [](EtcPalPollContext* context, EtcPalPollEvent* event, int) -> etcpal_error_t {
    EXPECT_NE(context, nullptr);
    EXPECT_NE(event, nullptr);
    if (event)
    {
      event->events = ETCPAL_POLL_IN;
      event->err = kEtcPalErrOk;
      event->socket = DEFAULT_MONITOR_SOCKET_VAL;
      event->user_data = etcpal_poll_add_socket_fake.arg3_history[0];
    }
    return kEtcPalErrOk;
  };
  rdmnet_disc_tick();
  EXPECT_EQ(DNSServiceProcessResult_fake.call_count, 1u);
  EXPECT_EQ(DNSServiceProcessResult_fake.arg0_history[0], DEFAULT_MONITOR_DNS_REF);
}

TEST_F(TestDiscoveryBonjour, NormalResolveWorksCorrectly)
{
  // Drive the state machine by calling the appropriate callbacks
  auto browse_cb = MonitorDefaultScope();
  auto resolve_cb = DriveBrowseCallback(browse_cb);

  ASSERT_EQ(DNSServiceResolve_fake.call_count, 1u);
  EXPECT_EQ(DNSServiceResolve_fake.arg2_val, 0u);
  EXPECT_STREQ(DNSServiceResolve_fake.arg3_val, default_discovered_broker_.service_name);
  EXPECT_STREQ(DNSServiceResolve_fake.arg4_val, E133_DNSSD_SRV_TYPE);
  EXPECT_STREQ(DNSServiceResolve_fake.arg5_val, E133_DEFAULT_DOMAIN);

  // DNSServiceResolveReply
  auto gai_cb = DriveResolveCallback(resolve_cb);

  ASSERT_EQ(DNSServiceGetAddrInfo_fake.call_count, 1u);
  EXPECT_EQ(DNSServiceGetAddrInfo_fake.arg2_val, 0u);
  EXPECT_STREQ(DNSServiceGetAddrInfo_fake.arg4_val, "testhost");

  monitorcb_broker_found_fake.custom_fake = [](rdmnet_scope_monitor_t, const RdmnetBrokerDiscInfo* broker_info,
                                               void* context) {
    TestDiscoveryBonjour* test_fixture = static_cast<TestDiscoveryBonjour*>(context);
    ASSERT_TRUE(test_fixture != nullptr);
    ASSERT_TRUE(broker_info != nullptr);
    EXPECT_EQ(test_fixture->default_discovered_broker_, *broker_info);
  };
  DriveGetAddrInfoCallback(gai_cb);

  ASSERT_EQ(monitorcb_broker_found_fake.call_count, 1u);
  EXPECT_EQ(monitorcb_broker_found_fake.arg0_val, monitor_handle_);
}

// Test that a discovered broker is cleaned up properly after going through the entire resolution
// process.
TEST_F(TestDiscoveryBonjour, DiscoveredBrokerCleanedUpAfterResolve)
{
  // Drive the state machine by calling the appropriate callbacks
  auto browse_cb = MonitorDefaultScope();
  auto resolve_cb = DriveBrowseCallback(browse_cb);
  auto gai_cb = DriveResolveCallback(resolve_cb);
  EXPECT_EQ(DNSServiceRefDeallocate_fake.call_count, 1u);
  DriveGetAddrInfoCallback(gai_cb);
  EXPECT_EQ(DNSServiceRefDeallocate_fake.call_count, 2u);

  // Make sure we are back to only one socket in the tick thread
  //  etcpal_poll_fake.return_val = 0;
  //  DNSServiceProcessResult_fake.return_val = kDNSServiceErr_NoError;
  //
  //  rdmnet_disc_tick();
  //  ASSERT_EQ(etcpal_poll_fake.call_count, 1u);
  //  ASSERT_EQ(etcpal_poll_fake.arg1_history[0], 1u);
  //  ASSERT_EQ(DNSServiceProcessResult_fake.call_count, 0u);
}
