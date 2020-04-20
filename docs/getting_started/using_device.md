# Using the Device API                                          {#using_device}

The RDMnet Device API exposes both a C and C++ language interface. The C++
interface is a header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization

The RDMnet library must be globally initialized before using the RDMnet Device API. See
\ref global_init_and_destroy.

To create a device instance, use the rdmnet_device_create() function in C, or instantiate an
rdmnet::Device and call its Startup() function in C++.

The RDMnet device API is an asynchronous, callback-oriented API. Part of the initial configuration
for a device instance is a set of function pointers (or abstract interface) for the library to use
as callbacks. Callbacks are dispatched from the background thread that is started when the RDMnet
library is initialized.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/device.h"

// Sets optional values to defaults. Must pass your ESTA manufacturer ID. If you have not yet
// requested an ESTA manufacturer ID, the range 0x7ff0 to 0x7fff can be used for prototyping.
RdmnetDeviceConfig config = RDMNET_DEVICE_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID_VAL);

// Each device is a component that must have a Component ID (CID), which is simply a UUID. Pure
// redistributable software apps may generate a new CID on each run, but hardware-locked devices
// should use a consistent CID locked to a MAC address (a V3 or V5 UUID).
etcpal_generate_device_uuid("My Device Name", &device_mac_addr, 0, &config.cid);

// Set the callback functions - defined elsewhere
// p_some_opaque_data is an opaque data pointer that will be passed back to each callback function
rdmnet_device_set_callbacks(&config, my_device_connected_cb, my_device_connect_failed_cb,
                            my_device_disconnected_cb, my_device_rdm_command_received_cb,
                            my_device_llrp_rdm_command_received_cb, p_some_opaque_data);

rdmnet_device_t my_device_handle;
etcpal_error_t result = rdmnet_device_create(&config, &my_device_handle);
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
#include "rdmnet/cpp/device.h"

class MyDeviceNotifyHandler : public rdmnet::DeviceNotifyHandler
{
  // Implement the DeviceNotifyHandler callbacks...
};

MyDeviceNotifyHandler my_device_notify_handler;

// Example method for generating a CID for a hardware-locked device:
auto my_cid = etcpal::Uuid::Device("My Device Name", device_mac_addr, 0);

rdmnet::DeviceSettings settings(my_cid, MY_ESTA_MANUFACTURER_ID_VAL);
rdmnet::Device device;

// In this example we are using the convenience method to startup with the default scope. The
// Startup() overloads can be used to specify a scope to start on.
auto result = device.StartupWithDefaultScope(my_device_notify_handler, settings);
if (result)
{
  // Device is valid and running.
}
else
{
  std::cout << "RDMnet device startup failed with error: " << result.ToString() << '\n';
}
```
<!-- CODE_BLOCK_END -->

## Deinitialization

The device should be shut down and destroyed gracefully before program termination. This will send
a graceful disconnect message to any connected broker and deallocate the device's resources.

<!-- CODE_BLOCK_START -->
```c
rdmnet_device_destroy(my_device_handle, kRdmnetDisconnectShutdown);

// At this point, the device is no longer running and its handle is no longer valid.
```
<!-- CODE_BLOCK_MID -->
```cpp
device.Shutdown();

// At this point, the device is no longer running and its handle is no longer valid. It can be
// started again (with a new handle) by calling Startup() again.
```
<!-- CODE_BLOCK_END -->

## Managing Scopes

Devices operate on a single RDMnet scope at a time, which is set at initial configuration time when
the device instance is created, and can be changed using rdmnet_device_change_scope(). See the
"Scopes" section in \ref how_it_works for more information on scopes.

On startup, a device will immediately begin the scope connection state machine from the RDMnet tick
thread. If a static configuration is not provided (using the RDMNET_CLIENT_SET_SCOPE() macro to
initialize the device's RdmnetScopeConfig, or calling rdmnet::Device::Startup() without a
static_broker_addr argument), the first action will be to attempt to discover brokers for this
scope via DNS-SD. Once a broker is found, connection will be initiated automatically, and the
result will be delivered via either the connected or connect_failed callback.

If a broker for a given scope has been configured with a static IP address and port, you can skip
DNS-SD discovery by providing this address to the relevant functions/macros.

You can change the initial scope configuration before the device is started:

<!-- CODE_BLOCK_START -->
```c
RdmnetDeviceConfig config = RDMNET_DEVICE_CONFIG_DEFAULT_INIT(MY_ESTA_MANUFACTURER_ID_VAL);

