# Building and Integrating the RDMnet Library into Your Project {#building_and_integrating}

## Building RDMnet for Your Platform

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

### Controller only:

6. To run or distribute the controller executable outside of Visual Studio, run
   the `windeployqt` application on the `RDMnetControllerGUI.exe` binary in its
   final location; e.g.
   ```
   %QTDIR%\bin\windeployqt RDMnetControllerGUI.exe
   ```

