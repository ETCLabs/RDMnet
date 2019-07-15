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
#include <iostream>
#include "gtest/gtest.h"
#include "lwpa/netint.h"
#include "lwpa/socket.h"

// Need to pass this from the command line to a test case; there doesn't seem to be a better way to
// do this than using a global variable.
// LwpaIpAddr g_netint;

int main(int argc, char** argv)
{
  testing::InitGoogleTest(&argc, argv);

  // Only check our custom argument if we haven't been given the "list_tests" flag
  //  if (!testing::GTEST_FLAG(list_tests))
  //  {
  //    if (argc == 2)
  //    {
  //      if (0 >= lwpa_inet_pton(kLwpaIpTypeV4, argv[1], &g_netint))
  //      {
  //        std::cout << "Usage: " << argv[0] << " <interface_addr>" << std::endl;
  //        std::cout << "  interface_addr: IP address of network interface to use for test." << std::endl;
  //        return 1;
  //      }
  //    }
  //    else
  //    {
  //      LwpaNetintInfo default_netint;
  //      lwpa_netint_get_default_interface(&default_netint);
  //      g_netint = default_netint.addr;
  //    }
  //  }

  return RUN_ALL_TESTS();
}
