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

## Dependencies

RDMnet depends on two other ETC libraries, [RDM](https://github.com/ETCLabs/RDM) and 
[EtcPal](https://github.com/ETCLabs/EtcPal). By default, these libraries are included in source
form as git submodules in the RDMnet repository. CMake will automatically pull the submodules when
building, so no other action is needed to build RDMnet in the default configuration.

If you are using other libraries that have these dependencies (like ETC's
[sACN](https://github.com/ETCLabs/sACN) library, which also depends on EtcPal, for example), you
can make sure they are using the same version of the dependencies by cloning all of the libraries
and dependencies at the same directory level, e.g.:

```
|- external/
|--- EtcPal/
|--- RDM/
|--- RDMnet/
|--- sACN/
```

The CMake configurations for these libraries will automatically check for their dependencies at
this level before using submodules. You can disable this behavior by defining the CMake option
`RDMNET_FORCE_SUBMODULE_DEPS` to ON.

## Including RDMnet in your project

When adding RDMnet to your project, you can choose whether to use RDMnet in binary or source form.
RDMnet publishes a small set of built binaries for specific platforms and toolchains to its
[Releases page](https://github.com/ETCLabs/RDMnet/releases).

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

RDMnet can be built on its own using CMake and its headers and binaries can be installed for
inclusion in a non-CMake project. Typical practice is to create a clean directory to hold the build
results named some varion of "build".

**NOTE**: If you are cross-compiling and/or building for an embedded target, some additional
configuration is necessary. EtcPal helps make this possible; see the 
[EtcPal embedded build documentation](https://etclabs.github.io/EtcPalDocs/head/building_for_embedded.html)
for more details.

1. Create a directory in your location of choice (a directory called 'build' at the repository root
   is recommended) to hold your build projects or Makefiles:
   ```
   $ mkdir build && cd build
   ```
2. Run CMake to configure the RDMnet project:
   ```
   $ cmake -DCMAKE_INSTALL_PREFIX=[path/to/install/location] [path/to/RDMnet/root]
   ```
   You can optionally specify your build system with the `-G` option; otherwise, CMake will choose
   a system-appropriate default. Use `cmake --help` to see all available options. CMake also has a
   GUI tool that can be used for this, as well as plugins available for several editors and IDEs.
   `CMAKE_INSTALL_PREFIX` specifies where the final binaries and headers will go; if not given,
   they will be installed in a system-appropriate place like `/usr/local/include` and
   `/usr/local/lib` on a *nix system.
3. Use CMake to invoke the generated build system to build the RDMnet library and any extras you
   have enabled:
   ```
   $ cmake --build .
   ```
   If you are generating IDE project files, you can use CMake to open the projects in the IDE:
   ```
   $ cmake --open .
   ```
4. Use CMake's installation target to install the built binaries and headers. This usually shows up
   as another project called "INSTALL" inside an IDE or a target called "install" (e.g.
   `make install` for a Makefile generator). You can also do it manually from the command line in
   the build directory:
   ```
   $ cmake -P cmake_install.cmake
   ```

### Building RDMnet on its own

If you want to build the RDMnet library on its own, in order to tweak the examples or contribute
changes, follow the instructions for "Including RDMnet in non-CMake projects" above. Additionally,
you can set some CMake options to build extras like the unit tests and examples:
* `RDMNET_BUILD_TESTS`: Build the unit tests
* `RDMNET_BUILD_CONSOLE_EXAMPLES`: Build the console example applications
* `RDMNET_BUILD_GUI_EXAMPLES`: Build the controller GUI example
* `RDMNET_BUILD_TEST_TOOLS`: Build the library test tools

These can be specified using the CMake GUI tool or at the command line using `-D`:
```
$ cmake -DRDMNET_BUILD_GUI_EXAMPLES=ON [...] # Include the Qt example app
$ cmake -DRDMNET_BUILD_TESTS=ON [...] # Include the unit tests
```

If you want to build the GUI example applications, you will need another prerequisite:
Qt (>=5.9.7 open-source). Qt installers are available [here](https://www.qt.io/download). To point
CMake at the Qt dependency, there are two options:
  + Set a system environment variable called QTDIR which points at the Qt installation directory
    (i.e. .../Qt/5.9.7/msvc2017_64)
  + Provide the Qt installation directory as an argument to CMake:
    ```
    cmake -DQTDIR=[...]/Qt/5.9.7/msvc2017_64
    ```
