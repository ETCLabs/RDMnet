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

///////////////////////////////////////////////////////////////////////
//
// ServiceShell.cpp: implementation of the CServiceShell class.
//
//
//
//  See the header file ServiceShell.h for notes
//
//
//
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "ServiceShell.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CServiceShell::CServiceShell()
{
  szName[0] = 0;
  lpServiceCtrlFn = NULL;
  hTerminateEvent = NULL;
  pauseService = FALSE;    // flag indicating if the serivce is paused
  runningService = FALSE;  // flag indicating if the serivce is running
  lpServiceThreadRoutine = NULL;
  // The worker thread
  hServiceThread = 0;
  exitServiceThread = false;
}

CServiceShell::~CServiceShell()
{
}

/*-------------------------------------------

  Function :  InitShell

  Must be called by the user of CServiceShell
  before registering ServiceMain

-------------------------------------------*/
bool CServiceShell::InitShell(LPWSTR lpServiceName, LPSCM_CALLBACK_FUNCTION lpfn,
                              LPTHREAD_START_ROUTINE lpServiceThread)
{
  if (wcslen(lpServiceName))
  {
    wcsncpy_s(szName, 512, lpServiceName, _TRUNCATE);
  }
  else
  {
    szName[0] = 0;
  }
  lpServiceCtrlFn = lpfn;
  lpServiceThreadRoutine = lpServiceThread;
  hTerminateEvent = NULL;
  pauseService = FALSE;    // flag indicating if the serivce is paused
  runningService = FALSE;  // flag indicating if the serivce is running
  // The worker thread
  hServiceThread = 0;
  return true;
}

/*---------------------------------------------------------------------

  Function :  ServiceMain

  This function is called at the startup of the service.
  It is called from the Callback function registered for the service
  associated with this instance of CServiceShell.

---------------------------------------------------------------------*/

void WINAPI CServiceShell::ServiceMain(DWORD /*argc*/, LPWSTR * /*argv*/)
{
  BOOL success;

  // first things first, call the Registration Function
  hServiceStatus = RegisterServiceCtrlHandler(szName, lpServiceCtrlFn);
  if (!hServiceStatus)
  {
    terminate(GetLastError());
    return;
  }

  success = SendStatusToSCM(SERVICE_START_PENDING, NO_ERROR, 0, 1, 5000);
  if (!success)
  {
    terminate(GetLastError());
    return;
  }
  hTerminateEvent = CreateEvent(0, TRUE, FALSE, 0);
  if (!hTerminateEvent)
  {
    terminate(GetLastError());
    return;
  }

  success = InitService();

  if (!success)
  {
    terminate(GetLastError());
    return;
  }
  // service is now running
  success = SendStatusToSCM(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
  if (!success)
  {
    terminate(GetLastError());
    return;
  }

  WaitForSingleObject(hTerminateEvent, INFINITE);
}

/*---------------------------------------------------------------------

  Function :  terminate

  This function is called when the service is shutting down. The
  user of CServiceShell MUST call this function when the service
  is shutting down. This function is otherwise called from within
  CServiceShell if any error conditions are encountered.

---------------------------------------------------------------------*/
void CServiceShell::terminate(DWORD dwErr)
{
  exitServiceThread = true;

  if (hServiceThread)
  {
    WaitForSingleObject(hServiceThread, 20000);
  }

  if (hTerminateEvent)
  {
    CloseHandle(hTerminateEvent);
    hTerminateEvent = NULL;
  }
  if (hServiceStatus)
  {
    SendStatusToSCM(SERVICE_STOPPED, dwErr, 0, 0, 0);
    hServiceStatus = 0;
  }
  if (hServiceThread)
  {
    CloseHandle(hServiceThread);
    hServiceThread = 0;
  }
  if (dwErr)
  {
    delete this;
  }
}

/*---------------------------------------------------------------------

  Function :  InitService

  Called from ServiceMain, this function will create a thread to
  execute the function that does the actual work for this service.

---------------------------------------------------------------------*/
BOOL CServiceShell::InitService()
{
  DWORD id;
  // Start the service's thread
  hServiceThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)lpServiceThreadRoutine, 0, 0, &id);
  Sleep(500);
  if (WaitForSingleObject(hServiceThread, 2000) != WAIT_TIMEOUT)
  {
#ifdef _PRINT_DEBUG_LOG
    PrintDebugLog("Service Thread Terminated\r\n");
#endif
    return FALSE;
  }
  if (hServiceThread == 0)
  {
    return FALSE;
  }
  else
  {
    runningService = TRUE;
  }
  return TRUE;
}

