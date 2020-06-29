# Handling RDM Commands                                                    {#handling_rdm_commands}

<!-- LANGUAGE_SELECTOR -->

Handling and responding to RDM commands is the core functionality of RDMnet devices and gateways,
and an optional part of the functionality of RDMnet controllers.

## Interpreting RDM Commands

RDM commands are delivered via a callback that looks like the below:

<!-- CODE_BLOCK_START -->
```c
void rdm_command_received(rdmnet_device_t handle, const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* response,
                          void* context)
{
  if (RDMNET_COMMAND_IS_TO_DEFAULT_RESPONDER(cmd))
  {
    // Handle the command with the default responder.
  }
  else
  {
    // Command is addressed to a non-default virtual or physical RDM responder.
    // Inspect cmd->dest_endpoint and cmd->rdm_header.dest_uid to determine which one.
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
rdmnet::ResponseAction MyRdmnetNotifyHander::HandleRdmCommand(rdmnet::Device::Handle handle,
                                                              const rdmnet::RdmCommand& cmd)
{
  if (cmd.IsToDefaultResponder())
  {
    // Handle the command with the default responder.
  }
  else
  {
    // Command is addressed to a non-default virtual or physical RDM responder.
    // Inspect cmd.dest_endpoint() and cmd.rdm_dest_uid() to determine which one.
  }
}
```
<!-- CODE_BLOCK_END -->

An RDM command in RDMnet can be addressed to the default responder or a sub-responder (see
@ref devices_and_gateways for more information on these concepts). A command to the default
responder addresses the RDM parameters of the device implementing RDMnet. A command to a
sub-responder addresses either a portion of the device's functionality dedicated to handling sACN
(a virtual responder) or a physical RDM responder connected to an RDMnet gateway.

Besides this special RDMnet addressing information, the RDM command structure delivered with the
RDM command handler callback contains data typical to RDM handling logic; the command class,
parameter ID, subdevice ID, and parameter data, if any. The only other RDMnet-specific construct is
the sequence number, which is only used internally by the library.

## Responding to RDM Commands

This RDMnet library allows two different paradigms for responding to RDM commands: synchronous and
asynchronous. The RDMnet Device API will be used as an example, but this also applies to the
Controller API if using the RDM responder callbacks.

By the time this callback is called, the library will already have handled some error conditions
internally. For example, if the received RDMnet RDM command was malformed, or not addressed to the
local RDMnet component, the message will be dropped or an error response will be sent according to
the rules of the RDMnet standard.

### Responding synchronously

If you can access the data needed to respond to the RDM command relatively quickly from the code
path of the notification callback, it's convenient to respond synchronously. The RDM command
notification callbacks provide a way to set the response information directly from the callback,
which the library will use to send a response after the callback returns.

_Important note: Do not do this when accessing response data may block for a significant time._ See
"Responding asynchronously" below for that scenario.

There are two different ways to respond synchronously to an RDM command received over RDMnet:

#### ACK (ACKnowledge)

An ACK response indicates that the action in the command was carried out. ACK responses can contain
data; for example, the responses to most RDM GET commands contain data that was requested by a
controller. When an ACK response contains data, it should be copied into the buffer that was
provided when the RDMnet handle was created, before returning from the callback function. All
notifications for any given RDMnet handle are delivered from the same thread, so accesses to this
buffer from the callback context are thread-safe. 

After copying the data, use the API-specific method to indicate that this is an ACK response as
shown below.

<!-- CODE_BLOCK_START -->
```c
// This buffer was provided as part of the RdmnetDeviceConfig when the device handle was created.
static uint8_t my_rdmnet_response_buf[MY_MAX_RESPONSE_SIZE];

void rdm_command_received(rdmnet_device_t handle, const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* response,
                          void* context)
{
  // Very simplified example...
  if (cmd->rdm_header.command_class == kRdmCCGetCommand && cmd->rdm_header.param_id == E120_DEVICE_INFO)
  {
    memcpy(my_rdmnet_response_buf, kMyDeviceInfoData, DEVICE_INFO_DATA_SIZE);
    RDMNET_SYNC_SEND_RDM_ACK(response, DEVICE_INFO_DATA_SIZE);
    // ACK will be sent after this function returns.
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// This buffer was provided as part of the Device::Settings when the Device instance was
// initialized.
static uint8_t my_rdmnet_response_buf[kMyMaxResponseSize];

rdmnet::RdmResponseAction MyRdmnetNotifyHander::HandleRdmCommand(rdmnet::Device::Handle handle,
                                                                 const rdmnet::RdmCommand& cmd)
{
  // Very simplified example...
  if (cmd.IsGet() && cmd.param_id() == E120_DEVICE_INFO)
  {
    std::memcpy(my_rdmnet_response_buf, kMyDeviceInfoData, kDeviceInfoDataSize);
    return rdmnet::RdmResponseAction::SendAck(kDeviceInfoDataSize);
  }
}
```
<!-- CODE_BLOCK_END -->

