# Determines the provider of mDNS and DNS-SD support based on the compilation
# platform and user input variables.
#
# Platform-specific configuration sets the following variables:
#   RDMNET_DISC_PLATFORM_SOURCES: Additional sources to compile to interface with the platform DNS-SD.
#   RDMNET_DISC_PLATFORM_INCLUDE_DIRS: Additional include directories necessary to bring in platform DNS-SD headers.
#   RDMNET_DISC_PLATFORM_LIBS: Additional DNS-SD libraries or CMake targets to link RDMnet to.
#   RDMNET_DISC_PLATFORM_DEPENDENCIES: Targets that RDMnet should be made dependent on for DNS-SD, e.g. building a DLL.

# The Windows config also sets the special variable:
#   RDMNET_BONJOUR_MOCK_LIB: The target representing the Bonjour mocking library.

# The platform-neutral portion of the discovery library.
set(RDMNET_DISC_PUBLIC_HEADERS
  ${RDMNET_INCLUDE}/rdmnet/discovery.h
)
set(RDMNET_DISC_COMMON_SOURCES
  ${RDMNET_SRC}/rdmnet/disc/common.h
  ${RDMNET_SRC}/rdmnet/disc/platform_api.h
  ${RDMNET_SRC}/rdmnet/disc/discovered_broker.h
  ${RDMNET_SRC}/rdmnet/disc/monitored_scope.h
  ${RDMNET_SRC}/rdmnet/disc/registered_broker.h

  ${RDMNET_SRC}/rdmnet/disc/common.c
  ${RDMNET_SRC}/rdmnet/disc/discovered_broker.c
  ${RDMNET_SRC}/rdmnet/disc/monitored_scope.c
  ${RDMNET_SRC}/rdmnet/disc/registered_broker.c
)

# Include the platform-specific configuration based on our target platform.

option(RDMNET_FORCE_LIGHTWEIGHT_DNS_QUERIER 
  "Use RDMnet's built-in lightweight mDNS querier even when targeting systems that have other mDNS solutions"
  OFF
)

if(NOT ${RDMNET_FORCE_LIGHTWEIGHT_DNS_QUERIER})
  if(WIN32)
    include(${RDMNET_ROOT}/cmake/dnssd/windows.cmake)
  elseif(APPLE)
    include(${RDMNET_ROOT}/cmake/dnssd/macos.cmake)
  elseif(UNIX)
    include(${RDMNET_ROOT}/cmake/dnssd/unix.cmake)
  else()
    include(${RDMNET_ROOT}/cmake/dnssd/lightweight.cmake)
  endif()
else()
  include(${RDMNET_ROOT}/cmake/dnssd/lightweight.cmake)
endif()
