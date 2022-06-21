# Using the Controller API                                                      {#using_controller}

The RDMnet Controller API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization

The RDMnet library must be globally initialized before using the RDMnet controller API. See
@ref global_init_and_destroy.

To create a controller instance, use the rdmnet_controller_create() function in C, or instantiate
an rdmnet::Controller and call its Startup() function in C++. Most apps will only need a single
controller instance. One controller can monitor an arbitrary number of RDMnet scopes at once.

The RDMnet controller API is an asynchronous, callback-oriented API. Part of the initial
configuration for a controller instance is a set of function pointers (or abstract interface) for
the library to use as callbacks. Callbacks are dispatched from the background thread that is
started when the RDMnet library is initialized.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/controller.h"

// Sets optional values to defaults. Must pass your ESTA manufacturer ID. If you have not yet
// requested an ESTA manufacturer ID, the range 0x7ff0 to 0x7fff can be used for prototyping.
RdmnetControllerConfig config = RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID_VAL);

// Each controller is a component that must have a Component ID (CID), which is simply a UUID.
// Software should generate and save a CID so that the same one is used on each run of the software.
etcpal_generate_os_preferred_uuid(&config.cid);

// Set the callback functions - defined elsewhere
// p_some_opaque_data is an opaque data pointer that will be passed back to each callback function
rdmnet_controller_set_callbacks(&config, my_controller_connected_cb, my_controller_connect_failed_cb,
                                my_controller_disconnected_cb, my_controller_client_list_update_received_cb,
                                my_controller_rdm_response_received_cb, my_controller_status_received_cb,
                                p_some_opaque_data);

// Needed to identify this controller to other controllers on the network. More on this later.
config.rdm_data.model_id = 0x0001;
config.rdm_data.software_version_id = 0x01000000;
config.rdm_data.manufacturer_label = "My Manufacturer Name";
config.rdm_data.device_model_description = "My Product Name";
config.rdm_data.software_version_label = "1.0.0";
config.rdm_data.device_label = "My Device Label";
config.rdm_data.device_label_settable = true;

rdmnet_controller_t my_controller_handle;
etcpal_error_t result = rdmnet_controller_create(&config, &my_controller_handle);
if (result == kEtcPalErrOk)
{
  // Handle is valid and may be referenced in later calls to API functions.
}
else
{
  printf("RDMnet controller creation failed with error: %s\n", etcpal_strerror(result));
}
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "etcpal/cpp/uuid.h"
#include "rdmnet/cpp/controller.h"

class MyControllerNotifyHandler : public rdmnet::Controller::NotifyHandler
{
  // Implement the NotifyHandler callbacks...
};

MyControllerNotifyHandler my_controller_notify_handler;

// Each controller is a component that must have a Component ID (CID), which is simply a UUID.
// Software should generate and save a CID so that the same one is used on each run of the software.
etcpal::Uuid my_cid = etcpal::Uuid::OsPreferred();

// Contains the configuration settings that the controller needs to operate. Some of these are set
// to default values and can be changed if necessary. Must pass your ESTA manufacturer ID. If you
// have not yet requested an ESTA manufacturer ID, the range 0x7ff0 to 0x7fff can be used for
// prototyping.
rdmnet::Controller::Settings my_settings(my_cid, MY_ESTA_MANUFACTURER_ID_VAL);

// Needed to identify this controller to other controllers on the network. More on this later.

rdmnet::Controller::RdmData my_rdm_data(0x0001,                   // Device Model ID
                                        0x01000000,               // Software Version ID
                                        "My Manufacturer Name",   // Manufacturer Label
                                        "My Product Name",        // Device Model Description
                                        "1.0.0",                  // Software Version Label
                                        "My Device Label");       // Device Label
rdmnet::Controller controller;
etcpal::Error result = controller.Startup(my_controller_notify_handler, my_settings, my_rdm_data);
if (result)
{
  // Controller is valid and running.
  rdmnet::Controller::Handle handle = controller.handle();
  // Store handle for later lookup from the NotifyHandler callback functions.
}
else
{
  std::cout << "RDMnet controller startup failed with error: " << result.ToString() << '\n';
}
```
<!-- CODE_BLOCK_END -->

## Deinitialization

The controller should be shut down and destroyed gracefully before program termination. This will send
a graceful disconnect message to any connected brokers and deallocate the controller's resources.

<!-- CODE_BLOCK_START -->
```c
rdmnet_controller_destroy(my_controller_handle, kRdmnetDisconnectShutdown);

