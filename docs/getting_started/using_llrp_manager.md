# Using the LLRP Manager API                                                  {#using_llrp_manager}

The LLRP Manager API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization and Destruction

The RDMnet library must be globally initialized before using the LLRP Manager API. See
\ref global_init_and_destroy.

To create an LLRP manager instance, use the llrp_manager_create() function in C, or instantiate an
rdmnet::LlrpManager and call its Startup() function in C++.

The LLRP manager API is an asynchronous, callback-oriented API. Part of the initial configuration
for an LLRP manager instance is a set of function pointers (or abstract interface) for the library
to use as callbacks. Callbacks are dispatched from the background thread that is started when the
RDMnet library is initialized.

LLRP managers operate on a single network interface at a time. Network interfaces are tracked by
OS-specific index (see the EtcPal doc page on \ref interface_indexes).

<!-- CODE_BLOCK_START -->
```c
// Sets optional values to defaults. 
LlrpManagerConfig config = LLRP_MANAGER_CONFIG_DEFAULT_INIT;

// Each LLRP manager is a component that must have a Component ID (CID), which is simply a UUID.
// Software should generate and save a CID so that the same one is used on each run of the software.
etcpal_generate_os_preferred_uuid(&config.cid);

// Specify the network interface and IP protocol that this manager should operate on.
config.netint.index = manager_netint_index;
config.netint.ip_type = kEtcPalIpTypeV4;

// Provide your ESTA manufacturer ID. If you have not yet requested an ESTA manufacturer ID, the
// range 0x7ff0 to 0x7fff can be used for prototyping.
config.manu_id = MY_ESTA_MANUFACTURER_ID_VAL;

// Set the callback functions - defined elsewhere
// p_some_opaque_data is an opaque data pointer that will be passed back to each callback function
llrp_manager_config_set_callbacks(&config, my_llrp_target_discovered_cb, my_llrp_rdm_response_received_cb,
                                  my_llrp_discovery_finished_cb, p_some_opaque_data);


llrp_manager_t my_manager_handle;
etcpal_error_t result = llrp_manager_create(&config, &my_manager_handle);
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
// A convenient construct to avoid having to write the entire rdmnet::llrp:: namespace prefix
// before LLRP API calls.
namespace llrp = rdmnet::llrp;

class MyLlrpNotifyHandler : public llrp::ManagerNotifyHandler
{
  // Implement the ManagerNotifyHandler callbacks...
};

MyLlrpNotifyHandler my_llrp_notify_handler;

llrp::Manager manager;
etcpal::Error result = manager.Startup(
    my_llrp_notify_handler,      // Class instance to handle callbacks
    MY_ESTA_MANUFACTURER_ID_VAL, // Your ESTA manufacturer ID
    manager_netint_index,        // The network interface index to operate on
    kEtcPalIpTypeV4              // The IP protocol type (V4 or V6) to use on the network interface
);
if (result)
{
  // Manager is valid and running.
  auto handle = manager.handle();
  // Store handle for later lookup from the ManagerNotifyHandler callback functions.
}
else
{
  // Startup failed, use result.code() or result.ToString() to inspect details
}
```
<!-- CODE_BLOCK_END -->

## Discovering Targets

## Sending RDM Commands

## Handling RDM Responses
