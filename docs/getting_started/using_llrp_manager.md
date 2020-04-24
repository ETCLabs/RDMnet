# Using the LLRP Manager API                                                  {#using_llrp_manager}

The LLRP Manager API exposes both a C and C++ language interface. The C++ interface is a
header-only wrapper around the C interface.

<!-- LANGUAGE_SELECTOR -->

## Initialization

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
#include "rdmnet/llrp_manager.h"

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
#include "rdmnet/cpp/llrp_manager.h"

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
  llrp::ManagerHandle handle = manager.handle();
  // Store handle for later lookup from the ManagerNotifyHandler callback functions.
}
else
{
  // Startup failed, use result.code() or result.ToString() to inspect details
}
```
<!-- CODE_BLOCK_END -->

## Deinitialization

The LLRP manager should be shut down and destroyed gracefully before program termination. This will
deallocate the manager's resources.

<!-- CODE_BLOCK_START -->
```c
llrp_manager_destroy(my_manager_handle);

// At this point, the manager is no longer running and its handle is no longer valid.
```
<!-- CODE_BLOCK_MID -->
```cpp
manager.Shutdown();

// At this point, the manager is no longer running and its handle is no longer valid. It can be
// started again (with a new handle) by calling Startup() again.
```
<!-- CODE_BLOCK_END -->

## Discovering Targets

Start the LLRP discovery process using the functions in the snippet examples below. LLRP discovery
continues in the background and typically takes a few seconds, but could take up to a minute or two
in systems with large numbers of LLRP targets present.

LLRP has a filter bitfield with some options that can be used to limit the types of LLRP targets
that respond to discovery.

<!-- CODE_BLOCK_START -->
```c
// To discover all LLRP targets...
etcpal_error_t result = llrp_manager_start_discovery(my_manager_handle, 0);

// Or, to limit discovery to targets that are not currently connected to a broker via RDMnet...
etcpal_error_t result = llrp_manager_start_discovery(my_manager_handle, LLRP_FILTERVAL_CLIENT_CONN_INACTIVE);

// Or, to limit discovery to RDMnet brokers...
etcpal_error_t result = llrp_manager_start_discovery(my_manager_handle, LLRP_FILTERVAL_BROKERS_ONLY);

if (result == kEtcPalErrOk)
{
  // Discovery is now ongoing and the associated callbacks will indicate when targets are discovered
  // and when discovery is finished.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// To discover all LLRP targets...
etcpal::Error result = manager.StartDiscovery();

// Or, to limit discovery to targets that are not currently connected to a broker via RDMnet...
etcpal::Error result = manager.StartDiscovery(LLRP_FILTERVAL_CLIENT_CONN_INACTIVE);

// Or, to limit discovery to RDMnet brokers...
etcpal::Error result = manager.StartDiscovery(LLRP_FILTERVAL_BROKERS_ONLY);

if (result)
{
  // Discovery is now ongoing and the associated callbacks will indicate when targets are discovered
  // and when discovery is finished.
}
```
<!-- CODE_BLOCK_END -->

During discovery, callbacks will indicate when new LLRP targets are discovered and when discovery
has finished. LLRP discovery finishes automatically after a timeout, but you can also call the
relevant API function at any time to stop discovery early.

<!-- CODE_BLOCK_START -->
```c
void handle_llrp_target_discovered(llrp_manager_t handle, const LlrpDiscoveredTarget* target, void* context)
{
  // Check handle and/or context as necessary...

  char uid_str[RDM_UID_STRING_BYTES];
  rdm_uid_to_string(&target->uid, uid_str);
  printf("Discovered new LLRP target with UID %s, target type %s\n", uid_str,
         llrp_component_type_to_string(target->component_type));
}

void handle_llrp_discovery_finished(llrp_manager_t handle, void* context)
{
  // Check handle and/or context as necessary...

  printf("LLRP discovery finished.\n");
}
```
<!-- CODE_BLOCK_MID -->
```cpp
namespace llrp = rdmnet::llrp;

void MyLlrpNotifyHandler::HandleLlrpTargetDiscovered(llrp::ManagerHandle handle, const llrp::DiscoveredTarget& target)
{
  // Check handle as necessary...

  std::cout << "Discovered new LLRP target with UID " << target.uid.ToString()
            << ", target type " << target.ComponentTypeToString() << '\n';
}

void MyLlrpNotifyHandler::HandleLlrpDiscoveryFinished(llrp::ManagerHandle handle)
{
  // Check handle as necessary...

  std::cout << "LLRP discovery finished.\n";
}
```
<!-- CODE_BLOCK_END -->

## Sending RDM Commands

Once one or more LLRP targets have been discovered, an LLRP manager can send RDM commands addressed
to them. LLRP targets must be tracked and addressed by a combination of RDM UID and CID; see
\ref roles_and_addressing and \ref llrp for more information on this. The DiscoveredTarget
structure contains both of these addressing identifiers for each discovered target, and the
DestinationAddr structure takes them to form an LLRP destination address.

After sending a command, the library will provide a _sequence number_ that will be echoed in the
corresponding RDM response. This sequence number should be saved.

<!-- CODE_BLOCK_START -->
```c
// Assuming we have an LlrpDiscoveredTarget structure named 'target'...

LlrpDestinationAddr dest = { target.cid, target.uid, 0 };

// Send a GET:DEVICE_LABEL command to an LLRP target
uint32_t cmd_seq_num;
etcpal_error_t result = llrp_manager_send_get_command(my_manager_handle, &dest, E120_DEVICE_LABEL, NULL, 0,
                                                      &cmd_seq_num);
if (result == kEtcPalErrOk)
{
  // cmd_seq_num identifies this command transaction. Store it for when a response is received.
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// Send a GET:DEVICE_LABEL command to an LLRP target. Assuming we have an llrp::DiscoveredTarget
// structure named 'target'...
etcpal::Expected<uint32_t> result = manager.SendGetCommand(target.address(), E120_DEVICE_LABEL);
if (result)
{
  // *result identifies this command transaction. Store it for when a response is received.
}
```
<!-- CODE_BLOCK_END -->

## Handling RDM Responses

Responses to commands you send will be delivered asynchronously through the "RDM response" callback.

<!-- CODE_BLOCK_START -->
```c
void handle_llrp_rdm_response(llrp_manager_t handle, const LlrpRdmResponse* resp, void* context)
{
  // Check handle and/or context as necessary...

  // Check the response's sequence number against the one(s) you stored earlier.
  if (resp->seq_num == cmd_seq_num)
  {
    handle_response_data(resp->rdm_header.param_id, resp->rdm_data, resp->rdm_data_len);

    // LlrpRdmResponse structures do not own their data and the data will be invalid when this callback
    // ends. To save the data for later processing:
    LlrpSavedRdmResponse saved_resp;
    rdmnet_save_llrp_rdm_response(resp, &saved_resp);
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
namespace llrp = rdmnet::llrp;

void MyLlrpNotifyHandler::HandleLlrpRdmResponse(llrp::ManagerHandle handle, const llrp::RdmResponse& resp)
{
  // Check handle as necessary...

  // Check the response's sequence number against the one(s) you stored earlier.
  if (resp.seq_num() == cmd_seq_num)
  {
    HandleResponseData(resp.param_id(), resp.data(), resp.data_len());

    // llrp::RdmResponse classes do not own their data and the data will be invalid when this
    // callback ends. To save the data for later processing:
    llrp::SavedRdmResponse saved_resp = resp.Save();
  }
}
```
<!-- CODE_BLOCK_END -->