// At this point, the controller is no longer running and its handle is no longer valid.
```
<!-- CODE_BLOCK_MID -->
```cpp
controller.Shutdown();

// At this point, the controller is no longer running and its handle is no longer valid. It can be
// started again (with a new handle) by calling Startup() again.
```
<!-- CODE_BLOCK_END -->

## Managing Scopes

A controller instance is initially created without any configured scopes. If the app has not been
reconfigured by a user, the E1.33 RDMnet standard requires that the RDMnet default scope be
configured automatically. There is a shortcut function for this. Otherwise, you can add a custom
scope.

Per the requirements of RDMnet, a scope string is always UTF-8 and is thus represented by a char[]
in C and a std::string in C++.

See the "Scopes" section in @ref how_it_works for more information on scopes.

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
etcpal::Expected<rdmnet::ScopeHandle> add_res = controller.AddDefaultScope();
// Or...
etcpal::Expected<rdmnet::ScopeHandle> add_res = controller.AddScope("custom_scope_name");

if (add_res)
{
  rdmnet::ScopeHandle default_scope_handle = *add_res;
}
else
{
  std::cout << "Error adding default scope: '" << add_res.error().ToString() << "'\n"
}
```
<!-- CODE_BLOCK_END -->

### Dynamic vs Static Scopes

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
etcpal_string_to_ip(kEtcPalIpTypeV4, "192.168.2.1", &static_broker_addr.ip);
static_broker_addr.port = 8000;

RdmnetScopeConfig config;
RDMNET_CLIENT_SET_STATIC_SCOPE(&config, "my_custom_scope", static_broker_addr);
// Or:
RDMNET_CLIENT_SET_STATIC_DEFAULT_SCOPE(&config, static_broker_addr);

