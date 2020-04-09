# Using the Device API                                          {#using_device}

The RDMnet Device API exposes both a C and C++ language interface. The C++
interface is a header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The core RDMnet init and deinit functions should be called once each at application startup and
shutdown time. These functions interface with the EtcPal \ref etcpal_log API to configure what
happens when the library logs messages. Optionally pass an EtcPalLogParams structure to use this 
functionality. This structure can be shared across different ETC library modules.

The init function by default starts a background thread for handling periodic RDMnet functionality
and receiving data. The deinit function joins this thread.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/device.h"

// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...

etcpal_error_t init_result = rdmnet_init(&log_params, NULL);
// Or, to init without worrying about logs from the RDMnet library...
etcpal_error_t init_result = rdmnet_init(NULL, NULL);

// In some function called at shutdown...
rdmnet_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "rdmnet/cpp/device.h"

// In some function called at startup...
etcpal::Logger logger;
// Initialize and start logger...

etcpal::Error init_result = rdmnet::Init(logger);

// Or, to init without worrying about logs from the RDMnet library...
etcpal::Error init_result = rdmnet::Init();

// In some function called at shutdown...
rdmnet::Deinit();
```
<!-- CODE_BLOCK_END -->

To create a device instance, use the rdmnet_device_create() function in C, or instantiate an
rdmnet::Device and call its Startup() function in C++.

The RDMnet device API is an asynchronous, callback-oriented API. Part of the initial configuration
for a device instance is a set of function pointers (or abstract interface) for the library to use
as callbacks. Callbacks are dispatched from the thread context which calls rdmnet_core_tick().

<!-- CODE_BLOCK_START -->
```c
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
  // Startup failed, use result.code() or result.ToString() to inspect details
}
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
  // Check handle and/or context as necessary...

  char addr_str[ETCPAL_INET6_ADDRSTRLEN];
  etcpal_inet_ntop(&info->broker_addr.ip, addr_str, ETCPAL_INET6_ADDRSTRLEN);

  printf("Connected to broker '%s' at address %s:%d\n", info->broker_name, addr_str, info->broker_addr.port);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyDeviceNotifyHandler::HandleConnectedToBroker(rdmnet::DeviceHandle handle, const RdmnetClientConnectedInfo& info)
{
  // Check handle as necessary and get device instance... 

}
```
<!-- CODE_BLOCK_END -->
