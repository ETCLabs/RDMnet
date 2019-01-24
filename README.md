## Important Note: Read Me First

THE SOFTWARE IN THIS REPOSITORY IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33
rev. 77. <mark><b>UNDER NO CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR OR
INCLUDED IN ANY PRODUCT AVAILABLE FOR GENERAL SALE TO THE PUBLIC.</b></mark>
DUE TO THE INEVITABLE CHANGE OF DRAFT PROTOCOL VALUES AND BEHAVIORAL
REQUIREMENTS, <mark>PRODUCTS USING THIS SOFTWARE WILL **NOT** BE INTEROPERABLE
WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED STANDARD.</mark>

# RDMnet

[![Build status](https://ci.appveyor.com/api/projects/status/76wa62avw50x7r9o?svg=true)](https://ci.appveyor.com/project/ETCLabs/rdmnet)

*RDMnet* is an upcoming entertainment technology standard by
[ESTA](http://tsp.esta.org) for transmission of [RDM](http://www.rdmprotocol.org)
over IP networks. RDMnet leverages and extends the widely-adopted RDM message
set and provides a standard method for configuring both IP-based entertainment
equipment and interfaces between IP and DMX512 (commonly referred to as
*gateways*).

## About this ETCLabs Project

RDMnet is official, open-source software developed by ETC employees and is
designed to interact with ETC products. For challenges using, integrating,
compiling, or modifying this software, we encourage posting on the
[issues page](https://github.com/ETCLabs/RDMnet/issues) of this project.

Before posting an issue or opening a pull request, please read the
[contribution guidelines](./CONTRIBUTING.md).

## Repository Contents

This repository contains a C-language library for core RDMnet communication. It
also contains example applications that demonstrate the roles of Components in
RDMnet:

* *rdmnet_controller_example*: A Qt-based GUI application which does basic discovery,
display and configuration of RDMnet Components.
* *rdmnet_broker_example*: A console application which implements an RDMnet Broker.
* *rdmnet_device_example*: A console application which implements an RDMnet Device.
* *llrp_manager_example*: A console application which implements a basic LLRP Manager.

The applications are available as a binary package
[here](https://etclabs.github.io/RDMnet).

The library and applications currently support Microsoft Windows, and are built
using CMake and Microsoft's Visual Studio toolchain.

## Building

For instructions on building the RDMnet library and applications, as well as an
RDMnet overview and in-depth documentation, please see the
[documentation](https://etclabs.github.io/RDMnet/docs/index.html).

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

* The Broker has no RDM responder functionality and does not implement an LLRP 
  Target
* EPT is not implemented
* IPv6 is not implemented
* The "Request Dynamic UID Assignment" and "Fetch Dynamic UID Assignment List"
  Broker messages are not yet implemented

Other key items on the `TODO` list include:

* Higher API layers to make library usage simpler
* Platform ports for macOS, Linux, and several popular embedded platforms
* Tests and continuous integration

## Standard Version

The current version of this repository implements BSR E1.33 rev. 77, which is
currently being offered to the ESTA Control Protocols Working Group for a
publication vote.
