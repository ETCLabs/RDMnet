# Extensible Packet Transport (EPT)                                                          {#ept}

In addition to RPT and LLRP, RDMnet also defines a protocol called
**Extensible Packet Transport (EPT)**, which allows for non-RDM data to be transmitted through an
RDMnet broker. EPT data passes between **EPT Clients**, which connect to a broker the same way
RPT Controllers and Devices do.

```
EPT Client           Broker           EPT Client
    ||                 ||                 ||
    ||  Opaque data A  ||                 ||
    || --------------> ||  Opaque data A  ||
    ||                 || --------------> ||
    ||                 ||                 ||
    ||                 ||                 ||
    ||                 ||  Opaque data B  ||
    ||  Opaque data B  || <-------------- ||
    || <-------------- ||                 ||
    ||                 ||                 ||
    ||                 ||                 ||
```

EPT data has no prescribed structure, and EPT clients have no prescribed "controller" or "device"
role the way controllers and devices do in RPT.

Instead, EPT uses a construct called _sub-protocols_ to attach identification information to data.
A sub-protocol is identified by a 32-bit value which consists of a 16-bit ESTA manufacturer ID
combined with a 16-bit protocol identifier:

```
MSB                                         LSB
+------------------------+--------------------+
| 16-bit manufacturer ID | 16-bit protocol ID |
+------------------------+--------------------+
```

EPT clients implement one or more sub-protocols; all data sent over EPT is accompanied by a
sub-protocol identifier. There are currently no standard sub-protocols defined by RDMnet.
Manufacturers can create manufacturer-specific EPT sub-protocols and identify them with a
combination of their ESTA manufacturer ID and a 16-bit identifier that they choose.

## EPT Clients

EPT clients are identified and addressed by a CID; see \ref roles_and_addressing for more
information on CIDs.

EPT clients participate in one or more scopes, just like an RDMnet controller. EPT clients can
query a broker to discover a list of other EPT clients on their scope, along with any sub-protocols
those EPT clients support. Using this method, an EPT client discovers other EPT clients that have
compatible protocols and can send data to and receive data from them using the CID as an addressing
identifier.

## Brokers

Like in RPT, all traffic between EPT clients passes through an RDMnet broker. All RDMnet brokers
implement both RPT and EPT; this means that a broker for a given RDMnet scope may at any time have
an assortment of RDMnet controllers, RDMnet devices and EPT clients connected to it.