// Edit the scope information in the configuration struct

RDMNET_CLIENT_SET_SCOPE(&config.scope_config, "My initial scope");

// Or, if configured with a static broker address...
RDMNET_CLIENT_SET_STATIC_SCOPE(&config.scope_config, "My initial scope", static_broker_addr);
```
<!-- CODE_BLOCK_MID -->
```cpp
auto result = device.Startup(my_device_notify_handler, settings, "My initial scope");

// Or, if configured with a static broker address...
auto result = device.Startup(my_device_notify_handler, settings, "My initial scope", my_static_broker_addr);
```
<!-- CODE_BLOCK_END -->

Or change the scope while the device is running. This will cause the device to disconnect from the
old scope and connect to the new one.

<!-- CODE_BLOCK_START -->
```c
RdmnetScopeConfig new_scope_config;
RDMNET_CLIENT_SET_SCOPE(&new_scope_config, "My new scope");

// The disconnect_reason argument provides the RDMnet disconnect reason code to send on the current
// connected scope. Select the most appropriate value for why the scope is changing.
etcpal_error_t result = rdmnet_device_change_scope(my_device_handle, &new_scope_config, kRdmnetDisconnectUserReconfigure);
```
<!-- CODE_BLOCK_MID -->
```cpp
// The disconnect_reason argument provides the RDMnet disconnect reason code to send on the current
// connected scope. Select the most appropriate value for why the scope is changing.
auto result = device.ChangeScope("My new scope", kRdmnetDisconnectUserReconfigure);
```
<!-- CODE_BLOCK_END -->

## Handling Callbacks

The library will dispatch callback notifications from the context in which rdmnet_core_tick() is
called (in the default configuration, this is a single dedicated worker thread). It is safe to call
any RDMnet API function from any callback.

Generally the first callback a device gets will be when it has connected to a broker:

<!-- CODE_BLOCK_START -->
```c
void device_connected_callback(rdmnet_device_t handle, const RdmnetClientConnectedInfo* info, void* context)
{
  char addr_str[ETCPAL_IP_STRING_BYTES];
  etcpal_ip_to_string(&info->broker_addr.ip, addr_str);
  printf("Connected to broker '%s' at address %s:%d\n", info->broker_name, addr_str, info->broker_addr.port);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyDeviceNotifyHandler::HandleConnectedToBroker(rdmnet::DeviceHandle handle, const rdmnet::ClientConnectedInfo& info)
{
  std::cout << "Connected to broker '" << info.broker_name()
            << "' at IP address " << info.broker_addr().ToString() << '\n';
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

## Adding Endpoints and Responders

As explained in \ref devices_and_gateways, RDMnet devices can contain endpoints which serve as
grouping mechanisms for virtual or physical RDM responders. The RDMnet device API has functionality
for managing endpoints and responders on a local device.

### Adding Physical Endpoints and Responders

Physical endpoints are added using the physical endpoint configuration structure. This structure
defines the endpoint number (an integer between 1 and 63,999) and any responders which are present
on the endpoint initially.

<!-- CODE_BLOCK_START -->
```c
// Create a physical endpoint numbered 1, with no responders on it initially.
RdmnetPhysicalEndpointConfig endpoint_config = RDMNET_PHYSICAL_ENDPOINT_INIT(1);

etcpal_error_t result = rdmnet_device_add_physical_endpoint(my_device_handle, &endpoint_config);
if (result == kEtcPalErrOk)
{
  // The endpoint has been added. If currently connected, the library will send the proper
  // notification which indicates to connected controllers that there is a new endpoint present.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// Endpoint configs are constructed implicitly from endpoint numbers, so when creating an endpoint
// with no responders on it initially, it's as simple as: 

auto result = device.AddPhysicalEndpoint(1); // Create a physical endpoint numbered 1
if (result)
{
  // The endpoint has been added. If currently connected, the library will send the proper
  // notification which indicates to connected controllers that there is a new endpoint present.
}
```
<!-- CODE_BLOCK_END -->

Adding and removing physical responders from an endpoint will typically be done when an RDMnet
gateway discovers or loses RDM responders on its RDM ports. For example:

<!-- CODE_BLOCK_START -->
```c
void rdm_responder_discovered(uint16_t port_endpoint_number, const RdmUid* responder_uid)
{
  etcpal_error_t result = rdmnet_device_add_static_responders(my_device_handle, port_endpoint_number,
                                                              responder_uid, 1);
  if (result == kEtcPalErrOk)
  {
    // The responder has been added. If currently connected, the library will send the proper
    // notification which indicates to connected controllers that there is a new responder present.
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void RdmResponderDiscovered(uint16_t port_endpoint_number, const rdm::Uid& responder_uid)
{
  auto result = device.AddPhysicalResponder(port_endpoint_number, responder_uid);
  if (result)
  {
    // The responder has been added. If currently connected, the library will send the proper
    // notification which indicates to connected controllers that there is a new responder present.
  }
}
```
<!-- CODE_BLOCK_END -->

You can also add responders initially present on an endpoint at the same time as the endpoint is
added. This should be done anytime the initial responders on an endpoint are known, as it reduces
the volume of RDMnet messages that need to be sent.

<!-- CODE_BLOCK_START -->
```c
// Create a physical endpoint numbered 2
RdmnetPhysicalEndpointConfig endpoint_config = RDMNET_PHYSICAL_ENDPOINT_INIT(2);

// UIDs for a group of responders that have already been discovered on this endpoint are contained
// in the responders array below.
RdmUid responders[3] = { 
  { 0x6574, 0x00000001 },
  { 0x6574, 0x00000002 },
  { 0x6574, 0x00000003 }
};
endpoint_config.responders = responders;
endpoint_config.num_responders = 3;

// Add the endpoint
etcpal_error_t result = rdmnet_device_add_physical_endpoint(my_device_handle, &endpoint_config);
if (result == kEtcPalErrOk)
{
  // The endpoint has been added. If currently connected, the library will send the proper
  // notification which indicates to connected controllers that there is a new endpoint and new
  // responders present.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// Create a physical endpoint numbered 2 with the UIDs of 3 previously-discovered physical
// responders.
std::vector<rdm::Uid> responders = {
  { 0x6574, 0x00000001 },
  { 0x6574, 0x00000002 },
  { 0x6574, 0x00000003 }
};
rdmnet::PhysicalEndpointConfig endpoint_config(2, responders);

// Add the endpoint
auto result = device.AddPhysicalEndpoint(endpoint_config);
if (result)
{
  // The endpoint has been added. If currently connected, the library will send the proper
  // notification which indicates to connected controllers that there is a new endpoint and new
  // responders present.
}
```
<!-- CODE_BLOCK_END -->

### Adding Virtual Endpoints and Responders

The process for adding virtual endpoints is similar to that for physical endpoints, except that
virtual responders can have Dynamic UIDs (see \ref roles_and_addressing for more information about
this).

<!-- CODE_BLOCK_START -->
```c
// Create a virtual endpoint numbered 3
RdmnetVirtualEndpointConfig endpoint_config = RDMNET_VIRTUAL_ENDPOINT_INIT(3);

// The virtual endpoint has a single responder that needs a dynamic UID. The responder needs a
// permanent Responder ID (RID), which is a UUID that should not change over the lifetime of the
// responder.
EtcPalUuid rid;
etcpal_string_to_uuid("f7c58fe2-d380-4367-91d1-b148eade448d", &rid);
endpoint_config.dynamic_responders = &rid;
endpoint_config.num_dynamic_responders = 1;

// Add the endpoint
etcpal_error_t result = rdmnet_device_add_virtual_endpoint(my_device_handle, &endpoint_config);
if (result == kEtcPalErrOk)
{
  // The endpoint has been added. If currently connected, the library will send the proper
  // notification which indicates to connected controllers that there is a new endpoint present,
  // and request a dynamic UID for the responder which will be delivered to the
  // RdmnetDeviceDynamicUidStatusCallback.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// Create a virtual endpoint numbered 3, which has a single responder that needs a dynamic UID. The
// responder needs a permanent Responder ID (RID), which is a UUID that should not change over the
// lifetime of the responder.
std::vector<etcpal::Uuid> responders = { etcpal::Uuid::FromString("f7c58fe2-d380-4367-91d1-b148eade448d") };
rdmnet::VirtualEndpointConfig endpoint_config(3, responders);

// Add the endpoint
auto result = device.AddVirtualEndpoint(endpoint_config);
if (result)
{
  // The endpoint has been added. If currently connected, the library will send the proper
  // notification which indicates to connected controllers that there is a new endpoint present,
  // and request a dynamic UID for the responder which will be delivered to the
  // rdmnet::DeviceNotifyHandler::HandleDynamicUidStatus() callback.
}
```
<!-- CODE_BLOCK_END -->

Before dynamic virtual responders can be used, they must be assigned a dynamic UID. The status of
dynamic UID assignment is communicated through the dynamic UID status callback.

<!-- CODE_BLOCK_START -->
```c
void handle_dynamic_uid_status(rdmnet_device_t handle, const RdmnetDynamicUidAssignmentList* list, void* context)
{
  // Check handles and/or context as necessary...

  for (const RdmnetDynamicUidMapping* mapping = list->mappings; mapping < list->mappings + list->num_mappings;
       ++mapping)
  {
    if (mapping->status_code == kRdmnetDynamicUidStatusOk)
    {
      // This function is assumed to add the dynamic UID to the virtual responder's runtime data.
      // This UID should be used to determine whether an RDM command received later is addressed to
      // the virtual responder.
      set_responder_dynamic_uid(&mapping->rid, &mapping->uid);
    }
    else
    {
      char cid_str[ETCPAL_UUID_STRING_BYTES];
      etcpal_uuid_to_string(&mapping->cid, cid_str);
      printf("Error obtaining dynamic UID for responder %s: '%s'\n", cid_str,
             rdmnet_dynamic_uid_status_to_string(mapping->status_code));
    }
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyDeviceNotifyHandler::HandleDynamicUidStatus(rdmnet::DeviceHandle handle,
                                                   const rdmnet::DynamicUidAssignmentList& list)
{
  // Check handles as necessary...

  for (const auto& mapping : list.GetMappings())
  {
    if (mapping.IsOk())
    {
      // This function is assumed to add the dynamic UID to the virtual responder's runtime data.
      // This UID should be used to determine whether an RDM command received later is addressed to
      // the virtual responder.
      SetResponderDynamicUid(mapping.rid, mapping.uid);
    }
    else
    {
      std::cout << "Error obtaining dynamic UID for responder " << mapping.cid.ToString() << ": '"
                << mapping.CodeToString() << "'\n";
    }
  }
}
```
<!-- CODE_BLOCK_END -->

One last thing about endpoints: if the set of physical and virtual endpoints that a device has is
known at initialization time, they can be added to the initial configuration structures for a
device instance, so that they don't need to be added later.

## Handling RDM Commands

Handling RDM commands is the main functionality of RDMnet devices. See \ref handling_rdm_commands
for information on how to do this.