Most SET commands don't require data in their ACK response; the response is just an acknowledgment
that the changed parameter was acted upon. In this case, there's no need to copy any data before
responding with an ACK.

<!-- CODE_BLOCK_START -->
```c
void rdm_command_received(rdmnet_device_t handle, const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* response,
                          void* context)
{
  // Very simplified example...
  if (cmd->rdm_header.command_class == kRdmCCSetCommand && cmd->rdm_header.param_id == E120_IDENTIFY_DEVICE)
  {
    if (cmd->data[0] == 0)
    {
      // Stop the physical identify operation
    }
    else
    {
      // Start the physical identify operation
    }
    // Indicate an ACK should be sent with no data.
    RDMNET_SYNC_SEND_RDM_ACK(response, 0);
    // ACK will be sent after this function returns.
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
rdmnet::RdmResponseAction MyRdmnetNotifyHander::HandleRdmCommand(rdmnet::Device::Handle handle,
                                                                 const rdmnet::RdmCommand& cmd)
{
  // Very simplified example...
  if (cmd.IsSet() && cmd.param_id() == E120_IDENTIFY_DEVICE)
  {
    if (cmd.data()[0] == 0)
    {
      // Stop the physical identify operation
    }
    else
    {
      // Start the physical identify operation
    }
    // Indicate an ACK should be sent with no data.
    return rdmnet::RdmResponseAction::SendAck();
  }
}
```
<!-- CODE_BLOCK_END -->

#### NACK (Negative ACKnowledge)

A NACK response indicates that the RDMnet command was valid, but the RDM transaction cannot be
acted upon for some reason. NACK responses contain a reason code that indicates the reason that the
command cannot be acted upon. Common reasons to NACK an RDMnet RDM command include:

* The given RDM Parameter ID (PID) is not supported by this responder (@ref kRdmNRUnknownPid)
* The PID is supported, but the requested command class is not (@ref kRdmNRUnsupportedCommandClass)
  + Example: a SET was requested on a read-only parameter
* Some data present in the command contains an index that is out of range (@ref kRdmNRDataOutOfRange)
  + Example: a device has 3 sensors and a GET:SENSOR_VALUE command was received for sensor number 4

To respond with a NACK, use the API-specific method to indicate that this is a NACK response and
provide the reason as shown below.

<!-- CODE_BLOCK_START -->
```c
// Called from rdm_command_received()
void handle_get_sensor_value(rdmnet_device_t handle, const uint8_t* data, RdmnetSyncRdmResponse* response)
{
  uint8_t sensor_num = data[0];
  if (sensor_num >= NUM_SENSORS)
  {
    RDMNET_SYNC_SEND_RDM_NACK(response, kRdmNRDataOutOfRange);
    // NACK will be sent after the RDM command handler callback returns.
  }
  else
  {
    // Process data and send ack...
  }
}
```
<!-- CODE_BLOCK_MID -->
```cpp
// Called from MyRdmnetNotifyHandler::HandleRdmCommand()
rdmnet::RdmResponseAction MyRdmnetNotifyHander::HandleGetSensorValue(const uint8_t* data)
{
  uint8_t sensor_num = data[0];
  if (sensor_num >= NUM_SENSORS)
  {
    return rdmnet::RdmResponseAction::SendNack(kRdmNRDataOutOfRange);
  }
}
```
<!-- CODE_BLOCK_END -->

### Responding asynchronously

