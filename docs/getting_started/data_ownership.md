# Data Ownership Paradigms in the RDMnet Library                                  {#data_ownership}

RDMnet APIs are _non-ownership-transferring_; this means that when calling API functions that take
pointers to data buffers as arguments, the data buffer memory is owned by the caller, and the
caller is responsible for managing that memory both before and after the API function call.

For example, consider sending an RDM command with parameter data using the RDMnet controller API:

<!-- CODE_BLOCK_START -->
```c
#include <stdlib.h>
#include <stdint.h>
#include "rdmnet/controller.h"

uint8_t* device_label_data = (uint8_t*)malloc(32);
strcpy(device_label_data, "New Device Label");

RdmnetDestinationAddr dest = RDMNET_ADDR_TO_DEFAULT_RESPONDER(0x6574, 0x12345678);
uint32_t cmd_seq_num;
etcpal_error_t result = rdmnet_controller_send_set_command(my_controller_handle, my_scope_handle, &dest,
                                                           E120_DEVICE_LABEL, device_label_data,
                                                           strlen(device_label_data), &cmd_seq_num);

// The library uses and is finished with the device_label_data buffer by the time
// rdmnet_controller_send_set_command() returns. Now we are responsible for freeing the memory.
free(device_label_data);

// Of course, this would not be necessary if we used a static buffer or a buffer on the stack, e.g.:
uint8_t device_label_data[32];
```
<!-- CODE_BLOCK_MID -->
```cpp
#include <string>
#include "rdmnet/cpp/controller.h"

std::string device_label = "New Device Label";

auto dest_addr = rdmnet::DestinationAddr::ToDefaultResponder(0x6574, 0x12345678);
etcpal::Expected<uint32_t> result = controller.SendSetCommand(my_scope_handle, dest_addr, E120_DEVICE_LABEL,
                                                              reinterpret_cast<const uint8_t*>(device_label.c_str()),
                                                              device_label.size());

// The library uses and is finished with the device_label string by the time SendSetCommand()
// returns. The memory associated with the string will then be freed when the string goes out of
// scope.
```
<!-- CODE_BLOCK_END -->

## Callbacks

The same non-ownership-transferring principle applies to callbacks delivered from the RDMnet
library. The library retains ownership of any data buffers supplied to a callback; if a library
user wants to access that data after the callback returns, it must be saved.

The API provides convenient saving functions to make copies of data buffers present in commands.
Of course, data can also be saved manually from the data members.

<!-- CODE_BLOCK_START -->
```c
void controller_rdm_response_callback(rdmnet_controller_t controller_handle, rdmnet_client_scope_t scope_handle,
                                      const RdmnetRdmResponse* resp, void* context)
{
  // resp->rdm_data and resp->original_cmd_data are pointers to data buffers owned by the library.
  // These buffers will be invalid once this callback finishes.

  // You can copy out data manually:
  memcpy(my_data_buf, resp->rdm_data, resp->rdm_data_len);

  // Or using the save function to create a version with owned data:
  RdmnetSavedRdmResponse saved_resp;
  rdmnet_save_rdm_response(resp, &saved_resp);

  // If you use this method, you must then free the saved_resp data when you're done with it:
  rdmnet_free_saved_rdm_response(&saved_resp);
}
```
<!-- CODE_BLOCK_MID -->
```cpp
void MyControllerNotifyHandler::HandleRdmResponse(rdmnet::Controller::Handle controller_handle,
                                                  rdmnet::ScopeHandle scope_handle,
                                                  const rdmnet::RdmResponse& resp)
{
  // resp.data() and resp.original_cmd_data() return pointers to data buffers owned by the library.
  // These buffers will be invalid once this callback finishes.

  // You can copy out the data manually:
  std::memcpy(my_data_buf, resp.data(), resp.data_len());

  // Or using the data copy accessor:
  std::vector<uint8_t> saved_data = resp.GetData();

  // Or using the Save() function to create a version with owned data:
  rdmnet::SavedRdmResponse saved_resp = resp.Save();

  // The SavedRdmResponse class has allocating containers to hold the data - it will be cleaned up
  // when it goes out of scope.
}
```
<!-- CODE_BLOCK_END -->