rdmnet_client_scope_t scope_handle;
etcpal_error_t result = rdmnet_controller_add_scope(my_controller_handle, &config, &scope_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get configured static broker address
etcpal::Sockaddr static_broker_addr(etcpal::IpAddr::FromString("192.168.2.1"), 8000);
etcpal::Expected<rdmnet::ScopeHandle> add_res = controller.AddScope("my_custom_scope", static_broker_addr);
// Or:
etcpal::Expected<rdmnet::ScopeHandle> add_res = controller.AddDefaultScope(static_broker_addr);
```
<!-- CODE_BLOCK_END -->

## Handling Callbacks

The library will dispatch callback notifications from the background thread which is started when
rdmnet_init() is called. It is safe to call any RDMnet API function from any callback; in fact,
this is the recommended way of handling many callbacks.

For example, a very common controller behavior will be to fetch a client list from the broker
after a successful connection:

<!-- CODE_BLOCK_START -->
```c
void controller_connected_callback(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                                   const RdmnetClientConnectedInfo* info, void* context)
{
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&info->broker_addr.ip, addr_str);
  printf("Connected to broker '%s' at address %s:%d\n", info->broker_name, addr_str, info->broker_addr.port);

  // Check handles and/or context as necessary...

  rdmnet_controller_request_client_list(controller_handle, scope_handle);
}                                   
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleConnectedToBroker(rdmnet::Controller::Handle controller_handle,
                                                        rdmnet::ScopeHandle scope_handle,
                                                        const rdmnet::ClientConnectedInfo& info)
{
  std::cout << "Connected to broker '" << info.broker_name()
            << "' at IP address " << info.broker_addr().ToString() << '\n';

  // Check handles as necessary and get controller instance...
  controller.RequestClientList(scope_handle);
}
```
<!-- CODE_BLOCK_END -->

### Connection Failure

It's worth noting connection failure as a special case here. RDMnet connections can fail for many
reasons, including user misconfiguration, network misconfiguration, components starting up or
shutting down, programming errors, and more.

The ClientConnectFailedInfo and ClientDisconnectedInfo structs passed back with the 
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
void my_client_list_update_cb(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                              client_list_action_t list_action, const RdmnetRptClientList* list, void* context)
{
  // Check handles and/or context as necessary...

  switch (list_action)
  {
    case kRdmnetClientListAppend:
      // These are new entries to be added to the list of clients. Append the new entry to our
      // bookkeeping.
      add_new_clients(list->client_entries, list->num_client_entries);
      break;
    case kRdmnetClientListRemove:
      // These are entries to be removed from the list of clients. Remove the entry from our
      // bookkeeping.
      remove_clients(list->client_entries, list->num_client_entries);
      break;
    case kRdmnetClientListUpdate:
      // These are entries to be updated in the list of clients. Update the corresponding entry
      // in our bookkeeping.
      update_clients(list->client_entries, list->num_client_entries);
      break;
    case kRdmnetClientListReplace:
      // This is the full client list currently connected to the broker - our existing client 
      // list should be replaced wholesale with this one. This will be the response to
      // rdmnet_controller_request_client_list(); the other cases are sent unsolicited.
      replace_client_list(list->client_entries, list->num_client_entries);
      break;
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
void MyControllerNotifyHandler::HandleClientListUpdate(rdmnet::Controller::Handle controller_handle,
                                                       rdmnet::ScopeHandle scope_handle,
                                                       client_list_action_t list_action,
                                                       const rdmnet::RptClientList& list)
{
  // Check handles as necessary...

  switch (list_action)
  {
    case kRdmnetClientListAppend:
      // These are new entries to be added to the list of clients. Append the new entry to our
      // bookkeeping.
      AddNewClients(list.GetClientEntries());
      break;
    case kRdmnetClientListRemove:
      // These are entries to be removed from the list of clients. Remove the entry from our
      // bookkeeping.
      RemoveClients(list.GetClientEntries());
      break;
    case kRdmnetClientListUpdate:
      // These are entries to be updated in the list of clients. Update the corresponding entry
      // in our bookkeeping.
      UpdateClients(list.GetClientEntries());
      break;
    case kRdmnetClientListReplace:
      // This is the full client list currently connected to the broker - our existing client 
      // list should be replaced wholesale with this one. This will be the response to
      // Controller::RequestClientList(); the other cases are sent unsolicited.
      ReplaceClientList(list.GetClientEntries());
      break;
  }

  if (list.more_coming())
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
in each RPT client entry structure.

## Sending RDM Commands

Sending RDM commands requires a destination address structure to indicate the RDMnet component and
RDM responder to which the command is addressed. See @ref devices_and_gateways for more information
on the fields of the destination address structure.

The caller retains ownership of any data buffers supplied to the RDM command sending API functions.
See @ref data_ownership for more information.

After sending a command, the library will provide a _sequence number_ that will be echoed in the
corresponding RDM response. This sequence number should be saved.

<!-- CODE_BLOCK_START -->
```c
RdmnetDestinationAddr dest = RDMNET_ADDR_TO_DEFAULT_RESPONDER(0x6574, 0x12345678);
uint32_t cmd_seq_num;
etcpal_error_t result = rdmnet_controller_send_get_command(my_controller_handle, my_scope_handle, &dest,
                                                           E120_DEVICE_LABEL, NULL, 0, &cmd_seq_num);
