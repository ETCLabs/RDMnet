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
interacting with sACN or RDM, things get a bit more complex. Let's consider the example of a 2-port
DMX gateway.

Otherwise, 
Devices may have additional RDM responders
