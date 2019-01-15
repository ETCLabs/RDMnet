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

// brokermain.cpp : Defines the entry point for the console application,
// and drives the generic RDMnet broker logic.

#include <WinSock2.h>  //MUST ALWAYS BE INCLUDED before windows.h!
#include <IPHlpApi.h>  //For the network change detection
#include <windows.h>
#include <WS2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <string>
#include <vector>
#include "lwpa/log.h"
#include "lwpa/pack.h"
#include "lwpa/socket.h"
#include "rdmnet/version.h"
#include "rdmnet/broker.h"
#include "service_shell.h"
#include "iflist.h"
#include "broker_log.h"

static bool debug = false;

const LPWSTR SERVICE_NAME = L"ETC RDMnet Broker";
const LPWSTR BROKER_SERVICE_DESCRIPTION = L"Provides basic RDMnet Broker functionality";

// static global instance of the class associated with the service
static CServiceShell *g_shell = NULL;

bool g_set_new_scope = false;
std::string g_scope_to_set;

#define MYKEY HKEY_CURRENT_USER
#define MYSUBKEY L"SOFTWARE\\ETC\\RDMnetBroker"
#define MYSCOPE L"scope"
#define MYIFACES L"localips"
#define MYMACS L"localmacs"
#define MYPORT L"port"

// Turn defined string literals into wide strings
#define __MAKE_L(strlit) L##strlit
#define MAKE_L(strlit) __MAKE_L(strlit)

// If no scope exists, default is returned
void GetScopeKey(std::string &scope)
{
  scope = E133_DEFAULT_SCOPE;
  HKEY key;
  if (ERROR_SUCCESS == RegOpenKeyExW(MYKEY, MYSUBKEY, 0, KEY_READ, &key))
  {
    WCHAR val[128];
    DWORD valsize = sizeof(val);
    if (ERROR_SUCCESS == RegQueryValueExW(key, MYSCOPE, NULL, NULL, (BYTE *)&val, &valsize))
    {
      if (wcslen(val) != 0)
      {
        char val_utf8[256];
        WideCharToMultiByte(CP_UTF8, 0, val, -1, val_utf8, 256, NULL, NULL);
        scope = val_utf8;
      }
    }

    RegCloseKey(key);
  }
}

// Given a pointer to a string, parses out a mac addr
void ParseMac(WCHAR *s, uint8_t *pmac)
{
  WCHAR *p = s;

  for (int index = 0; index < IFList::kMACLen; ++index)
  {
    pmac[index] = (uint8_t)wcstol(p, &p, 16);
    ++p;  // P points at the :
  }
}

// Fills in the vector of addrs based on the registry key.
// If the key is empty, all interfaces are used.
void GetMyIfaceKey(std::vector<LwpaIpAddr> &addrs, const std::vector<IFList::iflist_entry> &interfaces)
{
  addrs.clear();

  HKEY key;
  if (ERROR_SUCCESS == RegOpenKeyExW(MYKEY, MYSUBKEY, 0, KEY_READ, &key))
  {
    WCHAR val[MAX_PATH];
    DWORD valsize = sizeof(val);
    // We prefer mac addresses to ip addrs
    if (ERROR_SUCCESS == RegQueryValueExW(key, MYMACS, NULL, NULL, (BYTE *)&val, &valsize))
    {
      if (wcslen(val) != 0)
      {
        WCHAR *context;
        for (WCHAR *p = wcstok_s(val, L",", &context); p != NULL; p = wcstok_s(NULL, L",", &context))
        {
          uint8_t tstmac[IFList::kMACLen];
          ParseMac(p, tstmac);
          for (auto iface = interfaces.cbegin(); iface != interfaces.cend(); ++iface)
          {
            if (0 == memcmp(tstmac, iface->mac, IFList::kMACLen))
            {
              addrs.push_back(iface->addr);
              break;
            }
          }
        }
      }
    }
    else if (ERROR_SUCCESS == RegQueryValueExW(key, MYIFACES, NULL, NULL, (BYTE *)&val, &valsize))
    {
      if (wcslen(val) != 0)
      {
        WCHAR *context;
        for (WCHAR *p = wcstok_s(val, L",", &context); p != NULL; p = wcstok_s(NULL, L",", &context))
        {
          struct in_addr tst_addr;
          struct in6_addr tst_addr6;
          LwpaIpAddr addr;

          INT res = InetPtonW(AF_INET, p, &tst_addr);
          if (res == 1)
            ip_plat_to_lwpa_v4(&addr, &tst_addr);
          else
          {
            res = InetPtonW(AF_INET6, p, &tst_addr6);
            if (res == 1)
              ip_plat_to_lwpa_v6(&addr, &tst_addr6);
          }
          if (res == 1)
          {
            for (auto iface = interfaces.cbegin(); iface != interfaces.cend(); ++iface)
            {
              if (lwpaip_equal(&addr, &iface->addr))
              {
                addrs.push_back(iface->addr);
                break;
              }
            }
          }
        }
      }
    }
    RegCloseKey(key);
  }

  if (addrs.empty())
  {
    for (auto iface = interfaces.cbegin(); iface != interfaces.cend(); ++iface)
      addrs.push_back(iface->addr);
  }
}

