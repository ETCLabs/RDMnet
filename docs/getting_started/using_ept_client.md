# Using the EPT Client API                                                      {#using_ept_client}

The EPT Client API exposes both a C and C++ language interface. The C++ interface is a header-only
wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization

The RDMnet library must be globally initialized before using the EPT Client API. See
@ref global_init_and_destroy.

To create an EPT client instance, use the rdmnet_ept_client_create() function in C, or instantiate
an rdmnet::EptClient and call its Startup() function in C++.

The EPT Client API is an asynchronous, callback-oriented API. Part of the initial configuration
for an EPT client instance is a set of function pointers (or abstract interface) for the library
to use as callbacks. Callbacks are dispatched from the background thread that is started when the
RDMnet library is initialized.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/ept_client.h"

// Sets optional values to defaults. 
RdmnetEptClientConfig config = RDMNET_EPT_CLIENT_CONFIG_DEFAULT_INIT;

// Each EPT client is a component that must have a Component ID (CID), which is simply a UUID.
// Software should generate and save a CID so that the same one is used on each run of the software.
etcpal_generate_os_preferred_uuid(&config.cid);

// Set the callback functions - defined elsewhere
// p_some_opaque_data is an opaque data pointer that will be passed back to each callback function
rdmnet_ept_client_set_callbacks(&config, my_ept_connected_cb, my_ept_connect_failed_cb, my_ept_disconnected_cb,
                                my_ept_client_list_update_received_cb, my_ept_data_received_cb,
                                my_ept_status_received_cb, p_some_opaque_data);

// Provide information about the EPT sub-protocols we support. Protocols are namespaced under your
// ESTA manufacturer ID. If you have not yet requested an ESTA manufacturer ID, the range 0x7ff0 to
// 0x7fff can be used for prototyping.
RdmnetEptSubProtocol my_protocols[2] = {
  {
    MY_ESTA_MANUFACTURER_ID, // Our ESTA manufacturer ID.
    1,                       // The protocol number.
    "My protocol 1"          // A descriptive string about the protocol.
  },
  {
    MY_ESTA_MANUFACTURER_ID,
    2,
    "My protocol 2"
  }
};
config.protocols = my_protocols;
config.num_protocols = 2;

rdmnet_ept_client_t my_ept_client_handle;
etcpal_error_t result = rdmnet_ept_client_create(&config, &my_ept_client_handle);
if (result == kEtcPalErrOk)
{
  // Handle is valid and may be referenced in later calls to API functions.
}
else
{
  printf("RDMnet EPT client creation failed with error: %s\n", etcpal_strerror(result));
}
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "rdmnet/cpp/ept_client.h"

class MyEptNotifyHandler : public rdmnet::EptClient::NotifyHandler
{
  // Implement the NotifyHandler callbacks...
};

MyEptNotifyHandler my_ept_notify_handler;

// Each EPT client is a component that must have a Component ID (CID), which is simply a UUID.
// Software should generate and save a CID so that the same one is used on each run of the software.
etcpal::Uuid my_cid = etcpal::Uuid::OsPreferred();

// Provide information about the EPT sub-protocols we support. Protocols are namespaced under your
// ESTA manufacturer ID. If you have not yet requested an ESTA manufacturer ID, the range 0x7ff0 to
// 0x7fff can be used for prototyping.
std::vector<rdmnet::EptSubProtocol> my_protocols = {
  {
    MY_ESTA_MANUFACTURER_ID, // Our ESTA manufacturer ID.
    1,                       // The protocol number.
    "My protocol 1"          // A descriptive string about the protocol.
  },
  {
    MY_ESTA_MANUFACTURER_ID,
    2,
    "My protocol 2"
  }
};

// Contains the configuration settings that the EPT client needs to operate. Some of these are set
// to default values and can be changed if necessary. 
rdmnet::EptClient::Settings my_settings(my_cid, my_protocols);

rdmnet::EptClient ept_client;
etcpal::Error result = ept_client.Startup(my_ept_notify_handler, my_settings);
if (result)
{
  // EPT client is valid and running.
  rdmnet::EptClient::Handle handle = ept_client.handle();
  // Store handle for later lookup from the NotifyHandler callback functions.
}
else
{
  std::cout << "RDMnet EPT client startup failed with error: " << result.ToString() << '\n';
}
```
<!-- CODE_BLOCK_END -->

## Deinitialization

The EPT client should be shut down and destroyed gracefully before program termination. This will
send a graceful disconnect message to any connected brokers and deallocate the EPT client's
resources.

<!-- CODE_BLOCK_START -->
```c
rdmnet_ept_client_destroy(my_ept_client_handle, kRdmnetDisconnectShutdown);

