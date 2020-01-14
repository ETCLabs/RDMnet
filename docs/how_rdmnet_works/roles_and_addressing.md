# Roles and Addressing                                                      {#roles_and_addressing}

## Addressing Constructs

### CIDs

Each software entity communicating using RDMnet is referred to as a _component_. Each component has
a unique ID called a _Component ID_, or _CID_. A CID is simply a UUID; for pure-software components
like desktop or mobile applications, a CID may be ephemeral and regenerated each time the software
starts (or persistent, if desired). For hardware-locked components like lighting fixtures or
gateways, a CID should be generated using the UUID version 3 or version 5 method, to produce the
same CID throughout the lifetime of the product.

### UIDs

In addition to its CID, each component in RDMnet also has an RDM Unique ID (UID). This is for 
compatibility with the RDM protocol.

RDM UIDs are a six-byte identifier, divided into two parts:

```
mm:mm:dd:dd:dd:dd
```

Where `mm:mm` is the 2-byte ESTA Manufacturer ID (valid range `0x0000` to `0x7fff`) and
`dd:dd:dd:dd` is the 4-byte Device ID.

Hardware-locked devices typically have a UID assigned by combining an organization's ESTA
manufacturer ID with a device ID generated at time of manufacture and unique among all products
manufactured by that organization.

#### Dynamic UIDs

RDMnet also introduces the concept of dynamic UIDs, which are assigned by a broker at runtime.
Dynamic UIDs are useful for pure software devices of which many instances can be downloaded and
used on the personal devices of users.

A dynamic UID is assigned to a component by a broker on initial connection, and becomes invalid
after disconnecting; the component will get a different, unrelated dynamic UID if it connects
again. Think of a dynamic UID like a DHCP-assigned IP address - it is not permanent and has a lease
time equal to the length of the RDMnet connection.

A dynamic UID is identified by the top bit of the Manufacturer ID field being set to 1. Recall that
only manufacturer IDs from `0x0000` to `0x7fff` are assigned, which allows this reserved bit to be
used to indicate a dynamic UID.

## Component Roles

Components in RDMnet fall into 3 subcategories: Controllers, Devices and Brokers.

### Controllers

An RDMnet controller originates and sends RDM commands to devices, and handles RDM responses from
those devices. At a high level, a controller's job is to allow the user to discover and configure
RDMnet devices. Typical examples of controllers are lighting console software, lighting network
configuration software, and mobile lighting network monitoring apps.

Controllers can participate in one or more RDMnet scopes. All controllers must by default be
configured to discover and connect to the default RDMnet scope (the string `"default"`). Additional
scopes can also be added (and the default scope removed, if desired) by user configuration.

In each scope, controllers connect to a broker for that scope, either by discovering the broker
using DNS-SD, or connecting to a preconfigured IP address and port. Controllers can then send a
command to retrieve a list of other controllers and devices connected to each broker.

The reply to this command will contain the following information about each component currently
connected:

* Its CID
* Its RDM UID
* Whether it is a device or a controller

This information is all that is necessary for the controller to retrieve more information about
each component using RDM commands.

### Devices

RDMnet devices are the primary recipents of configuration in RDMnet. A device is typically (but not
always) a piece of hardware-locked equipment; a lighting fixture, a DMX gateway, etc.

Devices can only participate in one RDMnet scope at a time. All devices must by default be
configured to discover and connect to the default RDMnet scope (the string `"default"`); this can
be changed by user configuration.

Devices connect to a broker for their configured scope either by discovering the broker using
DNS-SD or by connecting to a preconfigured IP address and port. Once connected, devices are 
passive; they wait to receive RDM commands before acting on them and sending responses.

In addition to accepting configuration themselves, devices can also represent and forward
configuration to and from other RDM responders; see \ref devices_and_gateways for more details
about this.

### Brokers
