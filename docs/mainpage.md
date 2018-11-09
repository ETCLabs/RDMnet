# RDMnet                                                            {#mainpage}

## Important Note: Read Me First

THIS SOFTWARE IMPLEMENTS A **DRAFT** STANDARD, BSR E1.33 rev. 63. **UNDER NO
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

* **For controller, broker and device: Bonjour SDK for Windows v3.0**. Apple's
  Bonjour service implements RDMnet's discovery mechanism (mDNS/DNS-SD). Note
  that you must have an Apple developer account to download the Bonjour SDK for
  Windows. It is available [here](https://developer.apple.com/bonjour/).
  Installation of the Bonjour SDK will set a system environment variable which
  will enable the RDMnet projects to find the proper headers and libraries.

* **For controller: Qt (>= 5.9.7) open-source**.  Qt installers are available
  [here](https://www.qt.io/download).

Steps to build the Windows RDMnet prototypes:

1. Clone (or download and extract) the RDMnet repository:
   ```
   git clone https://github.com/ETCLabs/RDMnet
   ```
2. Clone (or download and extract) the lwpa repository at the same level as the
   RDMnet repository:
   ```
   git clone https://github.com/ETCLabs/lwpa
   ```
3. Clone (or download and extract) the RDM repository at the same level as the
   RDMnet repository:
   ```
   git clone https://github.com/ETCLabs/RDM
   ```

#### To build the Device:

4. Open the Device solution (`RDMnet/apps/windows/device/msvc2015/device.sln`)
5. Build the solution in your configuration of choice (Build -> Build Solution)

#### To build the %Broker:

4. Open the %Broker solution (`RDMnet/apps/windows/broker/msvc2015/broker.sln`)
5. Build the solution in your configuration of choice (Build -> Build Solution)

#### To build the LLRP Manager:

4. Open the LLRP Manager solution (`RDMnet/apps/windows/llrp_manager/msvc2015/manager.sln`)
5. Build the solution in your configuration of choice (Build -> Build Solution)

#### To build the Controller:

3. To satisfy the Controller's Qt dependency, there are two options:
   + Set a system environment variable called QTDIR which points at the Qt
     installation directory (i.e. .../Qt/5.9.7/msvc2017_64)
   + Provide the Qt installation directory as an argument to CMake:
     ```
     cmake -DQTDIR=[...]/Qt/5.9.7/msvc2017_64
     ```
4. Open the Controller solution (`RDMnet/apps/windows/controller/msvc2015/controller.sln`)
5. Build the solution in your configuration of choice (Build -> Build Solution)
6. To run or distribute the executable outside of Visual Studio, run the
   `windeployqt` application on the `RDMnetControllerGUI.exe` binary in its
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

### Bonjour SDK for Windows

The Windows versions of the broker, controller and device applications depend
on the [Bonjour SDK for Windows](https://developer.apple.com/bonjour/) v3.0.
You must have an Apple developer account to download the Bonjour SDK for
Windows.

### Qt

The controller depends on Qt 5.9.1. You can download the open-source version of
Qt without an account [here](https://www.qt.io/download).

### Platform Dependencies

The platform ports of RDMnet have the following dependencies:
* Windows
  + Windows XP or higher
