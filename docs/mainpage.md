# RDMnet                                                            {#mainpage}

## Important Note: Read Me First

THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 rev. 77. **UNDER NO
CIRCUMSTANCES SHOULD THIS SOFTWARE BE USED FOR OR INCLUDED IN ANY PRODUCT
AVAILABLE FOR GENERAL SALE TO THE PUBLIC.** DUE TO THE INEVITABLE CHANGE OF
DRAFT PROTOCOL VALUES AND BEHAVIORAL REQUIREMENTS, PRODUCTS USING THIS SOFTWARE
WILL **NOT** BE INTEROPERABLE WITH PRODUCTS IMPLEMENTING THE FINAL RATIFIED
STANDARD.

## Introduction

RDMnet is a library that implements an upcoming ANSI standard, **BSR E1.33:
Message Transport and Device Management of ANSI E1.20 (RDM) over IP Networks**,
commonly referred to as **RDMnet**. The RDMnet library is designed to be
portable and scalable to almost any RDMnet usage scenario, from lightweight
embedded devices to large-scale data sending and routing operations.

\htmlonly
To jump right into the documentation, check out the
<a href="modules.html">Modules Overview</a>.

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

RDMnet is currently ported for the following platforms and toolchains:
* Windows
  + Microsoft Visual Studio (x86 and x86_64)

### Building RDMnet for Your Platform

Prerequisites:

* **For all applications: Microsoft Visual Studio**. Currently the only
  toolchain for which lwpa (and thus RDMnet) is ported. Visual Studio 2017
  Community Edition is free, without restriction, for open-source projects. It
  is available [here](https://visualstudio.microsoft.com/downloads/). Make sure
  to install Visual C++ as part of the Visual Studio installation.

* **For controller, broker and device: An implementation of DNS-SD/mDNS**.
  There are two options for this:
  + Apple's Bonjour service implements DNS-SD/mDNS. Note that you must have an
    Apple developer account to download the Bonjour SDK for Windows, and that
    bundling Bonjour with a Windows application may be subject to additional
    licensing restrictions from Apple. The SDK is available
    [here](https://developer.apple.com/bonjour/). Define
    RDMNET_WINDOWS_USE_BONJOUR_SDK=ON at configure time to use the Bonjour SDK.
  + ETC's fork of Bonjour, maintained
    [here](https://github.com/ETCLabs/mDNSWindows). ETC maintains a fork of the
    Apache-licensed Bonjour code which can be used as a DNS-SD/mDNS provider on
    Windows. Download the binaries from the Github releases page and specify
    their location with MDNSWINDOWS_INSTALL_LOC, or simply clone the mDNSWindows repository at the same directory level as RDMnet to build from source.

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
3. Clone (or download and extract) the lwpa repository at the same level as the
   RDMnet repository:
   ```
   $ git clone https://github.com/ETCLabs/lwpa
   ```
4. Clone (or download and extract) the RDM repository at the same level as the
   RDMnet repository:
   ```
   $ git clone https://github.com/ETCLabs/RDM
   ```
5. Create a directory in your location of choice (a directory called 'build' at
   the repository root is recommended) to hold your build projects or
   Makefiles:
   ```
   $ mkdir build && cd build
   ```
6. Run CMake to configure the RDMnet project:
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
7. Use CMake to invoke the genreated build system to build the RDMnet library
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

8. To run or distribute the controller executable outside of Visual Studio, run
   the `windeployqt` application on the `RDMnetControllerGUI.exe` binary in its
   final location; e.g.
   ```
   %QTDIR%\bin\windeployqt RDMnetControllerGUI.exe
   ```

## Dependencies

### lwpa

RDMnet depends on the LightWeight Platform Abstraction (lwpa) library for
platform abstraction. See the
[documentation for lwpa](https://etclabs.github.io/lwpa) for details on how to
include lwpa in your project.

### RDM

RDMnet depends on the ETC's RDM library for RDM protocol support. See the
[documentation for RDM](https://etclabs.github.io/RDM) for details on how to
include RDM in your project.

### Bonjour SDK for Windows or ETCLabs/mDNSWindows

- If using the [Bonjour SDK for Windows](https://developer.apple.com/bonjour/):
  v3.0.
- If using ETCLabs/mDNSWindows: v1.1.1

### Qt

The controller depends on Qt 5.9.7 or higher. You can download the open-source
version of Qt without an account [here](https://www.qt.io/download).

### Platform Dependencies

The platform ports of RDMnet have the following dependencies:
* Windows
  + Windows XP or higher
