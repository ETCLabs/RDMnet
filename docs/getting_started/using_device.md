# Using the Device API                                          {#using_device}

The RDMnet Device API exposes both a C and C++ language interface. The C++
interface is a header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The device init and deinit functions should be called once each at application startup and shutdown
time. These functions interface with the EtcPal \ref etcpal_log API to configure what happens when
the library logs messages. Optionally pass an EtcPalLogParams structure to use this functionality.
This structure can be shared across different ETC library modules.

The init function has the side-effect of initializing the core RDMnet library, which by default
starts a background thread for handling periodic RDMnet functionality and receiving data (this
behavior can be overridden at compile-time if an app wants more control over its threading, see
#RDMNET_USE_TICK_THREAD and rdmnet_core_tick()). The deinit function joins this thread.

<!-- CODE_BLOCK_START -->
```c
#include "rdmnet/device.h"

// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...

etcpal_error_t init_result = rdmnet_device_init(&log_params);
// Or, to init without worrying about logs from the RDMnet library...
etcpal_error_t init_result = rdmnet_device_init(NULL);

// In some function called at shutdown...
rdmnet_device_deinit();
```
<!-- CODE_BLOCK_MID -->
```cpp
#include "rdmnet/cpp/device.h"

// In some function called at startup...
etcpal::Logger logger;
// Initialize and start logger...

etcpal::Error init_result = rdmnet::Device::Init(logger);

// Or, to init without worrying about logs from the RDMnet library...
etcpal::Error init_result = rdmnet::Device::Init();

// In some function called at shutdown...
rdmnet::Device::Deinit();
```
<!-- CODE_BLOCK_END -->

To create a device instance, use the rdmnet_device_create() function in C, or instantiate an
rdmnet::Device and call its Startup() function in C++.

The RDMnet device API is an asynchronous, callback-oriented API. Part of the initial configuration
for a device instance is a set of function pointers (or abstract interface) for the library to use
as callbacks. Callbacks are dispatched from the thread context which calls rdmnet_core_tick().

<!-- CODE_BLOCK_START -->
```c
RdmnetDeviceConfig config;

// Sets optional values to defaults. Must pass your ESTA manufacturer ID. If you have not yet
// requested an ESTA manufacturer ID, the range 0x7ff0 to 0x7fff can be used for prototyping.
rdmnet_device_config_init(&config, MY_ESTA_MANUFACTURER_ID_VAL);

// Each device is a component that must have a Component ID (CID), which is simply a UUID. Pure
// redistributable software apps may generate a new CID on each run, but hardware-locked devices
// should use a consistent CID locked to a MAC address (a V3 or V5 UUID).
etcpal_generate_device_uuid("My Device Name", &device_mac_addr, 0, &config.cid);

// Set the callback functions - defined elsewhere
// p_some_opaque_data is an opaque data pointer that will be passed back to each callback function
RDMNET_DEVICE_SET_CALLBACKS(&config, my_device_connected_cb, my_device_connect_failed_cb,
                            my_device_disconnected_cb, my_device_rdm_command_received_cb,
                            my_device_llrp_rdm_command_received_cb, p_some_opaque_data);

// Set the RDMnet scope that the device is operating on initially. In this example we are just
// using the default scope.
RDMNET_CLIENT_SET_DEFAULT_SCOPE(&config.scope_config);

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

// Recommended way of generating my_cid for a hardware-locked device:
// auto my_cid = etcpal::Uuid::Device("My Device Name", device_mac_addr, 0);
rdmnet::DeviceData device_data(my_cid, rdm::Uid::BrokerDynamicUidRequest(MY_ESTA_MANUFACTURER_ID_VAL));
rdmnet::Device device;

// In this example we are using the convenience method to startup with the default scope. The
// Startup() overloads can be used to specify a scope to start on.
etcpal::Error result = device.StartupWithDefaultScope(my_device_notify_handler, device_data);
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

Devices operate on a single RDMnet scope at a time, which is set at initial configuration time when
the device instance is created, and can be changed using rdmnet_device_change_scope(). See the
"Scopes" section in \ref how_it_works for more information on scopes.
