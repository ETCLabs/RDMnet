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

#pragma once

#include "lwpa/lock.h"

// Macros to suppress warnings inside of Qt headers.
#if defined(_MSC_VER)

#define BEGIN_INCLUDE_QT_HEADERS() \
  __pragma(warning(push)) __pragma(warning(disable : 4127)) __pragma(warning(disable : 4251))

#define END_INCLUDE_QT_HEADERS() __pragma(warning(pop))

#else

#define BEGIN_INCLUDE_QT_HEADERS()
#define END_INCLUDE_QT_HEADERS()

#endif

// A representation of an optional static Broker configuration.
struct StaticBrokerConfig
{
  bool valid{false};
  LwpaSockaddr addr;
};

class ControllerReadGuard
{
public:
  explicit ControllerReadGuard(lwpa_rwlock_t &rwlock) : m_rwlock(rwlock)
  {
    if (!lwpa_rwlock_readlock(&m_rwlock, LWPA_WAIT_FOREVER))
      throw std::runtime_error("Controller failed to take a read lock.");
  }
  ~ControllerReadGuard() { lwpa_rwlock_readunlock(&m_rwlock); }

private:
  lwpa_rwlock_t &m_rwlock;
};

class ControllerWriteGuard
{
public:
  explicit ControllerWriteGuard(lwpa_rwlock_t &rwlock) : m_rwlock(rwlock)
  {
    if (!lwpa_rwlock_writelock(&m_rwlock, LWPA_WAIT_FOREVER))
      throw std::runtime_error("Controller failed to take a write lock.");
  }
  ~ControllerWriteGuard() { lwpa_rwlock_writeunlock(&m_rwlock); }

private:
  lwpa_rwlock_t &m_rwlock;
};