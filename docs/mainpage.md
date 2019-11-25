# RDMnet                                                            {#mainpage}

## Introduction

RDMnet is a library that implements an ANSI standard, **ANSI E1.33: Message
Transport and Device Management of ANSI E1.20 (RDM) over IP Networks**,
commonly referred to as **RDMnet**. ANSI E1.33 can be downloaded from the
[ESTA TSP website](https://tsp.esta.org/tsp/documents/published_docs.php).
The RDMnet library is designed to be portable and scalable to almost any RDMnet
usage scenario, from lightweight embedded devices to large-scale data sending
and routing operations.

Check out \ref getting_started to get started with using the library in your
application.

\htmlonly
The full API reference is organized by <a href="modules.html">module</a>.

\endhtmlonly
## What is RDMnet?

RDMnet is an extension of [RDM](http://www.rdmprotocol.org), a protocol for
discovery and remote configuration of entertainment lighting and other
equipment. In RDM, messages are transported over the physical layer specified
by [DMX512](https://en.wikipedia.org/wiki/DMX512), which is ubiquitous in the
entertainment lighting world and is becoming more prevalent in architectural
lighting and other applications.

The main feature of RDMnet is a standard method to transport the messages
defined by RDM in an IP network. RDMnet contains several key features to
accomplish this end and to build on RDM:

* Built-in scalability, including considerations for a many-to-many
  relationship between controllers and responders
* Zero-configuration setup, leveraging mDNS and [DNS-SD](http://www.dns-sd.org/)
  for network discovery
* Standardization of the behavior of interfaces between IP and DMX512 (commonly
  referred to as *gateways*)

For an overview of how RDMnet works, check out \ref how_it_works.

## Platforms

RDMnet is currently ported for the following platforms:
* macOS
* Microsoft Windows

### Building RDMnet for Your Platform

Prerequisites:

* **CMake**. CMake is an industry-standard cross-platform build system
  generator for C and C++. CMake can be downloaded
  [here](https://cmake.org/download). It is also available as a package in many
  Linux distributions.

* **An implementation of DNS-SD/mDNS**.
  There are different options for this on different platforms:
  + On Microsoft Windows:
    - Apple's Bonjour for Windows implements DNS-SD/mDNS. Note that you must
      have an Apple developer account to download the Bonjour SDK for Windows,
      and that bundling Bonjour with a Windows application may be subject to
      additional licensing restrictions from Apple. The SDK is available
      [here](https://developer.apple.com/bonjour/). Define
      RDMNET_WINDOWS_USE_BONJOUR_SDK=ON at configure time to use the Bonjour SDK.
    - ETC's fork of Bonjour for Windows ("mDNSWindows"), maintained
      [here](https://github.com/ETCLabs/mDNSWindows). ETC maintains a fork of the
      Apache-licensed Bonjour code which can be used as a DNS-SD/mDNS provider on
      Windows. The RDMnet CMake config will automatically download the latest
      version if no other options are specified. Or, to build mDNSWindows from
      source, specify the source location with the MDNSWINDOWS_SRC_LOC option.
  + On macOS:
    - RDMnet uses native Bonjour, which comes with every macOS distribution.

* **For controller: Qt (>= 5.9.7) open-source**.  Qt installers are available
  [here](https://www.qt.io/download). To point CMake at the Qt dependency,
  there are two options:
  + Set a system environment variable called QTDIR which points at the Qt
    installation directory (i.e. .../Qt/5.9.7/msvc2017_64)
  + Provide the Qt installation directory as an argument to CMake:
    ```
    cmake -DQTDIR=[...]/Qt/5.9.7/msvc2017_64
    ```

RDMnet is built with [CMake](https://cmake.org), a popular cross-platform build
system generator. CMake can also be used to include RDMnet as a dependency to
other projects, i.e. using the `add_subdirectory()` command.

To configure and build RDMnet on its own using CMake, follow these steps:

1. [Download and install](https://cmake.org/download/) CMake version 3.3 or
   higher.
2. Clone (or download and extract) the RDMnet repository:
   ```
   $ git clone https://github.com/ETCLabs/RDMnet
   ```
3. Create a directory in your location of choice (a directory called 'build' at
   the repository root is recommended) to hold your build projects or
   Makefiles:
   ```
   $ mkdir build && cd build
   ```
4. Run CMake to configure the RDMnet project:
   ```
   $ cmake .. [or cmake path/to/RDMnet/root as applicable]
   ```
   You can optionally specify your build system with the `-G` option;
   otherwise, CMake will choose a system-appropriate default. Use `cmake --help`
   to see all available options.

   By default, a configuration will be attempted that will build all of the
   example applications. This behavior can be tweaked:
   ```
   $ cmake -DRDMNET_BUILD_CONSOLE_EXAMPLES=ON .. # Exclude the Qt example app
   $ cmake -DRDMNET_BUILD_EXAMPLES=OFF .. # Do not build any examples
   ```
5. Use CMake to invoke the genreated build system to build the RDMnet library
   and example applications (if applicable):
   ```
   $ cmake --build .
   ```
   If you are generating IDE project files, you can use CMake to open the
   projects in the IDE:
   ```
   $ cmake --open .
   ```

#### Controller only:

6. To run or distribute the controller executable outside of Visual Studio, run
   the `windeployqt` application on the `RDMnetControllerGUI.exe` binary in its
   final location; e.g.
   ```
   %QTDIR%\bin\windeployqt RDMnetControllerGUI.exe
   ```

## Dependencies

### EtcPal

RDMnet depends on ETC's Platform Abstraction Layer (EtcPal) so that it can be 
platform-neutral. By default, EtcPal is automatically included as a submodule
in the `external` directory for RDMnet source builds. The CMake configuration
will automatically update and pull the submodule before building.

See the [documentation for EtcPal](https://etclabs.github.io/EtcPal).

### RDM

RDMnet depends on the ETC's RDM library for RDM protocol support. By default,
RDM is automatically included as a submodule in the `external` directory for
RDMnet source builds. The CMake configuration will automatically update and
pull the submodule before building.

See the [documentation for RDM](https://etclabs.github.io/RDM).

### DNS-SD

RDMnet requires a DNS-SD library. It is ported to use the following providers:
- On Windows:
  + [Bonjour SDK for Windows](https://developer.apple.com/bonjour/) v3.0
  + [ETCLabs/mDNSWindows](https://github.com/ETCLabs/mDNSWindows) (fork of
    Bonjour with permissive licensing) v1.2.0
    * Note: The Windows CMake configuration will download these binaries by
      default
- On macOS:
  + Native Bonjour (installed by default)
- On Linux:
  + [avahi-client](https://www.avahi.org/) v0.7
    * For compiling RDMnet on Debian-based distributions:
      `sudo apt-get install libavahi-client-dev`

### Qt

The controller depends on Qt 5.9.7 or higher. You can download the open-source
version of Qt without an account [here](https://www.qt.io/download).

### Platform Dependencies

The platform ports of RDMnet have the following dependencies:
* Windows
  + Windows XP SP1 or higher
