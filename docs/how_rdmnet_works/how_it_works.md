# How RDMnet Works                                              {#how_it_works}

This overview assumes a working familiarity with RDM. For more information
about RDM, go to http://www.rdmprotocol.org.

In RDM, the transaction for obtaining or modifying data from a piece of
equipment is quite simple. An RDM Controller sends an RDM command to an RDM
Responder. The RDM Responder replies with an RDM response.

```
RDM Controller        RDM Responder
      ||                    ||
      ||    RDM command     ||
      || -----------------> ||
      ||    RDM response    ||
      || <----------------- ||
      ||                    ||
```

This transaction takes place on the DMX512 physical layer.

RDMnet allows RDM commands and responses to be sent in almost the same way,
with two key differences:

* The messages are sent over an IP network.
* The messages are passed through an intermediary called a %Broker.

```
RDM Controller            Broker            RDM Responder
      ||                    ||                    ||
      ||    RDM command     ||                    ||
      || -----------------> ||    RDM command     ||
      ||                    || -----------------> ||
      ||                    ||    RDM response    ||
      ||    RDM response    || <----------------- ||
      || <----------------- ||                    ||
      ||                    ||                    ||
```

Why is the %Broker necessary? We'll get back to that. First, some new
terminology.

In RDMnet, the protocol that carries RDM messages is called **RDM Packet
Transport (RPT)**. Something that originates RDM commands using RPT and
receives responses is called an **RPT Controller**, and something that receives
RDM commands using RPT and sends responses is called an **RPT Device**. When
discussing RDMnet, these are often shortened to simply *Controller* and
*Device*.

An RPT message that carries an RDM commands is called a **Request**, and an RPT
message that carries an RDM response is called a **Notification**.

Let's update our diagram to reflect the proper terminology:

```
RPT Controller            Broker              RPT Device
      ||                    ||                    ||
      ||    RPT Request     ||                    ||
      || -----------------> ||    RPT Request     ||
      ||                    || -----------------> ||
      ||                    ||  RPT Notification  ||
      ||  RPT Notification  || <----------------- ||
      || <----------------- ||                    ||
      ||                    ||                    ||
```

All three of these components are necessary for message transport in RDMnet;
**an RDMnet system is not functional without a %Broker**.

Requiring a %Broker may seem like a burden, but its presence provides RDMnet
with many helpful features.

## Benefits of the %Broker model

### Multi-Controller functionality

RDM is a single-controller environment. In RDMnet, you can have as many
Controllers on the network as you want. The %Broker keeps everyone up-to-date
on the state of each Device by forwarding the responses to RDM SET commands to
all connected Controllers.

```
 Controller 1             Broker                Device
      ||                    ||                    ||
      ||    SET command     ||                    ||
      || -----------------> ||    SET command     ||
      ||                    || -----------------> ||
      ||                    ||    SET response    ||
      ||    SET response    || <----------------- ||
      || <----------------- ||                    ||
                            ||                    ||
 Controller 2               ||                    ||
      ||                    ||                    ||
      ||    SET response    ||                    ||
      || <----------------- ||                    ||
                            ||                    ||
 Controller 3               ||                    ||
      ||                    ||                    ||
      ||    SET response    ||                    ||
      || <----------------- ||                    ||
```

### Scalability

In RDMnet, each Controller and Device implementation only needs to worry about
one connection: its connection to the %Broker. Resource-constrained Device
implementations need not shoulder the burden of keeping track of many
Controller connections and handling subscriptions and updates. The scalability
of an RDMnet system is limited only by the scalability of its %Broker.

When a %Broker is run on non-resource-constrained hardware such as a modern PC
or server, an RDMnet system can easily scale to hundreds of Controllers and
tens of thousands of Devices.

### Simplicity

Having a %Broker makes implementing Controllers and Devices easy. Take a look at
`example_device.c` in the example Device application for proof of this. The
startup steps for a Device are simple:

* Discover a %Broker
* Connect using TCP
* Start listening for RDM commands

For a Controller, there is one additional step:

* Discover a %Broker
* Connect using TCP
* Ask the %Broker for a Device list
* Send RDM commands to one or more Devices

## Scopes

A %Broker has a Scope, which affects which Controllers and Devices connect to
it. A Controller or Device will only connect to a %Broker with a matching
Scope.

Scopes are UTF-8 strings from 1 to 62 bytes in length (this limitation is
imposed by the requirements of DNS-SD, which is used to discover %Brokers). The
standard requires all RDMnet equipment to be shipped with the Scope set to the
string `"default"`, which simplifies initial setup and operation. Most
applications will have no need to change the Scope from the default setting,
but it can be useful for large-scale and advanced setups.

## %Broker Setups

To simplify RDMnet for end-users, it is anticipated that a common way to ship
%Brokers will be to co-locate a %Broker and Controller on the same piece of
physical hardware (e.g. a lighting console). In this setup, the %Broker could
run as a system service or daemon which communicates with the Controller via
the local network stack, or the %Broker and Controller functionality could be
implemented by the same software.

Alternatively, a %Broker could be designed to be run on a more traditional
standalone server, such that the %Broker is "always on" on the lighting
network.

## Deeper Dives

To learn more about specific aspects of RDMnet, take a look at one of the topic
pages below.

* \subpage roles_and_addressing
* \subpage discovery
* \subpage devices_and_gateways