if (result == kEtcPalErrOk)
{
  // cmd_seq_num identifies this command transaction. Store it for when a response is received.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
auto dest_addr = rdmnet::DestinationAddr::ToDefaultResponder(0x6574, 0x12345678);
etcpal::Expected<uint32_t> result = controller.SendGetCommand(my_scope_handle, dest_addr, E120_DEVICE_LABEL);

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

Responses are always delivered atomically in RDMnet (contrast this with RDM, which uses the
ACK_OVERFLOW mechanism to deliver responses with oversized data). This means that RDM response data
in RDMnet can be larger than the 231-byte limit that is customary in RDM. The only time a response
can be fragmented is if the RDMnet library was compiled with dynamic memory allocation disabled 
(see the note on the more_coming flag below for more information on this).

Note that response callbacks reference data buffers owned by the library, which will be invalid
when the callback returns. See @ref data_ownership for more information.

<!-- CODE_BLOCK_START -->
```c
bool rdm_response_callback(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                           const RdmnetRdmResponse* resp, void* context)
{
  // Check handles and/or context as necessary...

  if (resp->is_response_to_me)
  {
    // Verify resp->seq_num against the cmd_seq_num you stored earlier. This command transaction is
    // now finished.
  }
  else
  {
    // This is either an unsolicited RDM response (an asynchronous update about a change you didn't initiate)
    // or a response to a command by another controller. Use it to update your cached data about this
    // RDMnet responder.
  }

  // If RDMNET_RESP_ORIGINAL_COMMAND_INCLUDED(resp) == true, resp->cmd will contain the original command you sent. 

  handle_response_data(resp->rdm_header.param_id, resp->rdm_data, resp->rdm_data_len);

  // RdmnetRdmResponse structures do not own their data and the data will be invalid when this callback
  // returns. To save the data for later processing:
  RdmnetSavedRdmResponse saved_resp;
  rdmnet_save_rdm_response(resp, &saved_resp);

  if (resp->more_coming)
  {
    // The library ran out of memory pool space while allocating responses - after this callback
    // returns, another will be delivered with the continuation of this response data.
    // If RDMNET_DYNAMIC_MEM == 1 (the default on non-embedded platforms), this flag will never be
    // set to true.

    // When more responses come, you can append their data to your saved response:
    rdmnet_append_to_saved_rdm_response(resp, &previously_saved_resp);
  }

  // Ready to process the next response, so return true.
  return true;
}
```
<!-- CODE_BLOCK_MID -->
```cpp
bool MyControllerNotifyHandler::HandleRdmResponse(rdmnet::Controller::Handle controller_handle,
                                                  rdmnet::ScopeHandle scope_handle,
                                                  const rdmnet::RdmResponse& resp)
{
  // Check handles as necessary...

  if (resp.is_response_to_me())
  {
    // Verify resp.seq_num() against the command seq_num you stored earlier. This command transaction is
    // now finished.
  }
  else
  {
    // This is either an unsolicited RDM response (an asynchronous update about a change you didn't initiate)
    // or a response to a command by another controller. Use it to update your cached data about this
    // RDMnet responder.
  }

  // If resp.OriginalCommandIncluded() == true, the original_cmd_...() getters will contain the data
  // of the original command that instigated this response.

  HandleResponseData(resp.param_id(), resp.data(), resp.data_len());

  // RdmResponse classes do not own their data and the data will be invalid when this callback returns.
  // To save the data for later processing:
  rdmnet::SavedRdmResponse saved_resp = resp.Save();

  if (resp.more_coming())
  {
    // The library ran out of memory pool space while allocating responses - after this callback
    // returns, another will be delivered with the continuation of this response.
    // If RDMNET_DYNAMIC_MEM == 1 (the default on non-embedded platforms), this flag will never be set to true.

    // When more responses come, you can append their data to your saved response:
    previously_saved_resp.AppendData(resp);
  }

  // Ready to process the next response, so return true.
  return true;
}
```
<!-- CODE_BLOCK_END -->

### Delaying Processing of RDM Responses

In the case where your application doesn't have the resources to process an RDM response, you can
return false to trigger another notification for the same response at a later time:

<!-- CODE_BLOCK_START -->
```c
bool rdm_response_callback(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                           const RdmnetRdmResponse* resp, void* context)
{
  // Check handles and/or context as necessary...

  if(my_app.rdm_response_queue_has_room)
  {
    // Save response and add it to the queue...

    // Ready for the next response, so return true.
    return true;
  }
  
  // Can't queue this response yet, so return false. This function will be called again later with the same response.
  return false;
}
```
<!-- CODE_BLOCK_MID -->
```cpp
bool MyControllerNotifyHandler::HandleRdmResponse(rdmnet::Controller::Handle controller_handle,
                                                  rdmnet::ScopeHandle scope_handle,
                                                  const rdmnet::RdmResponse& resp)
{
  // Check handles as necessary...

  if(my_app_.RdmResponseQueueHasRoom())
  {
    // Save response and add it to the queue...

    // Ready for the next response, so return true.
    return true;
  }
  
  // Can't queue this response yet, so return false. This function will be called again later with the same response.
  return false;
}
```
<!-- CODE_BLOCK_END -->

The application can do this consecutively for as long as needed, but keep in mind that this will
delay the library's processing of future messages from the broker (ultimately delaying potential
notifications), and may also narrow the TCP window of the connection with the broker, potentially
resulting in backpressure.

