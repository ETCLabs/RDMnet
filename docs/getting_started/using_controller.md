# Using the Controller API                                                      {#using_controller}

The RDMnet Controller API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The controller init and deinit functions should be called once each at application startup and
shutdown time. These functions interface with the EtcPal \ref etcpal_log API to configure what
happens when the library logs messages. Optionally pass an EtcPalLogParams structure to use this 
functionality. This structure can be shared across different ETC library modules.

The init function has the side-effect of initializing the core RDMnet library, which by default
starts a background thread for handling periodic RDMnet functionality and receiving data (this
behavior can be overridden at compile-time if an app wants more control over its threading, see
#RDMNET_USE_TICK_THREAD and rdmnet_core_tick()). The deinit function joins this thread.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/controller.h"

// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...

etcpal_error_t init_result = rdmnet_controller_init(&log_params);
// Or, to init without worrying about logs from the RDMnet library...
etcpal_error_t init_result = rdmnet_controller_init(NULL);

// In some function called at shutdown...
rdmnet_controller_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "rdmnet/cpp/controller.h"

// In some function called at startup...
etcpal::Logger logger;
// Initialize and start logger...

etcpal::Error init_result = rdmnet::Controller::Init(logger);

// Or, to init without worrying about logs from the RDMnet library...
etcpal::Error init_result = rdmnet::Controller::Init();

// In some function called at shutdown...
rdmnet::Controller::Deinit();
```
<!-- CODE_BLOCK_END -->

To create a controller instance, use the rdmnet_controller_create() function in C, or instantiate
an rdmnet::Controller and call its Startup() function in C++. Most apps will only need a single
controller instance. One controller can monitor an arbitrary number of RDMnet scopes at once.

The RDMnet controller API is an asynchronous, callback-oriented API. Part of the initial
configuration for a controller instance is a set of function pointers (or abstract interface) for
the library to use as callbacks. Callbacks are dispatched from the thread context which calls
rdmnet_core_tick().

<!-- CODE_BLOCK_START -->
```c
RdmnetControllerConfig config;

// Sets optional values to defaults. Must pass your ESTA manufacturer ID. If you have not yet
// requested an ESTA manufacturer ID, the range 0x7ff0 to 0x7fff can be used for prototyping.
rdmnet_controller_config_init(&config, MY_ESTA_MANUFACTURER_ID_VAL);

// Each controller is a component that must have a Component ID (CID), which is simply a UUID. Pure
// redistributable software apps may generate a new CID on each run, but hardware-locked devices
// should use a consistent CID locked to a MAC address (a V3 or V5 UUID).
etcpal_generate_os_preferred_uuid(&config.cid);

// Set the callback functions - defined elsewhere
// p_some_opaque_data is an opaque data pointer that will be passed back to each callback function
RDMNET_CONTROLLER_SET_CALLBACKS(&config, my_controller_connected_cb, my_controller_connect_failed_cb,
                                my_controller_disconnected_cb, my_controller_client_list_update_received_cb,
                                my_controller_rdm_response_received_cb, my_controller_status_received_cb,
                                p_some_opaque_data);

// Needed to identify this controller to other controllers on the network. More on this later.
RDMNET_CONTROLLER_SET_RDM_DATA(&config, "My Manufacturer Name", "My Product Name", "1.0.0", "My Device Label");

rdmnet_controller_t my_controller_handle;
etcpal_error_t result = rdmnet_controller_create(&config, &my_controller_handle);
if (result == kEtcPalErrOk)
{
  // Handle is valid and may be referenced in later calls to API functions.
}
else
{
  // Some error occurred, handle is not valid.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
class MyControllerNotifyHandler : public rdmnet::ControllerNotifyHandler
{
  // Implement the ControllerNotifyHandler callbacks...
};

MyControllerNotifyHandler my_controller_notify_handler;

// Needed to identify this controller to other controllers on the network. More on this later.
rdmnet::ControllerRdmData my_rdm_data("My Manufacturer Name",
                                      "My Product Name",
                                      "1.0.0",
                                      "My Device Label");
rdmnet::Controller controller;
etcpal::Error result = controller.Startup(my_controller_notify_handler,
                                           rdmnet::ControllerData::Default(MY_ESTA_MANUFACTURER_ID_VAL),my_rdm_data);
if (result)
{
  // Controller is valid and running.
}
else
{
  // Startup failed, use result.code() or result.ToString() to inspect details
}
```
<!-- CODE_BLOCK_END -->

## Managing Scopes

A controller instance is initially created without any configured scopes. If the app has not been
reconfigured by a user, the E1.33 RDMnet standard requires that the RDMnet default scope be
configured automatically. There is a shortcut function for this,
rdmnet_controller_add_default_scope().

Otherwise, use rdmnet_controller_add_scope() to add a custom configured scope. 

Per the requirements of RDMnet, a scope string is always UTF-8 and is thus represented by a char[]
in C and a std::string in C++.

See the "Scopes" section in \ref how_it_works for more information on scopes.

<!-- CODE_BLOCK_START -->
```c
// Add a default scope
rdmnet_client_scope_t default_scope_handle;
etcpal_error_t result = rdmnet_controller_add_default_scope(my_controller_handle, &default_scope_handle);

// Add a custom scope
RdmnetScopeConfig scope_config;
RDMNET_CLIENT_SET_SCOPE(&scope_config, "custom_scope_name");

rdmnet_client_scope_t custom_scope_handle;
etcpal_error_t result = rdmnet_controller_add_scope(my_controller_handle, &scope_config, &custom_scope_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
auto add_res = controller.AddDefaultScope();
// Or...
auto add_res = controller.AddScope("custom_scope_name");

if (add_res)
{
  rdmnet::ScopeHandle default_scope_handle = *add_res;
}
else
{
  // Handle error
  std::cout << "Error adding default scope: '" << add_res.result().ToString() << "'\n"
}
```
<!-- CODE_BLOCK_END -->

## Dynamic vs Static Scopes

Adding a scope will immediately begin the scope connection state machine from the RDMnet tick
thread. If a static configuration is not provided (using the RDMNET_CLIENT_SET_SCOPE() macro to
initialize an RdmnetScopeConfig, or calling rdmnet::Controller::AddScope() with only one argument)
the first action will be to attempt to discover brokers for this scope using DNS-SD. Once a broker
is found, connection will be initiated automatically, and the result will be delivered via either
the connected or connect_failed callback.

If a broker for a given scope has been configured with a static IP address and port, you can skip
DNS-SD discovery by providing a static configuration:

<!-- CODE_BLOCK_START -->
```c
// Get configured static broker address
EtcPalSockAddr static_broker_addr;
etcpal_inet_pton(kEtcPalIpTypeV4, "192.168.2.1", &static_broker_addr.ip);
static_broker_addr.port = 8000;

RdmnetScopeConfig config;
RDMNET_CLIENT_SET_STATIC_SCOPE(&config, "my_custom_scope", static_broker_addr);
// Or:
RDMNET_CLIENT_SET_STATIC_DEFAULT_SCOPE(&config, static_broker_addr);

rdmnet_client_scope_t scope_handle;
etcpal_error_t result = rdmnet_controller_static_config(my_controller_handle, &config, &scope_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get configured static broker address
auto static_broker_addr = etcpal::SockAddr(etcpal::IpAddr::FromString("192.168.2.1"), 8000);
auto add_res = controller.AddScope("my_custom_scope", static_broker_addr);
// Or:
auto add_res = controller.AddDefaultScope(static_broker_addr);
```
<!-- CODE_BLOCK_END -->

## Handling Callbacks

The library will dispatch callback notifications from the context in which rdmnet_core_tick() is
called (in the default configuration, this is a single dedicated worker thread). It is safe to call
any RDMnet API function from any callback; in fact, this is the recommended way of handling many
callbacks.

For example, a very common controller behavior will be to fetch a client list from the broker
after a successful connection:

<!-- CODE_BLOCK_START -->
```c
void controller_connected_callback(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                   const RdmnetClientConnectedInfo* info, void* context)
{
  // Check handles and/or context as necessary...
  rdmnet_controller_request_client_list(handle, scope_handle);
}                                   
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleConnectedToBroker(rdmnet::Controller& controller,
                                                        rdmnet::ScopeHandle scope_handle,
                                                        const RdmnetClientConnectedInfo& info)
{
  // Check handles as necessary...
  controller.RequestClientList(scope);
}
```
<!-- CODE_BLOCK_END -->

### Connection Failure

It's worth noting connection failure as a special case here. RDMnet connections can fail for many
reasons, including user misconfiguration, network misconfiguration, components starting up or
shutting down, programming errors, and more.

The RdmnetClientConnectFailedInfo and RdmnetClientDisconnectedInfo structs passed back with the 
"connect failed" and "disconnected" callbacks respectively have comprehensive information about the 
failure, including enum values containing the library's categorization of the failure, protocol
reason codes and socket errors where applicable. This information is typically used mostly for
logging and debugging. Each of these codes has a respective `to_string()` function to aid in
logging.

For programmatic use, the structs also contain a `will_retry` member which indicates whether the
library plans to retry this connection in the background. The only circumstances under which the 
library will not retry is when a connection failure is determined to be either a programming error
or a user misconfiguration. Some examples of these circumstances are:

* The broker explicitly rejected a connection with a reason code indicating a configuration error,
  such as `CONNECT_SCOPE_MISMATCH` or `CONNECT_DUPLICATE_UID`.
* The library failed to create a network socket before the connection was initiated.

## Discovering Devices

To discover devices in RDMnet, you need to request a _Client List_ from the broker you're connected
to. In our API, this is very easy - as we saw in the callbacks section earlier, we can just call
rdmnet_controller_request_client_list() or rdmnet::Controller::RequestClientList(). This sends the
appropriate request to the broker, and the reply will come back in the "Client List Update"
callback:

<!-- CODE_BLOCK_START -->
```c
void my_client_list_update_cb(rdmnet_controller_t handle, rdmnet_client_scope_t handle,
                              client_list_action_t list_action, const RptClientList* list, void* context)
{
  // Check handles and/or context as necessary...

  for (const RptClientEntry* entry = list->client_entries; entry < list->client_entries + list->num_client_entries;
       ++entry)
  {
    switch (list_action)
    {
      case kRdmnetClientListAppend:
        // These are new entries to be added to the list of clients. Append the new entry to our
        // bookkeeping.
        break;
      case kRdmnetClientListRemove:
        // These are entries to be removed from the list of clients. Remove the entry from our
        // bookkeeping.
        break;
      case kRdmnetClientListUpdate:
        // These are entries to be updated in the list of clients. Update the corresponding entry
        // in our bookkeeping.
        break;
      case kRdmnetClientListReplace:
        // This is the full client list currently connected to the broker - our existing client 
        // list should be replaced wholesale with this one. This will be the response to
        // rdmnet_controller_request_client_list(); the other cases are sent unsolicited.
        break;
    }
  }

  if (list->more_coming)
  {
    // The library ran out of memory pool space while allocating client entries - after this
    // callback returns, another will be delivered with the continuation of this response.
    // If RDMNET_DYNAMIC_MEM == 1 (the default on non-embedded platforms), this flag will never be
    // set to true and does not need to be checked.
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleClientListUpdate(rdmnet::Controller& controller, rdmnet::ScopeHandle scope_handle,
                                                       client_list_action_t list_action, const RptClientList& list)
{
  // Check handles as necessary...

  for (const RptClientEntry* entry = list.client_entries; entry < list.client_entries + list.num_client_entries;
       ++entry)
  {
    switch (list_action)
    {
      case kRdmnetClientListAppend:
        // These are new entries to be added to the list of clients. Append the new entry to our
        // bookkeeping.
        break;
      case kRdmnetClientListRemove:
        // These are entries to be removed from the list of clients. Remove the entry from our
        // bookkeeping.
        break;
      case kRdmnetClientListUpdate:
        // These are entries to be updated in the list of clients. Update the corresponding entry
        // in our bookkeeping.
        break;
      case kRdmnetClientListReplace:
        // This is the full client list currently connected to the broker - our existing client 
        // list should be replaced wholesale with this one. This will be the response to
        // rdmnet_controller_request_client_list(); the other cases are sent unsolicited.
        break;
    }
  }

  if (list.more_coming)
  {
    // The library ran out of memory pool space while allocating client entries - after this
    // callback returns, another will be delivered with the continuation of this response.
    // If RDMNET_DYNAMIC_MEM == 1 (the default on non-embedded platforms), this flag will never be
    // set to true and does not need to be checked.
  }
}
```
<!-- CODE_BLOCK_END -->

Brokers also send Client List Update messages asynchronously when things change; these messages
contain only the differences from the last client list sent to a given controller. This means you
should only have to request a full client list when a new connection completes; after that, expect
periodic callbacks notifying you of changes, with the #client_list_action_t set appropriately as
shown above.

Clients include both devices and other controllers; to differentiate the two, check the type field
in each RptClientEntry structure.

## Sending RDM Commands

Build RDM commands using the RdmnetLocalRdmCommand type. The library uses a naming convention where names
beginning with `Local` represent data that is generated locally, whereas names beginning with
`Remote` represent data received from a remote component.

<!-- CODE_BLOCK_START -->
```c
LocalRdmCommand cmd;
// Build the RDM command using cmd.rdm...
cmd.dest_uid = client_uid;
cmd.dest_endpoint = E133_NULL_ENDPOINT; // We're addressing this command to the default responder.

uint32_t cmd_seq_num;
etcpal_error_t result = rdmnet_controller_send_rdm_command(my_controller_handle, my_scope_handle, &cmd, &cmd_seq_num);
if (result == kEtcPalErrOk)
{
  // cmd_seq_num identifies this command transaction. Store it for when a response is received.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
LocalRdmCommand cmd;
// Build the RDM command using cmd.rdm...
cmd.dest_uid = client_uid;
cmd.dest_endpoint = E133_NULL_ENDPOINT; // We're addressing this command to the default responder.
etcpal::Expected<uint32_t> result = controller.SendRdmCommand(my_scope_handle, cmd);

// Alternatively, without using the RdmnetLocalRdmCommand structure:

etcpal::Expected<uint32_t> result = controller.SendRdmCommand(my_scope_handle, client_uid, E133_NULL_ENDPOINT,
                                                              my_rdm_command);

if (result)
{
  // *result identifies this command transaction. Store it for when a response is received.
}
```
<!-- CODE_BLOCK_END -->

## Handling RDM Responses

Responses to commands you send will be delivered asynchronously through the "RDM response" callback.
You may also receive "unsolicited responses" - asynchronous state updates from devices that don't
correspond to changes you requested.

<!-- CODE_BLOCK_START -->
```c
void rdm_response_callback(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                           const RdmnetRemoteRdmResponse* resp, void* context)
{
  // Check handles and/or context as necessary...

  if (resp->seq_num == 0)
  {
    // This is an unsolicited RDM response - an asynchronous update about a change you didn't
    // initiate.
  }
  else
  {
    // Verify resp->seq_num against the cmd_seq_num you stored earlier.
  }

  // If resp->seq_num != 0, resp->cmd will contain the original command you sent. 
  for (const RdmResponse* response = resp->responses; response < resp->responses + resp->num_responses; ++response)
  {
    // Process the list of responses.
  }

  if (resp->more_coming)
  {
    // The library ran out of memory pool space while allocating responses - after this callback
    // returns, another will be delivered with the continuation of this response.
    // If RDMNET_DYNAMIC_MEM == 1 (the default on non-embedded platforms), this flag will never be
    // set to true.
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleRdmResponse(rdmnet::Controller& controller, rdmnet::ScopeHandle scope_handle,
                                                  const RdmnetRemoteRdmResponse& resp)
{
  if (resp.seq_num == 0)
  {
    // This is an unsolicited RDM response - an asynchronous update about a change you didn't
    // initiate.
  }
  else
  {
    // Verify resp.seq_num against the result of rdmnet::Controller::SendRdmCommand() you stored earlier.
  }

  // If resp->seq_num != 0, resp.cmd will contain the command you sent.

  for (const RdmResponse* response = resp.responses; response < resp.responses + resp.num_responses; ++response)
  {
    // Process the list of responses.
  }

  if (resp.more_coming)
  {
    // The library ran out of memory pool space while allocating responses - after this callback
    // returns, another will be delivered with the continuation of this response.
    // If RDMNET_DYNAMIC_MEM == 1 (the default on non-embedded platforms), this flag will never be set to true.
  }
}
```
<!-- CODE_BLOCK_END -->

If something went wrong while either a broker or device was processing your message, you will get a
response called an "RPT Status". There is a separate callback for handling these messages.

When you send an RDM command, you start a transaction that is identified by a 32-bit sequence
number. That transaction is considered completed when you get either an RDM Response or an RPT
Status containing that same sequence number.

<!-- CODE_BLOCK_START -->
```c
void rpt_status_callback(rdmnet_controller_t handle, rdmnet_client_scope_t handle, const RemoteRptStatus* status,
                         void* context)
{
  // Check handles and/or context as necessary...

  // Verify status->seq_num against the cmd_seq_num you stored earlier.

  char uid_str[RDM_UID_STRING_BYTES];
  rdm_uid_to_string(&status->source_uid, uid_str);
  printf("Error sending RDM command to device %s: '%s'\n", uid_str, rpt_status_code_to_string(status->msg.status_code));
  
  // Other logic as needed; remove our internal storage of the RDM transaction, etc.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleRptStatus(rdmnet::Controller& controller, rdmnet::ScopeHandle scope_handle,
                                                const RemoteRptStatus& status)
{
  // Verify status.seq_num against the result of rdmnet::Controller::SendRdmCommand() you stored earlier.

  std::cout << "Error sending RDM command to device " << rdm::Uid(status.source_uid).ToString() << ": '"
            << rpt_status_code_to_string(status.msg.status_code) << "'\n";

  // Other logic as needed; remove our internal storage of the RDM transaction, etc.
}
```
<!-- CODE_BLOCK_END -->

## Handling RDM Commands

In addition to getting information about responders, RDMnet controllers are required to respond to
a basic set of RDM commands, which allows them to be identified by other controllers on the
network. By default, this behavior is implemented completely within the library.

The default implementation provides read-only access to the standard set of data that is required
to be readable from a controller. This includes the current scope(s) (`COMPONENT_SCOPE`), search
domain (`SEARCH_DOMAIN`), and RDMnet communication diagnostic data (`TCP_COMMS_STATUS`), as well as
some basic RDM data like the manufacturer (`MANUFACTURER_LABEL`), a description of the product
(`DEVICE_MODEL_DESCRIPTION`), the software version in string form (`SOFTWARE_VERSION_LABEL`) and a
user-settable label for the controller (`DEVICE_LABEL`). The library has access to all the
information necessary for the first three items, since that information is necessary for RDMnet
communication. Initial values for the last four items are provided to the library when creating a
new controller instance, through the RDMNET_CONTROLLER_SET_RDM_DATA() macro or the
rdmnet::ControllerRdmData structure.

If you want to provide richer RDM responder functionality from your controller implementation, you
can provide a set of callbacks to handle RDM commands. In this case, the library will no longer
handle any RDM commands on your behalf and you must handle all of the above PIDs, as well as
`SUPPORTED_PARAMETERS` and any additional ones you choose to support.

To use the library this way, you can:

<!-- CODE_BLOCK_START -->
```c
RdmnetControllerConfig config;
rdmnet_controller_config_init(&config, MY_ESTA_MANUFACTURER_ID_VAL);

// Generate CID and call RDMNET_CONTROLLER_SET_CALLBACKS() as above...

RDMNET_CONTROLLER_SET_RDM_CMD_CALLBACKS(&config, my_rdm_command_received_cb, my_llrp_rdm_command_received_cb);

rdmnet_controller_t my_controller_handle;
etcpal_error_t result = rdmnet_controller_create(&config, &my_controller_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
class MyControllerRdmCommandHandler : public rdmnet::ControllerRdmCommandHandler
{
  // Implement the ControllerRdmCommandHandler functions...
};

MyControllerRdmCommandHandler my_rdm_cmd_handler;

etcpal::Error result = controller.Startup(my_controller_notify_handler,
                                           rdmnet::ControllerData::Default(MY_ESTA_MANUFACTURER_ID_VAL),
                                           my_rdm_cmd_handler);
```
<!-- CODE_BLOCK_END -->
