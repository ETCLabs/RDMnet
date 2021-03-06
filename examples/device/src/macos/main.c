/******************************************************************************
 * Copyright 2020 ETC Inc.
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

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "etcpal/inet.h"
#include "example_device.h"
#include "macos_device_log.h"

static void print_help(const char* app_name)
{
  printf("Usage: %s [OPTION]...\n\n", app_name);
  printf("  --scope=SCOPE     Configures the RDMnet Scope to SCOPE. Enter nothing after\n");
  printf("                    '=' to set the scope to the default.\n");
  printf("  --broker=IP:PORT  Connect to a Broker at address IP:PORT instead of\n");
  printf("                    performing discovery.\n");
  printf("  --help            Display this help and exit.\n");
  printf("  --version         Output version information and exit.\n");
}

// Parse the --scope=SCOPE command line option and transfer it to the settings buffer.
static bool set_scope(const char* scope_str, char* scope_buf)
{
  if (strlen(scope_str) != 0)
  {
    strncpy(scope_buf, scope_str, E133_SCOPE_STRING_PADDED_LENGTH);
    scope_buf[E133_SCOPE_STRING_PADDED_LENGTH] = '\0';
    return true;
  }
  return false;
}

// Parse the --broker=IP:PORT command line option and transfer it to the sockaddr structure.
static bool set_static_broker(char* arg, EtcPalSockAddr* static_broker_addr)
{
  char* sep = strchr(arg, ':');
  if (sep != NULL && sep - arg < ETCPAL_IP_STRING_BYTES)
  {
    char      ip_str[ETCPAL_IP_STRING_BYTES];
    ptrdiff_t ip_str_len = sep - arg;

    memcpy(ip_str, arg, ip_str_len);
    ip_str[ip_str_len] = '\0';

    if ((etcpal_string_to_ip(kEtcPalIpTypeV4, ip_str, &static_broker_addr->ip) == kEtcPalErrOk) ||
        (etcpal_string_to_ip(kEtcPalIpTypeV6, ip_str, &static_broker_addr->ip) == kEtcPalErrOk))
    {
      if (1 == sscanf(sep + 1, "%hu", &static_broker_addr->port))
        return true;
    }
  }
  return false;
}

static bool device_keep_running = true;

void signal_handler(int signal)
{
  printf("Stopping Device...\n");
  device_keep_running = false;
}

int main(int argc, char* argv[])
{
  etcpal_error_t         res = kEtcPalErrOk;
  bool                   should_exit = false;
  char                   initial_scope[E133_SCOPE_STRING_PADDED_LENGTH];
  EtcPalSockAddr         initial_static_broker = {0, ETCPAL_IP_INVALID_INIT};
  const EtcPalLogParams* lparams;

  strcpy(initial_scope, E133_DEFAULT_SCOPE);

  if (argc > 1)
  {
    for (int i = 1; i < argc; ++i)
    {
      if (strncmp(argv[i], "--scope=", 8) == 0)
      {
        if (!set_scope(&argv[i][8], initial_scope))
        {
          print_help(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (strncmp(argv[i], "--broker=", 9) == 0)
      {
        if (!set_static_broker(&argv[i][9], &initial_static_broker))
        {
          print_help(argv[0]);
          should_exit = true;
          break;
        }
      }
      else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0)
      {
        device_print_version();
        should_exit = true;
        break;
      }
      else
      {
        print_help(argv[0]);
        should_exit = true;
        break;
      }
    }
  }
  if (should_exit)
    return 1;

  device_log_init();
  lparams = device_get_log_params();

  // Handle Ctrl+C and gracefully shutdown.
  struct sigaction sigint_handler;
  sigint_handler.sa_handler = signal_handler;
  sigemptyset(&sigint_handler.sa_mask);
  sigint_handler.sa_flags = 0;
  sigaction(SIGINT, &sigint_handler, NULL);

  /* Startup the device */
  res = device_init(lparams, initial_scope, &initial_static_broker);
  if (res != kEtcPalErrOk)
  {
    etcpal_log(lparams, ETCPAL_LOG_ERR, "Device failed to initialize: '%s'", etcpal_strerror(res));
    return 1;
  }

  etcpal_log(lparams, ETCPAL_LOG_INFO, "Device initialized.");

  while (device_keep_running)
  {
    usleep(100000);
  }

  device_deinit();
  device_log_deinit();
  return 0;
}