/*---------------------------------------------------------------------

  Function :  ServiceCtrlHandler

  Called from the SCM Callback funtion when the SCM wants to communicate
  with the service.


---------------------------------------------------------------------*/
void WINAPI CServiceShell::ServiceCtrlHandler(DWORD controlCode)
{
  DWORD currentState = 0;
  // BOOL success;
  switch (controlCode)
  {
    // There is no START option because
    // ServiceMain gets called on a start

    // Stop the service
    case SERVICE_CONTROL_STOP:
      currentState = SERVICE_STOP_PENDING;
      // Tell the SCM what's happening
      /*success =*/SendStatusToSCM(SERVICE_STOP_PENDING, NO_ERROR, 0, 1, 5000);
      // Not much to do if not successful

      // Stop the service
      StopService();
      return;

    // Pause the service
    case SERVICE_CONTROL_PAUSE:

      if (runningService && !pauseService)
      {
        // Tell the SCM what's happening
        /*success = */ SendStatusToSCM(SERVICE_PAUSE_PENDING, NO_ERROR, 0, 1, 1000);
        PauseService();
        currentState = SERVICE_PAUSED;
      }
      break;

    // Resume from a pause
    case SERVICE_CONTROL_CONTINUE:

      if (runningService && pauseService)
      {
        // Tell the SCM what's happening
        /*success = */ SendStatusToSCM(SERVICE_CONTINUE_PENDING, NO_ERROR, 0, 1, 1000);
        ResumeService();
        currentState = SERVICE_RUNNING;
      }
      break;

    // Update current status
    case SERVICE_CONTROL_INTERROGATE:
      // it will fall to bottom and send status
      if (hTerminateEvent == NULL && runningService == FALSE)
      {
        // Tell the SCM what's happening
        SendStatusToSCM(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
      }

      break;

    // Do nothing in a shutdown. Could do cleanup
    // here but it must be very quick.
    case SERVICE_CONTROL_SHUTDOWN:
      StopService();
      WaitForSingleObject(hServiceThread, 20000);
      return;
    default:
      break;
  }

  SendStatusToSCM(currentState, NO_ERROR, 0, 0, 0);
}

/*---------------------------------------------------------------------

  Function :  SendStatusToSCM

  The service uses this function to report its current status
  to the SCM.

---------------------------------------------------------------------*/
BOOL CServiceShell::SendStatusToSCM(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode,
                                    DWORD dwCheckPoint, DWORD dwWaitHint)
{
  BOOL success;
  SERVICE_STATUS serviceStatus;
  // Fill in all of the SERVICE_STATUS fields
  serviceStatus.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
  serviceStatus.dwCurrentState = dwCurrentState;

  // If in the process of something, then accept
  // no control events, else accept anything
  if (dwCurrentState == SERVICE_START_PENDING)
    serviceStatus.dwControlsAccepted = 0;
  else
    serviceStatus.dwControlsAccepted =
        SERVICE_ACCEPT_STOP |
        //      SERVICE_ACCEPT_PAUSE_CONTINUE |  //NJW No longer supporting this, as it confuses some apps
        SERVICE_ACCEPT_SHUTDOWN;

  // if a specific exit code is defines, set up
  // the win32 exit code properly
  if (dwServiceSpecificExitCode == 0)
    serviceStatus.dwWin32ExitCode = dwWin32ExitCode;
  else
    serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
  serviceStatus.dwServiceSpecificExitCode = dwServiceSpecificExitCode;

  serviceStatus.dwCheckPoint = dwCheckPoint;
  serviceStatus.dwWaitHint = dwWaitHint;

  // Pass the status record to the SCM
  success = SetServiceStatus(hServiceStatus, &serviceStatus);
  if (!success)
  {
    StopService();
  }
  return success;
}
/*----------------------------------------------------------------------
Following three functions pause, resome and stop the service.

----------------------------------------------------------------------*/
void CServiceShell::PauseService()
{
  pauseService = TRUE;
  SuspendThread(hServiceThread);
}

void CServiceShell::StopService()
{
  runningService = FALSE;
  terminate(0);
}

void CServiceShell::ResumeService()
{
  pauseService = FALSE;
  ResumeThread(hServiceThread);
}
