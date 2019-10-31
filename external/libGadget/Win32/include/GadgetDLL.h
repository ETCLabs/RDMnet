// Copyright (c) 2019 Electronic Theatre Controls, Inc., http://www.etcconnect.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef GADGETDLL_H_
#define GADGETDLL_H_

#include <stdint.h>
#include "RDM_CmdC.h"
#include "RDMDeviceInfo.h"

#ifdef GADGET_DLL_EXPORT
#define GADGET_DLL_API __declspec(dllexport)
#else
#define GADGET_DLL_API __declspec(dllimport)
#endif

#define GADGET_DLL_DMX_BREAK 0x8000
#define GADGET_DLL_FRAMING_ERROR 0x9000

/* The available speeds for Gadget DMX output */
enum
{
  GADGET_DLL_SPEED_MAX,
  GADGET_DLL_SPEED_FAST,
  GADGET_DLL_SPEED_MEDIUM,
  GADGET_DLL_SPEED_SLOW,
  GADGET_DLL_SPEED_SYNC,
  GADGET_DLL_SPEED_COUNT
};

extern "C" {
/***************** Type Definitions *****************/

/* Status flags for when Gadget is being updated, passed to the Gadget2_UpdateStatusCallback */
enum Gadget2_UpdateStatus
{
  Gadget2_Update_Beginning,
  Gadget2_Update_BootloaderFound,
  Gadget2_Update_TransferringFile,
  Gadget2_Update_ReadyForReboot,
  Gadget2_Update_Error
};

/* Callback for log messages */
typedef void(__stdcall Gadget2_LogCallback)(const char *LogData);

/* Callback to provide progress updates as the gadget is being updated */
typedef void(__stdcall Gadget2_UpdateStatusCallback)(Gadget2_UpdateStatus status, void *context);

/***************** Information about the DLL *****************/

/* Returns the version of the DLL being used */
GADGET_DLL_API char *Gadget2_GetDllVersion();

/***************** Startup and Shutdown *****************/

/* Start up the Gadget2 interface. This should be called before any other functions are used */
GADGET_DLL_API int Gadget2_Connect();

/* Shutdown the Gadget2 interface. Stops the threads; do not use other functions after calling this */
GADGET_DLL_API int Gadget2_Disconnect(void);

/***************** Logging *****************/
/* Set the callback for log data */
GADGET_DLL_API void Gadget2_SetLogCallback(Gadget2_LogCallback *Callback);

/* Set the verbosity of log messages */
GADGET_DLL_API void Gadget2_SetLogFilter(int verb, int cat, int sev);

/***************** DMX Transmission *****************/
/* Send DMX */
GADGET_DLL_API void Gadget2_SendDMX(unsigned int DeviceNum, unsigned int PortNum, unsigned char *Buffer,
                                    unsigned int Size);

/* Disable DMX */
GADGET_DLL_API void Gadget2_DisableDMX(unsigned int DeviceNum, unsigned int PortNum);

/* Set DMX Speed */
GADGET_DLL_API void Gadget2_SetDmxSpeed(unsigned int DeviceNum, unsigned int PortNum, unsigned int Speed);

/***************** Information and control of the Gadget *****************/
/* Get the number of Gadget devices found on the PC */
GADGET_DLL_API unsigned int Gadget2_GetNumGadgetDevices();

/* Return the version number of the specified device as a string */
GADGET_DLL_API unsigned char *Gadget2_GetGadgetVersion(unsigned int DeviceNum);

/* Return the serial number of the specified device as a string */
GADGET_DLL_API unsigned int Gadget2_GetGadgetSerialNumber(unsigned int DeviceNum);

/* Return the type of the specified device as a string */
GADGET_DLL_API const char *Gadget2_GetGadgetType(unsigned int DeviceNum);

/* Return the number of DMX ports the specified device has */
GADGET_DLL_API unsigned char Gadget2_GetPortCount(unsigned int DeviceNum);

/* Set the callback for status updates on the firmware update of Gadget2. Context is passed back with the callback */
GADGET_DLL_API void Gadget2_SetUpdateStatusCallback(Gadget2_UpdateStatusCallback *Callback, void *Context);

/* Perform an update of the Gadget2. FirmwarePath should point to the S-Record path for the Gadget Firmware */
GADGET_DLL_API void Gadget2_PerformFirmwareUpdate(unsigned int DeviceNum, const wchar_t *FirmwarePath);

/***************** Raw access mode *****************/
/* Set the device into Raw Access mode*/
GADGET_DLL_API int Gadget2_SetRawReceiveMode(unsigned int DeviceNum, unsigned int PortNum);

/* Return the number of bytes in the raw acess receive queue */
GADGET_DLL_API unsigned int Gadget2_GetNumberOfRXRawBytes(unsigned int DeviceNum, unsigned int PortNum);

/* Get raw bytes from the raw access queue. Raw bytes are copied into the provided Data pointer, so it must be of size
 * Length and must have enough space for the data*/
GADGET_DLL_API void Gadget2_GetRXRawBytes(unsigned int DeviceNum, unsigned int PortNum, unsigned short *Data,
                                          unsigned int Length);

/* Send raw bytes via the Gadget2 */
GADGET_DLL_API void Gadget2_SendRawBytes(unsigned int DeviceNum, unsigned int PortNum, unsigned char *Data,
                                         unsigned int Length);

/* Send a break, startcode and data */
GADGET_DLL_API void Gadget2_SendBreakAndData(unsigned int DeviceNum, unsigned int PortNum, unsigned char StartCode,
                                             unsigned char *Data, unsigned int Length);

/***************** RDM Interface Functions *****************/

/* Do full RDM discovery on the specified device and port */
GADGET_DLL_API void Gadget2_DoFullDiscovery(unsigned int DeviceID, unsigned int PortNum);

/* Turn RDM on or off. Turning it on enables background discovery, and RDM on the port*/
GADGET_DLL_API void Gadget2_SetRDMEnabled(unsigned int DeviceNum, unsigned int PortNum, unsigned char Enable);

/* Return the number of discovered devices */
GADGET_DLL_API unsigned int Gadget2_GetDiscoveredDevices(void);

/* Get the RDMDeviceInfo structre from the device at Index in the array */
GADGET_DLL_API RdmDeviceInfo *Gadget2_GetDeviceInfo(unsigned int Index);

/* Get the RDM Manufacturer ID for the device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceManfID(unsigned int Index);

/* Return the RDM Device ID for the device at Index in the array*/
GADGET_DLL_API unsigned int Gadget2_GetDeviceID(unsigned int Index);

/* Return the software version label for the RDM device at Index in the array */
GADGET_DLL_API unsigned char *Gadget2_GetDeviceSoftwareVersionLabel(unsigned int Index);

/* Return the RDM Protocol version for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceRDMProtocolVersion(unsigned int Index);

/* Return the RDM Model ID for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceModelID(unsigned int Index);

/* Return the RDM Product Category for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceProductCategoryType(unsigned int Index);

/* Return the RDM Software version ID for the RDM device at Index in the array */
GADGET_DLL_API unsigned int Gadget2_GetDeviceSoftwareVersionID(unsigned int Index);

/* Return the RDM DMX footprint for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceDMXFootprint(unsigned int Index);

/* Return the (integer) DMX personality for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceDMXPersonality(unsigned int Index);

/* Return the DMX start address for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceDMXStartAddress(unsigned int Index);

/* Return the RDM Subdevice count for the RDM device at Index in the array */
GADGET_DLL_API unsigned short Gadget2_GetDeviceSubdeviceCount(unsigned int Index);

/* Return the RDM sensor count for the RDM device at Index in the array */
GADGET_DLL_API unsigned char Gadget2_GetDeviceSensorCount(unsigned int Index);

/* Return the number of responses in the RDM command queue */
GADGET_DLL_API unsigned int Gadget2_GetNumResponses(void);

/* Return the RDM response at Index in the RDM command queue */
GADGET_DLL_API RDM_CmdC *Gadget2_GetResponse(unsigned int Index);

/* Removes the RDM response at Index in the RDM command queue */
GADGET_DLL_API void Gadget2_ClearResponse(unsigned int Index);

/* Return the RDM command parameter at Index in the RDM command queue */
GADGET_DLL_API unsigned char Gadget2_GetResponseCommand(unsigned int Index);

/* Return the RDM response parameter at Index in the RDM command queue */
GADGET_DLL_API unsigned short Gadget2_GetResponseParameter(unsigned int Index);

/* Return the RDM response subdevice parameter at Index in the RDM command queue */
GADGET_DLL_API unsigned short Gadget2_GetResponseSubdevice(unsigned int Index);

/* Return the RDM response length for the response at Index in the RDM command queue */
GADGET_DLL_API unsigned char Gadget2_GetResponseLength(unsigned int Index);

/* Return a pointer to the response buffer for the response at Index in the RDM command queue */
GADGET_DLL_API unsigned char *Gadget2_GetResponseBuffer(unsigned int Index);

/* Return the RDM response type for the response at Index in the RDM command queue */
GADGET_DLL_API unsigned char Gadget2_GetResponseResponseType(unsigned int Index);

/* Return the RDM response manufacturer ID for the response at Index in the RDM command queue */
GADGET_DLL_API unsigned short Gadget2_GetResponseManufacturer_id(unsigned int Index);

/* Return the RDM response device ID for the response at Index in the RDM command queue */
GADGET_DLL_API unsigned int Gadget2_GetResponseDevice_id(unsigned int Index);

/* Send an RDM command */
GADGET_DLL_API void Gadget2_SendRDMCommand(unsigned int DeviceNum, unsigned int PortNum, unsigned char Cmd,
                                           unsigned short ParameterID, unsigned short SubDevice, unsigned char DataLen,
                                           const char *Buffer, unsigned short ManfID, unsigned int DevID);
}

#endif /* GADGETDLL_H_ */
