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

// ServiceUtils.cpp
//
//  This file contains the general utilities for installing, running
//  and stoppoing services
//
//////////////////////////////////////////////////////////////////
#include <time.h>
#include "service_shell.h"
/*-----------------------------------------------------------------
  Function: GetLastErrorMessage

    Input:  - Pointer to the buffer where the descriptive message about
      the last error is returned.
      - Maximum number of characters that can be copied into
      the error message buffer (including the terminating
      NULL character)
-----------------------------------------------------------------*/
void GetLastErrorMessage(LPWSTR lpszerr, unsigned int nSize)
{
  LPWSTR lpMsgBuf;
  FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                 GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);

  if (wcslen(lpMsgBuf) >= nSize)
  {
    // truncate the message if it is longer than the supplied
    // size
    lpMsgBuf[nSize] = 0;
  }
  wcsncpy_s(lpszerr, nSize, lpMsgBuf, _TRUNCATE);
  LocalFree(lpMsgBuf);
}

/*-----------------------------------------------------------------
  Function: InstallService

    Input:  - The name of the service you wish to install

  Note: This name MUST be part of one of entries in the
  SERVICE_TABLE_ENTRY array that the main() uses to
  register the services with SCM. (I'm assuming that the user
  of this library is familiar with the Windows NT Services
  programming concepts).

-----------------------------------------------------------------*/
void InstallService(const LPWSTR lpName, const LPWSTR lpDescription, bool fAuto)
{
  SC_HANDLE newService, scm;
  WCHAR szPath[512];
  WCHAR szErr[512];

  if (GetModuleFileNameW(NULL, szPath, 512) == 0)
  {
    GetLastErrorMessage(szErr, 256);
    printf("Unable to install %S - %S\n", lpName, szErr);
    return;
  }

  // open a connection to the SCM
  scm = OpenSCManagerW(0, 0, SC_MANAGER_CREATE_SERVICE);
  if (!scm)
  {
    GetLastErrorMessage(szErr, 256);
    printf("Unable to Open Service Control manager for installing %S - %S\n", lpName, szErr);
    return;
  }
  // Install the new service
  newService =
      CreateServiceW(scm, lpName, lpName, SERVICE_ALL_ACCESS, SERVICE_WIN32_SHARE_PROCESS,
                     fAuto ? SERVICE_AUTO_START : SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, szPath, 0, 0, 0, 0, 0);
  if (!newService)
  {
    GetLastErrorMessage(szErr, 256);
    printf("Unable to Create Service %S - %S\n", lpName, szErr);
    return;
  }

  // Add the description
  SERVICE_DESCRIPTION description;
  description.lpDescription = const_cast<LPWSTR>(lpDescription);
  ChangeServiceConfig2W(newService, SERVICE_CONFIG_DESCRIPTION, &description);

  // clean up
  CloseServiceHandle(newService);
  CloseServiceHandle(scm);
}

/*-----------------------------------------------------------------
  Function: RemoveService

    Input:  - The name of the service you wish to remove

  Note: This name MUST be part of one of entries in the
  SERVICE_TABLE_ENTRY array that the main() uses to
  register the services with SCM. (I'm assuming that the user
  of this library is familiar with the Windows NT Services
  programming concepts).

-----------------------------------------------------------------*/
void RemoveService(const LPWSTR lpName)
{
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  SERVICE_STATUS ssStatus;  // current status of the service

  WCHAR szErr[512];

  schSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (schSCManager)
  {
    schService = OpenServiceW(schSCManager, lpName, SERVICE_ALL_ACCESS);

    if (schService)
    {
      // try to stop the service
      if (ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus))
      {
        printf("Stopping %S.", lpName);
        Sleep(1000);

        while (QueryServiceStatus(schService, &ssStatus))
        {
          if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
          {
            printf(".");
            Sleep(1000);
          }
          else
            break;
        }

        if (ssStatus.dwCurrentState == SERVICE_STOPPED)
          printf("\n%S stopped.\n", lpName);
        else
          printf("\n%S failed to stop.\n", lpName);
      }
      // now remove the service
      if (DeleteService(schService))
        printf("%S removed.\n", lpName);
      CloseServiceHandle(schService);
    }
    else
    {
      GetLastErrorMessage(szErr, 256);
      printf("OpenService failed - %S\n", szErr);
    }

    CloseServiceHandle(schSCManager);
  }
  else
  {
    GetLastErrorMessage(szErr, 256);
    printf("OpenSCManager failed - %S\n", szErr);
  }
}

