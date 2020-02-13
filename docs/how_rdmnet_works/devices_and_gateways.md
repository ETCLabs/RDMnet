# Devices and Gateways                                                      {#devices_and_gateways}

RDMnet is closely tied to Streaming ACN (ANSI E1.31, aka sACN); RDMnet devices are often also
senders or receivers of sACN. Typically, sACN is used for live control data, whereas RDMnet is used
for "pre-show" configuration. This pairing between the two protocols motivates RDMnet's model of a
device.

## Modeling an RDMnet Device

Devices can be thought of as containing a set of RDM responders, each of which accepts some RDM
configuration. Every device has a special RDM responder known as the _Default Responder_ which
represents the top-level configuration data about the device - its name, manufacturer, IP
addressing information, etc.

If a device only implements RDMnet, not RDM or sACN, the default responder is all that's needed; it
can contain all of the RDMnet-configurable information about the device. But when a device starts
interacting with sACN or RDM, things get a bit more complex.

### Endpoints

Consider the example of a 2-port DMX/RDM gateway:

![A 2-port DMX/RDM Gateway](./2_port_gateway.png)

One of the main ways RDMnet adds value is by providing a standard way of interfacing with RDM
responders through gateways. Each port on the two-port gateway is represented by an _endpoint_ in
the RDMnet protocol. RDM responders are associated with endpoints; this association represents
which gateway port they are connected to.

ANSI E1.37-7, an extension to RDM, provides RDM messages for getting information about RDMnet
gateways. The `ENDPOINT_LIST` message is used to retrieve a list of endpoints on a gateway, and the
`ENDPOINT_RESPONDERS` message is used to get a list of RDM responders on each endpoint.

Every RDM responder except for the default responder is associated with an endpoint. Endpoint
numbers are 16 bits values, starting at 1. To address a responder on an endpoint, you must include
the endpoint number in the RDMnet message. When addressing the default responder, this field is set
to a reserved value, `NULL_ENDPOINT` (0).

In the above figure, the default responder has a UID `0000:00000001`. To build an RDM command
addressed to the gateway itself, using the RDMnet controller API, we would use the following
addressing information:

```c
RdmnetLocalRdmCommand cmd;
// The destination UID of the RDMnet addressing header indicates which RDMnet device we are
// addressing.
cmd.rdmnet_dest_uid.manu = 0x0000;
cmd.rdmnet_dest_uid.id =  0x00000001;
cmd.dest_endpoint = E133_NULL_ENDPOINT;

// The RDM destination UID indicates which RDM responder we are addressing. In this case, it's the
// default responder, which shares the same UID as the encompassing device.
cmd.rdm_dest_uid.manu = 0x0000;
cmd.rdm_dest_uid.id = 0x00000001;
// Build the rest of the RDM command...
```

The gateway is also connected to two RDM responders, which are attached to port 1 (which the
gateway represents over RDMnet as endpoint 1). To address one of these responders, we would:

```c
RdmnetLocalRdmCommand cmd;
// This UID hasn't changed - we are still addressing the same device at the RDMnet level.
cmd.rdmnet_dest_uid.manu = 0x0000;
cmd.rdmnet_dest_uid.id = 0x00000001;
cmd.dest_endpoint = 1;

// The RDM destination UID indicates which RDM responder we are addressing; in this case, the
// physical responder 0000:00000012 connected to port 1 on the gateway.
cmd.rdm_dest_uid.manu = 0x0000;
cmd.rdm_dest_uid.id = 0x00000012;
```

### Patching ports to sACN universes
