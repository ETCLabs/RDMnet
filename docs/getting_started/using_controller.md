# Using the Controller API                                  {#using_controller}

The RDMnet Controller API exposes both a C and C++ language interface. The C++
interface is a header-only wrapper around the C interface.

## Initialization and Destruction

The controller init and deinit functions should be called once each at
application startup and shutdown time. These functions interface with the
\ref etcpal_log API to configure what happens when the library logs messages.
Optionally pass an EtcPalLogParams structure to use this functionality. This
structure can be shared across different ETC library modules.

The init function has the side-effect of initializing the core RDMnet library,
which by default starts a background thread for handling periodic RDMnet
functionality and receiving data (this behavior can be overridden at
compile-time if an app wants more control over its threading, see
#RDMNET_USE_TICK_THREAD and rdmnet_core_tick()). The deinit function joins this
thread.

```c
#include "rdmnet/controller.h"

// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...
etcpal_error_t init_result = rdmnet_controller_init(&log_params);

// In some function called at shutdown...
rdmnet_controller_deinit();
```

```cpp
#include "rdmnet/cpp/controller.h"

// In some function called at startup...
EtcPalLogParams log_params;
// Initialize log_params...
etcpal::Result init_result = rdmnet::Controller::Init(&log_params);

// In some function called at shutdown...
rdmnet::Controller::Deinit();
```

To create a controller instance, use the rdmnet_controller_create() function.
Most apps will only need a single controller instance. One controller can
monitor an arbitrary number of RDMnet scopes at once.

The RDMnet controller API is an asynchronous, callback-oriented API. On
creation, you give the library a configuration struct containing, among other
things, a set of function pointers to use as callbacks. Callbacks are
dispatched from the thread context which calls rdmnet_core_tick().

```c
RdmnetControllerConfig config;

// Sets optional values to defaults. Must pass your ESTA manufacturer ID. If
// you have not yet requested an ESTA manufacturer ID, the range 0x7ff0 to
// 0x7fff can be used for prototyping.
RDMNET_CONTROLLER_CONFIG_INIT(&config, MY_ESTA_MANUFACTURER_ID_VAL);

// Each controller is a component that must have a Component ID (CID), which is
// simply a UUID. Pure redistributable software apps may generate a new CID on
// each run, but hardware-locked devices should use a consistent CID locked to
// a MAC address (a V3 or V5 UUID).
etcpal_generate_os_preferred_uuid(&config.cid);

// Set the callback functions - defined elsewhere
config.callbacks.connected = my_controller_connected_cb;
config.callbacks.disconnected = my_controller_disconnected_cb;
config.callbacks.client_list_update = my_controller_client_list_update_cb;
config.callbacks.rdm_response_received = my_controller_rdm_response_received_cb;
config.callbacks.rdm_command_received = my_controller_rdm_command_received_cb;
config.callbacks.status_received = my_controller_status_received_cb;
config.callbacks.llrp_rdm_command_received = my_controller_llrp_command_received_cb;

// An opaque data pointer that will be passed back to each callback function
config.callback_context = p_some_opaque_data;

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

In C++, instantiate an `rdmnet::Controller` instance and call its `Startup()`
function.

```cpp
// MyControllerNotify derives from rdmnet::ControllerNotify
MyControllerNotify my_controller_notify;

rdmnet::Controller controller;
etcpal::Result result = controller.Startup(etcpal::Uuid::OsPreferred(), my_controller_notify);
if (result)
{
  // Controller is valid and running.
}
else
{
  // Startup failed, use result.code() or result.ToString() to inspect details
}
```

### Managing Scopes

A controller instance is initially created without any configured scopes. If
the app has not been reconfigured by a user, the E1.33 RDMnet standard requires
that the RDMnet default scope be configured automatically. There is a shortcut
function for this, rdmnet_controller_add_default_scope().

Otherwise, use rdmnet_controller_add_scope() to add a custom configured scope. 

Per the requirements of RDMnet, a scope string is always UTF-8 and is thus represented by a char[]
in C and a std::string in C++.

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

Or, in C++:

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

### Dynamic vs Static Scopes

Adding a scope will immediately begin the scope connection state machine from the RDMnet tick
thread. If a static configuration is not provided (using the RDMNET_CLIENT_SET_SCOPE() macro to
initialize an RdmnetScopeConfig, or calling rdmnet::Controller::AddScope() with only one argument)
the first action will be to attempt to discover brokers for this scope using DNS-SD. Once a broker
is found, connection will be initiated automatically, and the result will be delivered via either
the connected or connect_failed callback.

If a broker for a given scope has been configured with a static IP address and port, you can skip
DNS-SD discovery by providing a static configuration:

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
etcpal_error_t result = rdmnet_controller_add_scope(my_controller_handle, &config, &scope_handle);
```

Or, in C++:

```cpp
// Get configured static broker address
etcpal::SockAddr static_broker_addr = etcpal::SockAddr::FromString("192.168.2.1:8000");
auto add_res = controller.AddScope("my_custom_scope", static_broker_addr);
// Or:
auto add_res = controller.AddDefaultScope(static_broker_addr);
```

### Handling Callbacks

The library will dispatch callback notifications from the context in which rdmnet_core_tick() is
called (in the default configuration, this is a single dedicated worker thread). It is safe to call
any RDMnet API function from any callback; in fact, this is the recommended way of handling many
callbacks.

For example, a very common controller behavior will be to fetch a client list from the broker
after a successful connection:

```c
void controller_connected_callback(rdmnet_controller_t handle, rdmnet_client_scope_t scope_handle,
                                   const RdmnetClientConnectedInfo* info, void* context)
{
  // Check handle and/or context as necessary...
  rdmnet_controller_request_client_list(handle, scope_handle);
}                                   
```

```cpp
void MyControllerNotifyHandler::HandleConnectedToBroker(ScopeHandle scope, const RdmnetClientConnectedInfo& info)
{
  // Check handle as necessary...
  controller.RequestClientList(scope);
}
```

#### Connection Failure

It's worth noting connection failure as a special case here. RDMnet connections can fail for many
reasons, including user misconfiguration, network misconfiguration, components starting up or
shutting down, programming errors, and more. There is 

The RdmnetClientConnectFailedInfo and RdmnetClientDisconnectedInfo structs passed back with the 
connect_failed and disconnected callbacks respectively have comprehensive information about the 
failure, including enum values containing the library's categorization of the failure, protocol
reason codes and socket errors where applicable. This information is typically used mostly for
logging and debugging. Each of these codes has a respective to_string() function to aid in logging.

For programmatic use, the structs also contain a will_retry member which indicates whether the
library plans to retry this connection in the background. The only circumstances under which the 
library will not retry is when a connection failure is determined to be either a programming error
or a user misconfiguration. Some examples of these circumstances are:

* The broker explicitly rejected a connection with a reason code indicating a configuration error,
  such as CONNECT_SCOPE_MISMATCH or CONNECT_DUPLICATE_UID.
* The library failed to create a network socket before the connection was initiated.

### Sending RDM Commands

Build RDM commands using the LocalRdmCommand type. 

### Handling RDM Responses