// At this point, the EPT client is no longer running and its handle is no longer valid.
```
<!-- CODE_BLOCK_MID -->
```cpp
ept_client.Shutdown();

// At this point, the EPT client is no longer running and its handle is no longer valid. It can be
// started again (with a new handle) by calling Startup() again.
```
<!-- CODE_BLOCK_END -->

## Managing Scopes

An EPT client instance is initially created without any configured scopes. If the app has not been
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
etcpal_error_t result = rdmnet_ept_client_add_default_scope(my_ept_client_handle, &default_scope_handle);

// Add a custom scope
RdmnetScopeConfig scope_config;
RDMNET_CLIENT_SET_SCOPE(&scope_config, "custom_scope_name");

rdmnet_client_scope_t custom_scope_handle;
etcpal_error_t result = rdmnet_ept_client_add_scope(my_ept_client_handle, &scope_config, &custom_scope_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
etcpal::Expected<rdmnet::ScopeHandle> add_res = ept_client.AddDefaultScope();
// Or...
etcpal::Expected<rdmnet::ScopeHandle> add_res = ept_client.AddScope("custom_scope_name");

if (add_res)
{
  rdmnet::ScopeHandle default_scope_handle = *add_res;
}
else
{
  std::cout << "Error adding default scope: '" << add_res.result().ToString() << "'\n"
}
```
<!-- CODE_BLOCK_END -->

### Dynamic vs Static Scopes