void GetPortKey(uint16_t &port)
{
  HKEY key;

  port = 8888;

  if (ERROR_SUCCESS == RegOpenKeyExW(MYKEY, MYSUBKEY, 0, KEY_READ, &key))
  {
    WCHAR val[165];
    DWORD valsize = sizeof(val);
    if (ERROR_SUCCESS == RegQueryValueExW(key, MYPORT, NULL, NULL, (BYTE *)val, &valsize))
    {
      if (wcslen(val) != 0)
      {
        port = static_cast<uint16_t>(_wtoi(val));
      }
    }

    RegCloseKey(key);
  }
}

uint16_t GetPortKey()
{
  uint16_t port;
  GetPortKey(port);
  return port;
}

void SetScopeKey(const LPWSTR buffer)
{
  HKEY key;
  DWORD disp;

  LONG create_result =
      RegCreateKeyExW(MYKEY, MYSUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp);

  if (create_result == ERROR_SUCCESS)
  {
    if (wcslen(buffer) == 0)
    {
      RegSetValueExW(key, MYSCOPE, 0, REG_SZ, (BYTE *)MAKE_L(E133_DEFAULT_SCOPE),
                     (DWORD)((wcslen(MAKE_L(E133_DEFAULT_SCOPE)) + 1) * sizeof(WCHAR)));
    }
    else
    {
      RegSetValueExW(key, MYSCOPE, 0, REG_SZ, (BYTE *)buffer, (DWORD)((wcslen(buffer) + 1) * sizeof(WCHAR)));
    }

    RegCloseKey(key);
  }
}

void SetMyIfaceKey(const LPWSTR buffer)
{
  // We only allow setting the mac address key
  HKEY key;
  DWORD disp;
  if (ERROR_SUCCESS ==
      RegCreateKeyExW(MYKEY, MYSUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp))
  {
    RegSetValueExW(key, MYMACS, 0, REG_SZ, (BYTE *)buffer,
                   (DWORD)(min(((wcslen(buffer) + 1) * sizeof(WCHAR)), (MAX_PATH - 1))));
    // Also try to delete the localips
    RegDeleteValueW(key, MYIFACES);
    RegCloseKey(key);
  }
}

void SetPortKey(const LPWSTR buffer)
{
  HKEY key;
  DWORD disp;

  LPWSTR default_port_str = L"8888";

  if (ERROR_SUCCESS ==
      RegCreateKeyExW(MYKEY, MYSUBKEY, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &key, &disp))
  {
    if (wcslen(buffer) == 0)
    {
      RegSetValueExW(key, MYPORT, 0, REG_SZ, (BYTE *)default_port_str,
                     (DWORD)((wcslen(default_port_str) + 1) * sizeof(WCHAR)));
    }
    else
    {
      RegSetValueExW(key, MYPORT, 0, REG_SZ, (BYTE *)buffer, (DWORD)((wcslen(buffer) + 1) * sizeof(WCHAR)));
    }

    RegCloseKey(key);
  }
}

void SetPortKey(uint16_t port)
{
  WCHAR port_buffer[6];

  wmemset(port_buffer, 0, 6);
  _itow_s(port, port_buffer, 6, 10);

  SetPortKey(port_buffer);
}

class BrokerNotify : public RDMnet::BrokerNotify
{
public:
  // The scope has changed due to RDMnet messaging.
  // Save it for the next time you are starting the broker.
  void ScopeChanged(const std::string &new_scope)
  {
    g_set_new_scope = true;
    g_scope_to_set = new_scope;

    int w_new_scope_size = static_cast<int>((new_scope.size() * sizeof(WCHAR)) + 1);
    WCHAR *w_new_scope = new WCHAR[w_new_scope_size];
    if (w_new_scope)
    {
      if (0 != MultiByteToWideChar(CP_UTF8, 0, new_scope.c_str(), -1, w_new_scope, w_new_scope_size))
      {
        SetScopeKey(w_new_scope);
      }
      delete[] w_new_scope;
    }
  }
};