## Handling RPT Status Messages

If something went wrong while either a broker or device was processing your message, you will get a
response called an "RPT Status". There is a separate callback for handling these messages.

When you send an RDM command, you start a transaction that is identified by a 32-bit sequence
number. That transaction is considered completed when you get either an RDM Response or an RPT
Status containing that same sequence number.

<!-- CODE_BLOCK_START -->
```c
void rpt_status_callback(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetRptStatus* status, void* context)
{
  // Check handles and/or context as necessary...

  // Verify status->seq_num against the cmd_seq_num you stored earlier.

  char uid_str[RDM_UID_STRING_BYTES];
  rdm_uid_to_string(&status->source_uid, uid_str);
  printf("Error sending RDM command to device %s: '%s'\n", uid_str,
         rdmnet_rpt_status_code_to_string(status->status_code));
  
  // Other logic as needed; remove our internal storage of the RDM transaction, etc.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleRptStatus(rdmnet::Controller::Handle controller_handle,
                                                rdmnet::ScopeHandle scope_handle,
                                                const rdmnet::RptStatus& status)
{
  // Check handles as necessary...

  // Verify status.seq_num() against the result of rdmnet::Controller::SendRdmCommand() you stored earlier.

  std::cout << "Error sending RDM command to device " << status.source_uid().ToString() << ": '"
            << status.CodeToString() << "'\n";

  // Other logic as needed; remove our internal storage of the RDM transaction, etc.
}
```
<!-- CODE_BLOCK_END -->

## Getting Responder IDs

Controllers may encounter RDMnet responders which have dynamic UIDs. Base RDMnet components such as
controllers and devices can have dynamic UIDs, as can virtual responders present on devices. See
@ref devices_and_gateways for more information on virtual responders, and @ref roles_and_addressing
for more information on dynamic UIDs. As a reminder, a dynamic UID is identified by the top bit of
the Manufacturer ID portion being set and the UID not being a broadcast UID; this can be tested
using the convenience methods in the RDM library: RDMNET_UID_IS_DYNAMIC() and/or
rdm::Uid::IsDynamic().

A responder using a dynamic UID may get a different UID at any time. For this reason, it is useful
for a controller to be able to get a more permanent identifier for an RDMnet responder.

For controllers, devices and brokers, the permanent identifier is the CID, which is present
alongside the UID in an RPT client list entry. If you want to track these components beyond a
single RDMnet session, be sure to store the CID.

Virtual responders present on a device have a similar identifier called a Responder ID (RID) which
has the same function as a CID for that virtual responder. The UIDs of a device's virtual
responders are obtained using the `ENDPOINT_RESPONDERS` RDM command; however, this command's data
does not include the RID. To obtain RIDs for a set of virtual responders, it is necessary to query
the connected broker.

<!-- CODE_BLOCK_START -->
```c
void handle_response_data(uint16_t param_id, const uint8_t* param_data, size_t param_data_len)
{
  // This code is simplified and meant to illustrate an example of discovering virtual responders
  // using the ENDPOINT_RESPONDERS command.
  if (param_id == E137_7_ENDPOINT_RESPONDERS)
  {
    size_t num_uids = ((param_data_len - 6) / 6); // 6 bytes per UID, subtract the other data field sizes
    RdmUid* responder_uids = (RdmUid*)calloc(num_uids, sizeof(RdmUid));

    // Unpack the response data into the responder_uids array...

    // Now request the RIDs from the broker for the responder UIDs.
    etcpal_error_t result = rdmnet_controller_request_responder_ids(my_controller_handle, my_scope_handle,
                                                                    responder_uids, num_uids);
    if (result == kEtcPalErrOk)
    {
      // The broker's response will be forwarded to the RdmnetControllerResponderIdsReceivedCallback,
      // defined in the next snippet.
    }

    free(responder_uids);
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleResponseData(uint16_t param_id, const uint8_t* param_data, size_t param_data_len)
{
  // This code is simplified and meant to illustrate an example of discovering virtual responders
  // using the ENDPOINT_RESPONDERS command.
  if (param_id == E137_7_ENDPOINT_RESPONDERS)
  {
    std::vector<RdmUid> responder_uids;

    // Unpack the response data into the responder_uids array...

    // Now request the RIDs from the broker for the responder UIDs.
    etcpal::Error result = controller.RequestResponderIds(responder_uids);
    if (result)
    {
      // The broker's response will be forwarded to the MyControllerNotifyHandler::HandleResponderIdsReceived()
      // callback, defined in the next snippet.
    }
  }
}
```
<!-- CODE_BLOCK_END -->

