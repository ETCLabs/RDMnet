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

// ServiceShell.h: interface for the CServiceShell class.
//
//
//  Assumption: It is assumed that the user of this class
//  has the basic knowledge of Windows NT service operation.
//
//  This class encapsulates the functionality needed to implement
//  a Windows NT Services.
//  The class allows the user to add multiple services in one
//  executable.
//
//
//////////////////////////////////////////////////////////////////////

#ifndef SERVICESHELL_H
#define SERVICESHELL_H

#include <windows.h>
#include <stdio.h>
#include <winsvc.h>

//--- following are the declarations for the functions that
// allow the user to control the service (install, run, remove)
// from the command line
void RunService(const LPWSTR lpName, const LPWSTR lpDescription, DWORD argc, LPWSTR *argv, bool fAuto);
void RemoveService(const LPWSTR lpName);
void InstallService(const LPWSTR lpName, const LPWSTR lpDescription, bool fAuto);
void StopService(const LPWSTR lpName);
void GetLastErrorMessage(LPWSTR lpszerr, unsigned int nSize);

#ifdef _PRINT_DEBUG_LOG
void PrintDebugLog(char *pszLogMsg);
#endif

typedef void(__stdcall *LPSCM_CALLBACK_FUNCTION)(unsigned long);
/*-----------------------------------------------------------------
  The class CServiceShell encapsulates the functionality
  required by Windows NT from a service.

    The InitShell call is the most important call of this class.
  For every instance of this class, the user needs to call
  InitShell()

  The parameters for InitShell are :

  lpServiceName - Null terminated name of the service. The length
  of the service name can be maximum 512 characters including the
  termination character

  lpSCMCallbackfn - Pointer to a function that the Service Control
  Manager (SCM) can call for communicating with the service. The
  work of communicating with the SCM is done by the function
  ServiceCtrlHandler() in this class. So in reality the actual
  function body of this function in your code is one line.
  Please refer to the sample code snippet at the end of this
  comment that shows the sample callback function that should
  be implemented by the user of this library.

  lpServiceThread - The actual thread function that will perform
  all the work for your service.


  The user also must implement a function that will call the
  ServiceMain member of this class when ever Windows needs to
  communicate with ServiceMain. All the user has to do is to
  initialize the static instance of the CServiceShell class
  associated with this service and then call the ServiceMain
  member of this class.

  Summary:
  In order to use this class the user needs to:
  1. Add the proper entries to the SERVICE_TABLE_ENTRY
   This includes providing a unique name for the service,
   implementing a callback function as follows
        void CallbackServiceMain(DWORD argc, LPTSTR *argv)
  and referring to the function in the SERVICE_TABLE_ENTRY

   2. Create a static global instance of the class in the
     function corrospnding to the LPSERVICE_MAIN_FUNCTION
   in the service tabel entry (In the body of the
   function CallbackServiceMain). If class is already
   instantiated, call ServiceMain function of the instance.

  3. Call Initshell member of the class with the service name,
   SCM callback function and the thread function of the service.



  ****** BEGIN SAMPLE *********
#include <windows.h>
#include <stdio.h>
#include "ServiceShell.h"


  char *ServiceName= "MyService";

  static CServiceShell *lpsrvShell = NULL; // static global instance of the
                      // class associated with the service

  DWORD WINAPI ServiceThread(LPVOID param)
  {
      // allocate any resources needed in your thread here
    while(!lpsrvShell->exitServiceThread)
    {
      // do my service work
    }
      // deallocate any resources allocated in your thread here
    return 0;
  }


  void WINAPI  SCMCallback(DWORD controlCode)
  {
  if(lpsrvShell)
  {
    lpsrvShell->ServiceCtrlHandler(controlCode);
  }
  }

  void CallbackServiceMain(DWORD argc, LPTSTR *argv)
  {

  if(!lpsrvShell)
  {
    lpsrvShell = new CServiceShell();
    lpsrvShell->InitShell(ServiceName, SCMCallback, ServiceThread);
  }
  lpsrvShell->ServiceMain(argc, argv);

    // When the control reaches here, Windows is trying to shut down the service.
  // do the cleanup for your service here and terminate it
  lpsrvShell->terminate(0);
  delete lpsrvShell;
  }

  int main(int argc, char **argv)
  {

  SERVICE_TABLE_ENTRY serviceTable[] =
  {
    {
      ServiceName, (LPSERVICE_MAIN_FUNCTION) CallbackServiceMain
    },
    { NULL, NULL}
  };
  // handle any command line parsing for handling the install, remove
  // or run functions for the service(s)
  // findout if we want to run as a service or not
  if(argc > 1)
  {
    if ( __stricmp(argv[1], "-install") == 0)
    {
      // Install as a service
      InstallService(ServiceName, false);
      exit(0);
    }
    if ( __stricmp(argv[1], "-run") == 0)
    {
      // run service
      RunService(ServiceName, argc-2, &(argv[2]), false);
      exit(0);
    }
    if(__stricmp(argv[1], "-remove")  == 0)
    {
      RemoveService(ServiceName);
      exit(0);
    }
  }


  // call StartServiceCtrlDispatcher
  if(!StartServiceCtrlDispatcher(serviceTable))
  {
    printf("Error in calling StartServiceCtrlDispatcher\r\n");
  }
  return 0;
  }

  ****** END SAMPLE *********

-----------------------------------------------------------------*/
class CServiceShell
{
public:
  void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
  bool InitShell(LPTSTR lpServiceName, LPSCM_CALLBACK_FUNCTION lpSCMCallbackfn, LPTHREAD_START_ROUTINE lpServiceThread);
  CServiceShell();
  virtual ~CServiceShell();
  void WINAPI ServiceCtrlHandler(DWORD controlCode);
  void terminate(DWORD dwErr);
  // data
  LPTHREAD_START_ROUTINE lpServiceThreadRoutine;
  bool exitServiceThread;

private:
  void PauseService();
  void ResumeService();
  void StopService();
  BOOL InitService();

  LPSCM_CALLBACK_FUNCTION lpServiceCtrlFn;

  BOOL SendStatusToSCM(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwServiceSpecificExitCode, DWORD dwCheckPoint,
                       DWORD dwWaitHint);

  SERVICE_STATUS_HANDLE hServiceStatus;
  HANDLE hTerminateEvent;
  TCHAR szName[512];
  // Flags
  BOOL pauseService;    // flag indicating if the service is paused
  BOOL runningService;  // flag indicating if the serivce is running

  // The worker thread
  HANDLE hServiceThread;
};

#endif  // #define SERVICESHELL_H