bool ShouldApplyChanges(HANDLE net_handle, LPOVERLAPPED net_overlap, bool &bOverlapped)
{
  DWORD temp;
  bOverlapped = (GetOverlappedResult(net_handle, net_overlap, &temp, false) ? true : false);
  return (bOverlapped || g_set_new_scope);
}

void PrepForSettingsChange(RDMnet::Broker &broker, RDMnet::BrokerSettings &settings)
{
  broker.Shutdown();
  broker.GetSettings(settings);
}

void ApplySettingsChanges(RDMnet::BrokerLog &log, bool bOverlapped, RDMnet::BrokerSettings &settings, HANDLE net_handle,
                          LPOVERLAPPED net_overlap, std::vector<IFList::iflist_entry> &interfaces,
                          std::vector<LwpaIpAddr> &useaddrs)
{
  // If we detect the network changed, restart the broker core
  if (bOverlapped)
  {
    log.Log(LWPA_LOG_INFO, "Network change detected, restarting broker and applying changes");

    // We need to reset the useaddrs vector
    IFList::FindIFaces(log, interfaces);
    GetMyIfaceKey(useaddrs, interfaces);
    memset(net_overlap, 0, sizeof(OVERLAPPED));
    net_handle = NULL;
    NotifyAddrChange(&net_handle, net_overlap);
  }

  // If there are other new settings, apply them here.
  if (g_set_new_scope)
  {
    g_set_new_scope = false;
    log.Log(LWPA_LOG_INFO, "Scope change detected, restarting broker and applying changes");
    settings.disc_attributes.scope = g_scope_to_set;
  }
}

DWORD WINAPI ServiceThread(LPVOID /*param*/)
{
  // allocate any resources needed in your thread here
  WindowsBrokerLog broker_log(debug, "RDMnetBroker.log");
  broker_log.StartThread();

  BrokerNotify broker_notify;
  RDMnet::BrokerSettings broker_settings(0x6574);
  GetScopeKey(broker_settings.disc_attributes.scope);
  std::vector<IFList::iflist_entry> interfaces;
  IFList::FindIFaces(broker_log, interfaces);

  // Given the first network interface found, generate the cid and UID
  if (!interfaces.empty())
  {
    // The cid will be based on the scope, in case we want to run different instances on the same
    // machine
    std::string cidstr("ETC E133 BROKER for scope: ");
    cidstr += broker_settings.disc_attributes.scope;
    lwpa_generate_v3_uuid(&broker_settings.cid, cidstr.c_str(), interfaces.front().mac, 1);
  }

  broker_settings.disc_attributes.dns_manufacturer = "ETC";
  broker_settings.disc_attributes.dns_service_instance_name = "UNIQUE NAME";
  broker_settings.disc_attributes.dns_model = "E1.33 Broker Prototype";

  std::vector<LwpaIpAddr> useaddrs;
  GetMyIfaceKey(useaddrs, interfaces);

  RDMnet::Broker broker(&broker_log, &broker_notify);
  broker.Startup(broker_settings, GetPortKey(), useaddrs);

  // We want to detect network change as well
  OVERLAPPED net_overlap;
  memset(&net_overlap, 0, sizeof(OVERLAPPED));
  HANDLE net_handle = NULL;
  NotifyAddrChange(&net_handle, &net_overlap);

  // We want this to run forever if a console, otherwise run for how long the service manager allows
  // it
  while (!g_shell || !g_shell->exitServiceThread)
  {
    // Do the main service work here
    bool bOverlapped = false;

    broker.Tick();

    if (ShouldApplyChanges(net_handle, &net_overlap, bOverlapped))
    {
      PrepForSettingsChange(broker, broker_settings);
      ApplySettingsChanges(broker_log, bOverlapped, broker_settings, net_handle, &net_overlap, interfaces, useaddrs);
      broker.Startup(broker_settings, GetPortKey(), useaddrs);
    }

    Sleep(300);
  }

  // deallocate any resources allocated in your thread here
  broker.Shutdown();
  /* TESTING TODO REMOVE
  if(iface_lib)
  {
      iface_lib->Shutdown();
      IWinAsyncSocketServ::DestroyInstance(iface_lib);
  }
  */

  return 0;
}

void WINAPI SCMCallback(DWORD controlCode)
{
  if (g_shell)
  {
    g_shell->ServiceCtrlHandler(controlCode);
  }
}