The broker will respond with a list of mappings between dynamic UIDs and RIDs known as a Dynamic
UID Assignment List. Each entry in the list will contain a status code indicating whether the RID
was looked up successfully, followed by the UID and RID.

<!-- CODE_BLOCK_START -->
```c
void handle_responder_ids_received(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                                   const RdmnetDynamicUidAssignmentList* list, void* context)
{
  // Check handles and/or context as necessary...

  for (const RdmnetDynamicUidMapping* mapping = list->mappings; mapping < list->mappings + list->num_mappings;
       ++mapping)
  {
    if (mapping->status_code == kRdmnetDynamicUidStatusOk)
    {
      // This function is assumed to add the RID to the responder's cached data.
      add_responder_rid(&mapping->uid, &mapping->rid);
    }
    else
    {
      char uid_str[RDM_UID_STRING_BYTES];
      rdm_uid_to_string(&mapping->uid, uid_str);
      printf("Error obtaining RID for responder %s: '%s'\n", uid_str,
             rdmnet_dynamic_uid_status_to_string(mapping->status_code));
    }
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleResponderIdsReceived(rdmnet::Controller::Handle controller_handle,
                                                           rdmnet::ScopeHandle scope_handle,
                                                           const rdmnet::DynamicUidAssignmentList& list)
{
  // Check handles as necessary...

  for (const rdmnet::DynamicUidMapping& mapping : list.GetMappings())
  {
    if (mapping.IsOk())
    {
      // This function is assumed to add the RID to the responder's cached data.
      AddResponderRid(mapping.uid, mapping.rid);
    }
    else
    {
      std::cout << "Error obtaining RID for responder " << mapping.uid.ToString() << ": '"
                << mapping.CodeToString() << "'\n";
    }
  }
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
new controller instance, through the rdmnet_controller_set_rdm_data() function or the
rdmnet::Controller::RdmData structure.

If you want to provide richer RDM responder functionality from your controller implementation, you
can provide a set of callbacks to handle RDM commands. In this case, the library only handle
`TCP_COMMS_STATUS`, and you must handle the rest of the above PIDs, as well as
`SUPPORTED_PARAMETERS` and any additional ones you choose to support.

To use the library this way, you can:

<!-- CODE_BLOCK_START -->
```c
RdmnetControllerConfig config = RDMNET_CONTROLLER_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID_VAL);

// Generate CID and call rdmnet_controller_set_callbacks() as above...

rdmnet_controller_set_rdm_cmd_callbacks(&config, my_rdm_command_received_cb, my_llrp_rdm_command_received_cb);

rdmnet_controller_t my_controller_handle;
etcpal_error_t result = rdmnet_controller_create(&config, &my_controller_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
class MyControllerRdmCommandHandler : public rdmnet::Controller::RdmCommandHandler
{
  // Implement the RdmCommandHandler functions...
};

MyControllerRdmCommandHandler my_rdm_cmd_handler;
rdmnet::Controller::Settings my_settings(my_cid, MY_ESTA_MANUFACTURER_ID_VAL);

etcpal::Error result = controller.Startup(my_controller_notify_handler, my_settings, my_rdm_cmd_handler);
```
<!-- CODE_BLOCK_END -->

See @ref handling_rdm_commands for information about how to handle commands through the callbacks.