/*-----------------------------------------------------------------
  Function: StopService

    Input:  - The name of the service you wish to stop

  Note: This name MUST be part of one of entries in the
  SERVICE_TABLE_ENTRY array that the main() uses to
  register the services with SCM. (I'm assuming that the user
  of this library is familiar with the Windows NT Services
  programming concepts).

-----------------------------------------------------------------*/
void StopService(const LPWSTR lpName)
{
  SC_HANDLE schService;
  SC_HANDLE schSCManager;
  SERVICE_STATUS ssStatus;  // current status of the service

  WCHAR szErr[512];

  schSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if (schSCManager)
  {
    schService = OpenServiceW(schSCManager, lpName, SERVICE_ALL_ACCESS);

    if (schService)
    {
      // try to stop the service
      if (ControlService(schService, SERVICE_CONTROL_STOP, &ssStatus))
      {
        printf("Stopping %S.", lpName);
        Sleep(1000);

        while (QueryServiceStatus(schService, &ssStatus))
        {
          if (ssStatus.dwCurrentState == SERVICE_STOP_PENDING)
          {
            printf(".");
            Sleep(1000);
          }
          else
            break;
        }

        if (ssStatus.dwCurrentState == SERVICE_STOPPED)
          printf("\n%S stopped.\n", lpName);
        else
          printf("\n%S failed to stop.\n", lpName);
      }
      CloseServiceHandle(schService);
    }
    else
    {
      GetLastErrorMessage(szErr, 256);
      printf("OpenService failed - %S\n", szErr);
    }

    CloseServiceHandle(schSCManager);
  }
  else
  {
    GetLastErrorMessage(szErr, 256);
    printf("OpenSCManager failed - %S\n", szErr);
  }
}

/*-----------------------------------------------------------------
  Function: RunService

    Input:  - The name of the service you wish to run

  Note: This name MUST be part of one of entries in the
  SERVICE_TABLE_ENTRY array that the main() uses to
  register the services with SCM. (I'm assuming that the user
  of this library is familiar with the Windows NT Services
  programming concepts).

-----------------------------------------------------------------*/
void RunService(const LPWSTR lpName, const LPWSTR lpDescription, DWORD argc, LPWSTR *argv, bool fAuto)
{
  SC_HANDLE service, scm;
  WCHAR szErr[512];

  // open SCM

  scm = OpenSCManagerW(0, 0, SC_MANAGER_ALL_ACCESS | GENERIC_WRITE);
  if (!scm)
  {
    GetLastErrorMessage(szErr, 256);
    printf("OpenSCManager failed - %S\n", szErr);
    return;
  }

  // get our service handle from the SCM
  service = OpenServiceW(scm, lpName, SERVICE_ALL_ACCESS);
  if (!service)
  {
    InstallService(lpName, lpDescription, fAuto);
    Sleep(1000);
    service = OpenServiceW(scm, lpName, SERVICE_ALL_ACCESS);
    if (!service)
    {
      GetLastErrorMessage(szErr, 256);
      printf("OpenService failed - %S\n", szErr);
      return;
    }
  }
  if (!StartServiceW(service, argc, (LPCWSTR *)argv))
  {
    GetLastErrorMessage(szErr, 256);
    printf("StartService failed - %S\n", szErr);
  }
  return;
}

/*------------------------------------------------------------------------
  Since there is no way to interactively debug a service,
  following function can be used during debugging to log the
  debug messages

----------------------------------------------------------------------*/
#ifdef _PRINT_DEBUG_LOG
void PrintDebugLog(const char *pszLogMsg)
{
  HANDLE hFile;
  unsigned long nWritten;
  hFile = CreateFile("c:\\ServiceTest.log",         // pointer to name of the file
                     GENERIC_READ | GENERIC_WRITE,  // access (read-write) mode
                     0,                             // share mode
                     NULL,                          // pointer to security attributes
                     OPEN_ALWAYS,                   // how to create
                     FILE_ATTRIBUTE_NORMAL,         // file attributes
                     NULL);                         // handle to file with attributes to copy
  if (hFile == INVALID_HANDLE_VALUE)
  {
    hFile = CreateFile("c:\\ServiceTest.log",         // pointer to name of the file
                       GENERIC_READ | GENERIC_WRITE,  // access (read-write) mode
                       0,                             // share mode
                       NULL,                          // pointer to security attributes
                       CREATE_ALWAYS,                 // how to create
                       FILE_ATTRIBUTE_NORMAL,         // file attributes
                       NULL);                         // handle to file with attributes to copy
  }
  if (hFile != INVALID_HANDLE_VALUE)
  {
    char szTime[32];

    SetFilePointer(hFile, 0, 0, FILE_END);
    _strtime(szTime);
    strcat(szTime, ": ");
    WriteFile(hFile, szTime, strlen(szTime), &nWritten, NULL);
    WriteFile(hFile, pszLogMsg, strlen(pszLogMsg), &nWritten, NULL);
    WriteFile(hFile, "\r\n", 2, &nWritten, NULL);
    CloseHandle(hFile);
  }
}
#endif
