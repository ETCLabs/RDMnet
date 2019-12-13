# Building and Integrating the RDMnet Library into Your Project         {#building_and_integrating}

## Prerequisites

* **CMake >= 3.8**. CMake is an industry-standard cross-platform build system generator for C and
  C++. CMake can be downloaded [here](https://cmake.org/download). It is also available as a
  package in many Linux distributions.

* **An implementation of DNS-SD/mDNS**.
  There are different options for this on different platforms:
  + On Microsoft Windows:
    - Apple's Bonjour for Windows implements DNS-SD/mDNS. Note that you must have an Apple
      developer account to download the Bonjour SDK for Windows, and that bundling Bonjour with a
      Windows application may be subject to additional licensing restrictions from Apple. The SDK
      is available [here](https://developer.apple.com/bonjour/). Define
      RDMNET_WINDOWS_USE_BONJOUR_SDK=ON at configure time to use the Bonjour SDK.
    - ETC's fork of Bonjour for Windows ("mDNSWindows"), maintained
      [here](https://github.com/ETCLabs/mDNSWindows). ETC maintains a fork of the Apache-licensed
      Bonjour code which can be used as a DNS-SD/mDNS provider on Windows. The RDMnet CMake config
      will automatically download the latest version if no other options are specified. Or, to
      build mDNSWindows from source, specify the source location with the MDNSWINDOWS_SRC_LOC
      option.
  + On macOS:
    - RDMnet uses native Bonjour, which comes with every macOS distribution.

## Including RDMnet in your project

When adding RDMnet to your project, you can choose whether to use RDMnet in binary or source form.
RDMnet publishes a small set of built binaries for specific platforms and toolchains to
[JFrog Bintray](https://bintray.com/beta/#/etclabs/rdmnet_bin?tab=packages).

### Including RDMnet in CMake projects

#### Binaries

RDMnet binary packages come with exported CMake config files which allow them to be added using
`find_package()`. Once the binary package has been downloaded, you can include it in your project
by adding:

```cmake
# This line may not be necessary on Unix-like platforms if RDMnet is installed in your standard
# system paths
list(APPEND CMAKE_PREFIX_PATH ${PATH_TO_RDMNET_BINARY_PACKAGE})
find_package(RDMnet 0.3.0 REQUIRED)
# ...
target_link_libraries(MyApp PRIVATE RDMnet::RDMnet)
# Or for an app that needs a broker implementation:
target_link_libraries(MyApp PRIVATE RDMnet::Broker) # RDMnet will be linked transitively
```

#### Source

To include RDMnet as a source dependency from a CMake project, use the `add_subdirectory()`
command, specifying the root of the RDMnet repository, and use `target_link_libraries()` to add the
relevant RDMnet include paths and binaries to your project settings.

```cmake
add_subdirectory(path/to/RDMnet/root)
# ...
target_link_libraries(MyApp PRIVATE RDMnet)
# Or for an app that needs a broker implementation:
target_link_libraries(MyApp PRIVATE RDMnetBroker) # RDMnet will be linked transitively
```

### Including RDMnet in non-CMake projects

## Building RDMnet on its own

If you just want to play around with RDMnet, you can build the library standalone - in this
configuration, you can set some CMake options to build extras like the unit tests and examples:
* `RDMNET_BUILD_TESTS`: Build the unit tests
* `RDMNET_BUILD_CONSOLE_EXAMPLES`: Build the console example applications
* `RDMNET_BUILD_GUI_EXAMPLES`: Build the controller GUI example
* `RDMNET_BUILD_TEST_TOOLS`: Build the library test tools

If you want to build the controller GUI example application, you will need another prerequisite:
Qt (>=5.9.7 open-source). Qt installers are available [here](https://www.qt.io/download). To point
CMake at the Qt dependency, there are two options:
  + Set a system environment variable called QTDIR which points at the Qt installation directory
    (i.e. .../Qt/5.9.7/msvc2017_64)
  + Provide the Qt installation directory as an argument to CMake:
    ```
    cmake -DQTDIR=[...]/Qt/5.9.7/msvc2017_64
    ```

To configure and build RDMnet on its own using command-line CMake, follow these steps:

1. Clone (or download and extract) the RDMnet repository:
   ```
   $ git clone https://github.com/ETCLabs/RDMnet
   ```
2. Create a directory in your location of choice (a directory called 'build' at the repository root
   is recommended) to hold your build projects or Makefiles:
   ```
   $ mkdir build && cd build
   ```
3. Run CMake to configure the RDMnet project:
   ```
   $ cmake .. # [or cmake path/to/RDMnet/root as applicable]
   ```
   You can optionally specify your build system with the `-G` option; otherwise, CMake will choose
   a system-appropriate default. Use `cmake --help` to see all available options.

   Use -D to enable the building of any relevant extras that you want:
   ```
   $ cmake -DRDMNET_BUILD_GUI_EXAMPLES=ON .. # Include the Qt example app
   $ cmake -DRDMNET_BUILD_TESTS=ON .. # Include the unit tests
   ```
   etc..
4. Use CMake to invoke the generated build system to build the RDMnet library and any extras you
   have enabled:
   ```
   $ cmake --build .
   ```
   If you are generating IDE project files, you can use CMake to open the projects in the IDE:
   ```
   $ cmake --open .
   ```

**Note:** Steps 2 through 4 can also be done in the CMake GUI application, or via a CMake plugin to
your favorite IDE.

### Installation

5. To run or distribute the example applications outside of a debugging context, use CMake's 
   installation target. This usually shows up as another project called "INSTALL" inside an IDE or
   a target called "install" (e.g. `make install` for a Makefile generator). You can also do it
   manually from the command line in the build directory:
   ```
   $ cmake -P cmake_install.cmake
   ```
   Binaries will be installed in a directory underneath `CMAKE_INSTALL_PREFIX`, which normally
   defaults to a system directory. To change this, specify CMAKE_INSTALL_PREFIX explicitly when
   configuring the project.
