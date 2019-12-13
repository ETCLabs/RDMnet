# How RDMnet Works                                              {#how_it_works}

This overview assumes a working familiarity with RDM. For more information about RDM, go to
http://www.rdmprotocol.org.

In RDM, the transaction for obtaining or modifying data from a piece of equipment is quite simple.
An RDM Controller sends an RDM command to an RDM Responder. The RDM Responder replies with an RDM
response.

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

RDMnet allows RDM commands and responses to be sent in almost the same way, with two key
differences:

* The messages are sent over an IP network.
* The messages are passed through an intermediary called a Broker.

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

Why is the broker necessary? We'll get back to that. First, some new terminology.

In RDMnet, the protocol that carries RDM messages is called **RDM Packet Transport (RPT)**.
Something that originates RDM commands using RPT and receives responses is called an
**RPT Controller**, and something that receives RDM commands using RPT and sends responses is
called an **RPT Device**. When discussing RDMnet, these are often shortened to simply *controller*
and *device*.

An RPT message that carries an RDM commands is called a **Request**, and an RPT message that
carries an RDM response is called a **Notification**.

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
**an RDMnet system is not functional without a broker**.

Requiring a broker may seem like a burden, but its presence provides RDMnet with many helpful
features.

## Benefits of the broker model

### Multi-Controller functionality

RDM is a single-controller environment. In RDMnet, you can have as many controllers on the network
as you want. The broker keeps everyone up-to-date on the state of each Device by forwarding the
responses to RDM SET commands to all connected controllers.

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

In RDMnet, each controller and device implementation only needs to worry about one connection: its
connection to the broker. Resource-constrained device implementations need not shoulder the burden
of keeping track of many controller connections and handling subscriptions and updates. The
scalability of an RDMnet system is limited only by the scalability of its broker.

When a broker is run on non-resource-constrained hardware such as a modern PC or server, an RDMnet
system can easily scale to hundreds of controllers and tens of thousands of devices.

### Simplicity

Having a broker makes implementing controllers and devices easy. Take a look at `example_device.c`
in the example Device application for proof of this. The startup steps for a device are simple:

* Discover a broker
* Connect using TCP
* Start listening for RDM commands

For a controller, there is one additional step:

* Discover a broker
* Connect using TCP
* Ask the broker for a device list
* Send RDM commands to one or more devices

## Scopes

A broker has a scope, which affects which controllers and devices connect to it. A controller or
device will only connect to a broker with a matching scope.

Scopes are UTF-8 strings from 1 to 62 bytes in length (this limitation is imposed by the
requirements of DNS-SD, which is used to discover brokers). The standard requires all RDMnet
equipment or software to be shipped with a scope configured to the string `"default"`, which
simplifies initial setup and operation. Most applications will have no need to change the Scope
from the default setting, but it can be useful for large-scale and advanced setups.

## Broker Setups

To simplify RDMnet for end-users, a common way to ship brokers is to co-locate a broker and
controller on the same piece of physical hardware (e.g. a laptop or lighting console). In this
setup, the broker generally runs as a system service or daemon which communicates with the
controller via the local network stack. For mobile platforms, which are generally unfriendly to
background processes, a single app can implement both controller and broker functionality.

Alternatively, a broker can be designed to be run on a more traditional standalone server, such
that the broker is "always on" on the lighting network.

## Deeper Dives

To learn more about specific aspects of RDMnet, take a look at one of the topic pages below.

* \subpage roles_and_addressing
* \subpage devices_and_gateways
* \subpage discovery