Adding a scope will immediately begin the scope connection state machine from the RDMnet tick
thread. If a static configuration is not provided (using the RDMNET_CLIENT_SET_SCOPE() macro to
initialize an RdmnetScopeConfig, or calling rdmnet::EptClient::AddScope() with only one argument)
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
etcpal_error_t result = rdmnet_ept_client_add_scope(my_ept_client_handle, &config, &scope_handle);
```
<!-- CODE_BLOCK_MID -->
```cpp
// Get configured static broker address
etcpal::SockAddr static_broker_addr(etcpal::IpAddr::FromString("192.168.2.1"), 8000);
etcpal::Expected<rdmnet::ScopeHandle> add_res = ept_client.AddScope("my_custom_scope", static_broker_addr);
// Or:
etcpal::Expected<rdmnet::ScopeHandle> add_res = ept_client.AddDefaultScope(static_broker_addr);
```
<!-- CODE_BLOCK_END -->

## Handling Callbacks

The library will dispatch callback notifications from the background thread which is started when
rdmnet_init() is called. It is safe to call any RDMnet API function from any callback; in fact,
this is the recommended way of handling many callbacks.

For example, a very common EPT client behavior will be to fetch a client list from the broker
after a successful connection:

<!-- CODE_BLOCK_START -->
```c
void ept_connected_callback(rdmnet_ept_client_t client_handle, rdmnet_client_scope_t scope_handle,
                            const RdmnetClientConnectedInfo* info, void* context)
{
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&info->broker_addr.ip, addr_str);
  printf("Connected to broker '%s' at address %s:%d\n", info->broker_name, addr_str, info->broker_addr.port);

  // Check handles and/or context as necessary...

  rdmnet_ept_client_request_client_list(client_handle, scope_handle);
}                                   
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyEptNotifyHandler::HandleConnectedToBroker(rdmnet::EptClient::Handle client_handle,
                                                 rdmnet::ScopeHandle scope_handle,
                                                 const rdmnet::ClientConnectedInfo& info)
{
  std::cout << "Connected to broker '" << info.broker_name()
            << "' at IP address " << info.broker_addr().ToString() << '\n';

  // Check handles as necessary and get EPT client instance...
  ept_client.RequestClientList(scope_handle);
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

## Discovering Other Clients

To discover other EPT clients in RDMnet, you need to request a _Client List_ from the broker you're
connected to. In our API, this is very easy - as we saw in the callbacks section earlier, we can
just call rdmnet_ept_client_request_client_list() or rdmnet::EptClient::RequestClientList(). This
sends the appropriate request to the broker, and the reply will come back in the
"Client List Update" callback:

<!-- CODE_BLOCK_START -->
```c
void my_client_list_update_cb(rdmnet_ept_client_t client_handle, rdmnet_client_scope_t scope_handle,
                              client_list_action_t list_action, const RdmnetEptClientList* list, void* context)
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
      // rdmnet_ept_client_request_client_list(); the other cases are sent unsolicited.
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
void MyEptNotifyHandler::HandleClientListUpdate(rdmnet::EptClient::Handle client_handle,
                                                rdmnet::ScopeHandle scope_handle,
                                                client_list_action_t list_action,
                                                const rdmnet::EptClientList& list)
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
      // EptClient::RequestClientList(); the other cases are sent unsolicited.
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

Each EPT client entry structure contains a list of sub-protocols that client supports (along with
its CID). This list should be checked for protocols that match those of the local EPT client to
determine which clients you are interested in talking to. Make sure to save those clients' CIDs;
that is the primary addressing identifier you will use to send data to them.

## Sending Data

Send data to other EPT clients by providing the data, the EPT sub-protocol being used, and the
destination client's CID.

The caller retains ownership of any data buffers supplied to the data sending API functions. See
@ref data_ownership for more information.

<!-- CODE_BLOCK_START -->
```c
EtcPalUuid dest_cid = /* The CID of the client we are sending the data to */;

uint8_t data[20]; // An arbitrary data buffer of arbitrary size
// Fill in data with some protocol-specific opaque data to send...

// Send data for sub-protocol 1 to the client indicated by dest_cid
etcpal_error_t result = rdmnet_ept_client_send_data(my_ept_client_handle, my_scope_handle, &dest_cid,
                                                    MY_ESTA_MANUFACTURER_ID, 1, data, 20);
```
<!-- CODE_BLOCK_MID -->
```cpp
// The CID of the client we are sending the data to
etcpal::Uuid dest_cid = etcpal::Uuid::FromString("7f0bb8b0-fead-40c0-b657-d92d39e57586");

std::array<uint8_t, 20> data; // An arbitrary data buffer of arbitrary size
// Fill in data with some protocol-specific opaque data to send...

// Send data for sub-protocol 1 to the client indicated by dest_cid
etcpal::Error result = ept_client.SendData(my_scope_handle, dest_cid, MY_ESTA_MANUFACTURER_ID, 1,
                                           data.data(), data.size());
```
<!-- CODE_BLOCK_END -->

## Handling Data

EPT makes no distinction between requests and replies and has no method of correlating them; these
features are up to sub-protocols to implement. When data arrives addressed to a local client, it
will be delivered asynchronously through the "data received" callback.

Note that data received callbacks reference data buffers owned by the library, which will be
invalid when the callback returns. See @ref data_ownership for more information.

<!-- CODE_BLOCK_START -->
```c
void ept_data_received_callback(rdmnet_ept_client_t client_handle, rdmnet_client_scope_t scope_handle,
                                const RdmnetEptData* data, RdmnetSyncEptResponse* response, void* context)
{
  // Check handles and/or context as necessary...

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&data->source_cid, cid_str);
  printf("Got data from client %s for protocol %04x:%04x...\n", cid_str, data->manufacturer_id, data->protocol_id);

  // data->data and data->data_len will point to the actual protocol-specific opaque-data.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
rdmnet::EptResponseAction MyEptNotifyHandler::HandleEptData(rdmnet::EptClient::Handle client_handle,
                                                            rdmnet::ScopeHandle scope_handle,
                                                            const rdmnet::EptData& data)
{
  // Check handles as necessary...

  std::cout << "Got data from client " << data.source_cid().ToString()
            << " for protocol " << data.manufacturer_id() << ':' << data.protocol_id() << "...\n";

  // data.data() and data.data_len() will point to the actual protocol-specific opaque data.

  // More on this below...
  return rdmnet::EptResponseAction::DeferResponse();
}
```
<!-- CODE_BLOCK_END -->

### Responding Synchronously to EPT Data

If you receive EPT data that motivates a response, and you can access the data with which to
respond relatively quickly from the code path of the notification callback, it's convenient to
respond synchronously. The EPT data received notification callbacks provide a way to set the
response information directly from the callback, which the library will use to send a response
after the callback returns.

_Important note: Do not do this when accessing response data may block for a significant time._

When responding with EPT data, it should be copied into the buffer that was provided when the EPT
client handle was created, before returning from the callback function. All notifications for any
given RDMnet handle are delivered from the same thread, so accesses to this buffer from the
callback context are thread-safe. 

After copying the data, use the API-specific method to indicate that data should be sent in
response as shown below.

<!-- CODE_BLOCK_START -->
```c
// This buffer was provided as part of the RdmnetEptClientConfig when the device handle was created.
static uint8_t my_ept_response_buf[MY_MAX_RESPONSE_SIZE];

void ept_data_received_callback(rdmnet_ept_client_t client_handle, rdmnet_client_scope_t scope_handle,
                                const RdmnetEptData* data, RdmnetSyncEptResponse* response, void* context)
{
  // After determining that the message requires a response...
  memcpy(my_ept_response_buf, my_response_data, RESPONSE_DATA_SIZE);
  RDMNET_SYNC_SEND_EPT_DATA(response, RESPONSE_DATA_SIZE);
  // Data will be sent after this function returns.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// This buffer was provided as part of the EptClient::Settings when the EptClient instance
// was initialized.
static uint8_t my_ept_response_buf[kMyMaxResponseSize];

rdmnet::EptResponseAction MyEptNotifyHander::HandleEptData(rdmnet::EptClient::Handle client_handle,
                                                           rdmnet::ScopeHandle scope_handle,
                                                           const rdmnet::EptData& data)
{
  std::memcpy(my_ept_response_buf, my_response_data, kResponseDataSize);
  return rdmnet::EptResponseAction::SendData(kResponseDataSize);
}
```
<!-- CODE_BLOCK_END -->

### Responding Asynchronously or Not Responding

If EPT data does not require a response, or if you choose to defer the response to be sent at a
later time, this can be indicated in the data received callback as well. Note that if you want to
save the EPT data you received, it must be copied out (data delivered to callbacks is still owned
by the library). There are RDMnet API functions to save EPT data using heap allocation; you can
also use your own method of saving the data.

<!-- CODE_BLOCK_START -->
```c
void ept_data_received_callback(rdmnet_ept_client_t client_handle, rdmnet_client_scope_t scope_handle,
                                const RdmnetEptData* data, RdmnetSyncEptResponse* response, void* context)
{
  // RdmnetEptData structures do not own their data and the data will be invalid when this callback
  // returns. To save the data for later processing:
  RdmnetSavedEptData saved_data;
  rdmnet_save_ept_data(&data, &saved_data);

  // This structure must then be freed before going out of scope, using rdmnet_free_saved_ept_data().

  // response will be set to take no action (DEFER_RESPONSE) by default, so no action is necessary
  // here.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
rdmnet::EptResponseAction MyEptNotifyHandler::HandleEptData(rdmnet::EptClient::Handle client_handle,
                                                            rdmnet::ScopeHandle scope_handle,
                                                            const rdmnet::EptData& data)
{
  // EptData structures do not own their data and the data will be invalid when this callback
  // returns. To save the data for later processing, you can save the entire structure with its
  // addressing data:
  rdmnet::SavedEptData saved_data = data.Save();

  // Or to just save the opaque data:
  std::vector<uint8_t> saved_data = data.CopyData();

  // Indicate that we will take no response action directly from this callback.
  return rdmnet::EptResponseAction::DeferResponse();
}
```
<!-- CODE_BLOCK_END -->

## Handling EPT Status Messages

If something went wrong while a broker or an EPT client was processing your message, you will get a
response called an "EPT Status". There is a separate callback for handling these messages.

<!-- CODE_BLOCK_START -->
```c
void ept_status_callback(rdmnet_ept_client_t client_handle, rdmnet_client_scope_t scope_handle,
                         const RdmnetEptStatus* status, void* context)
{
  // Check handles and/or context as necessary...

  char cid_str[ETCPAL_UUID_STRING_BYTES];
  etcpal_uuid_to_string(&status->source_cid, cid_str);
  printf("Error sending EPT data to component %s: '%s'\n", cid_str,
         rdmnet_ept_status_code_to_string(status->status_code));
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyEptNotifyHandler::HandleEptStatus(rdmnet::EptClient::Handle client_handle,
                                         rdmnet::ScopeHandle scope_handle,
                                         const rdmnet::EptStatus& status)
{
  // Check handles as necessary...

  std::cout << "Error sending EPT data to component " << status.source_cid().ToString() << ": '"
            << status.CodeToString() << "'\n";
}
```
<!-- CODE_BLOCK_END -->