If you choose to defer the response to an RDM command to be sent at a later time, you _must_ ensure
that every RDM command gets a response eventually (one of ACK, NACK or RPT Status).

#### RPT Status

An RPT Status response is an RDMnet protocol level response to an RDM command. Most RPT Status
messages (except @ref kRptStatusBroadcastComplete) indicate that something has gone wrong at the
RDMnet protocol level and the request cannot be handled. RPT Status responses include a code that
indicates the reason for the status message.

Most conditions that would require sending an RPT Status are handled at the library level. The ones
that must be handled by the application relate exclusively to the operation of RDMnet Gateways.
RPT Status codes that need to be handled by a gateway application include:

* A gateway encountered an RDM timeout while attempting to forward an RDM command to a responder (@ref kRptStatusRdmTimeout)
* A gateway received an invalid RDM response from a responder (@ref kRptStatusInvalidRdmResponse)
* A gateway has finished transmitting an RDM broadcast message on a port (@ref kRptStatusBroadcastComplete)

#### Saving Commands

The RDM command structures delivered to callbacks by the RDMnet library reference data that is
kept in buffers owned by the library, and will be reused for other data when that callback returns.
In order to defer responses to RDM commands to be sent later, it's necessary to save the command
data to be sent along with its corresponding response.

RDMnet provides APIs for saving the command into different structs and/or classes which contain
data buffers to hold the RDM parameter data.

<!-- CODE_BLOCK_START -->
```c
void rdm_command_received(rdmnet_device_t handle, const RdmnetRdmCommand* cmd, RdmnetSyncRdmResponse* response,
                          void* context)
{ 
  // Note that this type contains a byte array large enough for the largest possible RDM command
  // data buffer, e.g. 231 bytes.
  RdmnetSavedRdmCommand saved_cmd;
  rdmnet_save_rdm_command(cmd, &saved_cmd);

  // Add saved_cmd to some memory location to be used later...

  RDMNET_SYNC_DEFER_RDM_RESPONSE(response);
}

// Later, when the response is available...
rdmnet_device_send_rdm_ack(my_device_handle, &saved_cmd, response_data, response_data_len);
```
<!-- CODE_BLOCK_MID -->
```cpp
rdmnet::RdmResponseAction MyRdmnetNotifyHander::HandleRdmCommand(rdmnet::Device::Handle handle,
                                                                 const rdmnet::RdmCommand& cmd)
{
  // Note that this type contains a byte array large enough for the largest possible RDM command
  // data buffer, e.g. 231 bytes.
  rdmnet::SavedRdmCommand saved_cmd = cmd.Save();

  // Add saved_cmd to some memory location to be used later...

  return rdmnet::RdmResponseAction::DeferResponse();
}

// Later, when the response is available...
my_device.SendRdmAck(saved_cmd, response_data, response_data_len);
```
<!-- CODE_BLOCK_END -->

## PIDs handled by the library

The RDMnet library has all the information necessary to handle certain RDMnet-related RDM commands
internally with no intervention by the application. The list of PIDs fully handled by the RDMnet
library are:

* ANSI E1.37-7
  + ENDPOINT_LIST
  + ENDPOINT_LIST_CHANGE
  + ENDPOINT_RESPONDERS
  + ENDPOINT_RESPONDER_LIST_CHANGE
  + BINDING_CONTROL_FIELDS
* ANSI E1.33
  + TCP_COMMS_STATUS
  + BROKER_STATUS
    - Will be handled according to the policies given in the broker's settings configuration.

There is no need to include these PID values in your response to the GET:SUPPORTED_PARAMETERS
command; they will be automatically added by the library before sending the response.

Note that the RDMnet standard requires support for additional PIDs that are not handled by the
library. If you are using the Controller or Device API, you must support these PIDs in your
implementation (or configure the Controller API with RDM data; see @ref using_controller).

* ANSI E1.20
  + IDENTIFY_DEVICE
  + SUPPORTED_PARAMETERS
  + PARAMETER_DESCRIPTION
  + MANUFACTURER_LABEL
  + DEVICE_MODEL_DESCRIPTION
  + SOFTWARE_VERSION_LABEL
  + DEVICE_LABEL
* ANSI E1.33
  + COMPONENT_SCOPE
  + SEARCH_DOMAIN
