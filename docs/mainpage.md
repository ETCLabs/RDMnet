# RDMnet                                                                                {#mainpage}

## Introduction

RDMnet is a library that implements an ANSI standard, **ANSI E1.33: Message Transport and Device
Management of ANSI E1.20 (RDM) over IP Networks**, commonly referred to as **RDMnet**. ANSI E1.33
can be downloaded from the [ESTA TSP website](https://tsp.esta.org/tsp/documents/published_docs.php).
The RDMnet library is designed to be portable and scalable to almost any RDMnet usage scenario,
from lightweight embedded devices to large-scale data sending and routing operations.

See @ref how_it_works for an RDMnet primer, or if you already know how it works, check out
@ref getting_started to get started with using the library in your application.

\htmlonly
The full API reference is organized by <a href="modules.html">module</a>.

\endhtmlonly
## What is RDMnet?

RDMnet is an extension of [RDM](http://www.rdmprotocol.org), a protocol for discovery and remote
configuration of entertainment lighting and other equipment. In RDM, messages are transported over
the [DMX512](https://en.wikipedia.org/wiki/DMX512) physical layer, which is ubiquitous in the
entertainment lighting world and is becoming more prevalent in architectural lighting and other
applications.

The main feature of RDMnet is a standard method to transport the messages defined by RDM in an IP
network. RDMnet contains several key features to accomplish this end and to build on RDM:

* Built-in scalability, including considerations for a many-to-many relationship between
  controllers and responders
* Zero-configuration setup, leveraging mDNS and [DNS-SD](http://www.dns-sd.org/) for network
  discovery
* Standardization of the behavior of interfaces between IP and DMX512 (commonly referred to as
  *gateways*)

## Platforms

RDMnet supports all platforms targeted by [EtcPal](https://github.com/ETCLabs/EtcPal), including
Microsoft Windows, macOS, Linux and several embedded RTOS platforms.

## Dependencies

### EtcPal

RDMnet depends on ETC's Platform Abstraction Layer (EtcPal) so that it can be platform-neutral. By
default, EtcPal is automatically included as a submodule in the `external` directory for RDMnet
source builds. The CMake configuration will automatically update and pull the submodule before
building.

See the [documentation for EtcPal](https://etclabs.github.io/EtcPal).

### RDM

RDMnet depends on the ETC's open-source RDM library for RDM protocol support. By default, RDM is
automatically included as a submodule in the `external` directory for RDMnet source builds. The
CMake configuration will automatically update and pull the submodule before building.

See the [documentation for RDM](https://etclabs.github.io/RDM).

### DNS-SD

RDMnet requires a DNS-SD library. It is ported to use the following providers:
- On Windows:
  + [Bonjour SDK for Windows](https://developer.apple.com/bonjour/) v3.0
  + [ETCLabs/mDNSWindows](https://github.com/ETCLabs/mDNSWindows) (fork of Bonjour with permissive 
    licensing) v1.2.0
    * Note: The Windows CMake configuration will download these binaries by default
- On macOS:
  + Native Bonjour (installed by default)
- On Linux:
  + [avahi-client](https://www.avahi.org/) v0.7
    * For compiling RDMnet on Debian-based distributions: `sudo apt-get install libavahi-client-dev`
- On other platforms:
  + RDMnet includes its own lightweight implementation of mDNS/DNS-SD querying functionality that
    is used on lower-level RTOS targets.

### Qt

The controller example application depends on Qt 5.9.7 or higher. You can download the open-source 
version of Qt without an account [here](https://www.qt.io/download).