void CallbackServiceMain(DWORD argc, LPWSTR *argv)
{
  if (!g_shell)
  {
    g_shell = new CServiceShell();
    g_shell->InitShell(SERVICE_NAME, SCMCallback, ServiceThread);
  }
  g_shell->ServiceMain(argc, argv);

  // When the control reaches here, Windows is trying to shut down the service.
  // do the cleanup for your service here and terminate it
  g_shell->terminate(0);
  delete g_shell;
}

void PrintVersion()
{
  printf("ETC Prototype RDMnet Broker\n");
  printf("Version %s\n\n", RDMNET_VERSION_STRING);
  printf("Copyright (c) 2018 ETC Inc.\n");
  printf("License: Apache License v2.0 <http://www.apache.org/licenses/LICENSE-2.0>\n");
  printf("Unless required by applicable law or agreed to in writing, this software is\n");
  printf("provided \"AS IS\", WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express\n");
  printf("or implied.\n");
}

void PrintHelp(wchar_t *app_name)
{
  printf("Usage: %ls [OPTION]... COMMAND\n\n", app_name);
  printf("Commands:\n");
  printf("  install  Install the service\n");
  printf("  run      Start the service, installing it if necessary\n");
  printf("  remove   Remove the service\n");
  printf("  debug    Run the service as a console application\n\n");
  printf("Options:\n");
  printf("  --auto               Installs/runs as automatic, otherwise manual\n");
  printf("  --scope=SCOPE        Configures the RDMnet Scope to SCOPE and saves it to the\n");
  printf("                       registry. Enter nothing after '=' to set the scope to\n");
  printf("                       the default.\n");
  printf("  --ifaces=IFACE_LIST  A comma-separated list of local network interface mac\n");
  printf("                       addresses to use, e.g. 00:c0:16:11:da:b3. These get\n");
  printf("                       saved to the registry. Enter nothing after '=' to clear\n");
  printf("                       the ifaces key and use all interfaces available.\n");
  printf("  --port=PORT          The port that this broker instance should use (default\n");
  printf("                       8888). This gets saved to the registry for future use.\n");
  printf("                       Enter nothing after '=' to set the port to the default.\n");
  printf("  --help               Display this help and exit.\n");
  printf("  --version            Output version information and exit.\n");
}

int wmain(int argc, wchar_t *argv[])
{
  SERVICE_TABLE_ENTRY serviceTable[] = {
      {const_cast<LPWSTR>(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)CallbackServiceMain}, {NULL, NULL}};

  bool should_exit = true;
  // Because we are doing automatic and non-automatic installs, we need wait until the parse is
  // complete
  bool fAuto = false;
  bool should_install = false;
  bool should_run = false;

  // handle any command line parsing for handling the install, remove or run functions for the
  // service(s)
  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      if (_wcsicmp(argv[i], L"--auto") == 0)
        fAuto = true;
      else if (_wcsnicmp(argv[i], L"--scope=", 8) == 0)
      {
        SetScopeKey(argv[i] + 8);
        should_exit = false;
      }
      else if (_wcsnicmp(argv[i], L"--ifaces=", 9) == 0)
      {
        SetMyIfaceKey(argv[i] + 9);
        should_exit = false;
      }
      else if (_wcsicmp(argv[i], L"install") == 0)
      {
        should_install = true;
      }
      else if (_wcsicmp(argv[i], L"run") == 0)
      {
        should_run = true;
        should_exit = true;
      }
      else if (_wcsicmp(argv[i], L"remove") == 0)
      {
        RemoveService(SERVICE_NAME);
        should_exit = true;
      }
      else if (_wcsicmp(argv[i], L"debug") == 0)
      {
        should_exit = false;
        debug = true;
      }
      else if (_wcsnicmp(argv[i], L"--port=", 7) == 0)
      {
        SetPortKey(argv[i] + 7);
        should_exit = false;
      }
      else if (_wcsicmp(argv[i], L"--version") == 0)
      {
        PrintVersion();
        break;
      }
      else
      {
        PrintHelp(argv[0]);
        break;
      }
    }
  }
  else
    PrintHelp(argv[0]);

  if (should_install)
    InstallService(SERVICE_NAME, BROKER_SERVICE_DESCRIPTION, fAuto);

  if (should_run)
    RunService(SERVICE_NAME, BROKER_SERVICE_DESCRIPTION, argc, argv, fAuto);

  if (debug)
  {
    ServiceThread(NULL);
  }
  // call StartServiceCtrlDispatcher if we don't need to exit
  if (!should_exit && !StartServiceCtrlDispatcherW(serviceTable))
  {
    printf("Error in calling StartServiceCtrlDispatcher\r\n");
  }
  return 0;
}
