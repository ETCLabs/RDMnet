#ifndef RDM_DEVICE_INFO_H
#define RDM_DEVICE_INFO_H

#include <stdint.h>

#include "rdmEtcConsts.h"
#include "uid.h"

struct RdmDeviceInfo
{
    // Constructor for RdmDeviceInfo; two flags determine if the information is valid yet
    explicit RdmDeviceInfo(uid i)
        : manufacturer_id (i.manu),
          device_id (i.id),
          rdm_protocol_version(0),
          device_model_id(0),
          product_category_type(0),
          software_version_id(0),
          dmx_footprint(0),
          dmx_personality(0),
          dmx_start_address(0),
          subdevice_count(0),
          sensor_count(0),
          port_number(0),
          subdevice_id(0),
          software_version_label_valid(false),
          e120_device_info_valid(false)
    {    
        memset(software_version_label, 0, sizeof(software_version_label));
    }
    RdmDeviceInfo() {}
    uint16_t manufacturer_id;                           // ESTA-assigned manufacturer id
    uint32_t device_id;                                 // Unique to the manufacturer
    uint8_t software_version_label[RDM_MAX_TEXT + 1];   // text, up to 32 characters

    uint16_t rdm_protocol_version;
    uint16_t device_model_id;                           // manufacturer-unique, assigned to device/model
    uint16_t product_category_type;                     // enumerated
    uint32_t software_version_id;

    // DMX universe footprint of the device
    uint16_t dmx_footprint;                             // up to 512
    uint16_t dmx_personality;
    uint16_t dmx_start_address;                         // start at slot 1

    uint16_t subdevice_count;
    uint8_t sensor_count;

    // Other Device Properties
    uint8_t port_number;                                 // Port number on which the device has been discovered (1-based)

    uint16_t subdevice_id;

    // Used to test for full completion of the struct
    bool software_version_label_valid;
    bool e120_device_info_valid;
};

#endif