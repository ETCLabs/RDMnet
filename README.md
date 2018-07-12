## Important Note: Read Me First

THE SOFTWARE IN THIS REPOSITORY IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33
rev. 63. <mark><b>UNDER NO CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR OR
INCLUDED IN ANY PRODUCT AVAILABLE FOR GENERAL SALE TO THE PUBLIC.</b></mark> 
DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL VALUES AND BEHAVIORAL
REQUIREMENTS, <mark>PRODUCTS USING THIS SOFTWARE WILL **NOT** BE INTEROPERABLE
WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.</mark>

# RDMnet

This repository contains a software library and example applications that
implement *RDMnet*, an upcoming entertainment technology standard by
[ESTA](http://tsp.esta.org) for transmission of [RDM](http://www.rdmprotocol.org)
over IP networks. RDMnet leverages and extends the widely-adopted RDM message
set and provides a standard method for configuring both IP-based entertainment
equipment and interfaces between IP and DMX512 (commonly referred to as
*gateways*).

## Repository Contents

This repository contains a C-language library for core RDMnet communication. It
also contains example applications that demonstrate the roles of Components in
RDMnet:

* *RDMnetControllerGUI*: A Qt-based GUI application which does basic discovery,
display and configuration of RDMnet Components.
* *broker*: A console application (which can also be run as a Windows service)
which implements an RDMnet Broker.
* *device*: A console application which implements an RDMnet Device.
* *fakeway* (binary only): A console application which implements an RDMnet
Gateway, using one or more ETC [Gadget II](https://www.etcconnect.com/Products/Networking/Gadget-II/Features.aspx)
devices to communicate with RDM fixtures.
* *manager*: A console application which implements a basic LLRP Manager.

The applications are available as a binary package here **TODO Link**.

The library and applications currently support Microsoft Windows, and are built
using Microsoft's Visual Studio 2015 toolchain.

## Building

For instructions on building the RDMnet library and applications, as well as an
RDMnet overview and in-depth documentation, please see the documentation
**TODO link**.

## Future Plans

This library is maintained by [ETC](http://www.etcconnect.com). This early
version has been published to facilitate early testing of the draft standard by
interested manufacturers. ETC plans to maintain and enhance this library, and
to update it for each new draft revision of the standard, as well as the final
version. There are no planned licensing changes for these future updates, which
means that you will be able to use this library in your commercial and/or
closed-source application, subject to the terms of the Apache License 2.0.

The current prototypes are mostly compliant with the current standard version,
with a few exceptions:

* The Broker and Controller have no RDM responder functionality and do not
  implement LLRP Targets
* EPT is not implemented
* IPv6 is not implemented
* Device and Fakeway do not support all required RDM PIDs

Other key items on the `TODO` list include:

* Higher API layers to make library usage simpler
* Platform ports for macOS, Linux, and several popular embedded platforms
* Usage of a cross-platform build system
* Tests and continuous integration

## Standard Version

The current version of this repository implements BSR E1.33 rev. 63, which was
offered for public review by ESTA in the first quarter of 2018.
