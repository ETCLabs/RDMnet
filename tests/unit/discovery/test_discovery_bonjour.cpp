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
#include "rdmnet/core/discovery.h"
#include "dns_sd.h"

#include "gtest/gtest.h"
#include "fff_cc.h"
DEFINE_FFF_GLOBALS;

// Mocking the dns_sd.h interface
FAKE_VALUE_FUNC(dnssd_sock_t, __stdcall, DNSServiceRefSockFD, DNSServiceRef);
FAKE_VALUE_FUNC(DNSServiceErrorType, __stdcall, DNSServiceProcessResult, DNSServiceRef);
FAKE_VOID_FUNC(__stdcall, DNSServiceRefDeallocate, DNSServiceRef);
FAKE_VALUE_FUNC(DNSServiceErrorType, __stdcall, DNSServiceRegister, DNSServiceRef *, DNSServiceFlags, uint32_t,
                const char *, const char *, const char *, const char *, uint16_t, uint16_t, const void *,
                DNSServiceRegisterReply, void *);
FAKE_VALUE_FUNC(DNSServiceErrorType, __stdcall, DNSServiceBrowse, DNSServiceRef *, DNSServiceFlags, uint32_t,
                const char *, const char *, DNSServiceBrowseReply, void *);
FAKE_VALUE_FUNC(DNSServiceErrorType, __stdcall, DNSServiceResolve, DNSServiceRef *, DNSServiceFlags, uint32_t,
                const char *, const char *, const char *, DNSServiceResolveReply, void *);
FAKE_VALUE_FUNC(DNSServiceErrorType, __stdcall, DNSServiceGetAddrInfo, DNSServiceRef *, DNSServiceFlags, uint32_t,
                DNSServiceProtocol, const char *, DNSServiceGetAddrInfoReply, void *);

class TestDiscoveryBonjour : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Reset fff state
    RESET_FAKE(DNSServiceRefSockFD);
    RESET_FAKE(DNSServiceProcessResult);
    RESET_FAKE(DNSServiceRefDeallocate);
    FFF_RESET_HISTORY();

    init_result_ = rdmnetdisc_init();
  }

  void TearDown() override { rdmnetdisc_deinit(); }

  lwpa_error_t init_result_;
};

TEST_F(TestDiscoveryBonjour, init)
{
  ASSERT_EQ(init_result_, LWPA_OK);
}
