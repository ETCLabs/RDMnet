# Determines the provider of mDNS and DNS-SD support based on the compilation
# platform and user input variables.
#
# Adds the following targets which are used by source builds:
# - dnssd: Platform DNS-SD library (if any)
# - RDMnetDiscoveryCommon: Interface library containing only the platform-neutral discovery sources.
# - RDMnetDiscoveryPlatform: Interface library containing the platform-specific discovery sources.

# The imported DNS-SD library. This will have its properties set in the platform scripts
# (dnssd/[option].cmake)
add_library(dnssd INTERFACE)

# The platform-neutral portion of the discovery library.
set(RDMNET_DISC_PUBLIC_HEADERS
  ${RDMNET_INCLUDE}/rdmnet/discovery.h
)
set(RDMNET_DISC_COMMON_SOURCES
  ${RDMNET_SRC}/rdmnet/discovery/disc_common.h
  ${RDMNET_SRC}/rdmnet/discovery/disc_platform_api.h
  ${RDMNET_SRC}/rdmnet/discovery/discovered_broker.h
  ${RDMNET_SRC}/rdmnet/discovery/monitored_scope.h
  ${RDMNET_SRC}/rdmnet/discovery/registered_broker.h

  ${RDMNET_SRC}/rdmnet/discovery/disc_common.c
  ${RDMNET_SRC}/rdmnet/discovery/discovered_broker.c
  ${RDMNET_SRC}/rdmnet/discovery/monitored_scope.c
  ${RDMNET_SRC}/rdmnet/discovery/registered_broker.c
)
add_library(RDMnetDiscoveryCommon INTERFACE)
target_sources(RDMnetDiscoveryCommon INTERFACE
  ${RDMNET_DISC_PUBLIC_HEADERS}
  ${RDMNET_DISC_COMMON_SOURCES}
)
target_include_directories(RDMnetDiscoveryCommon INTERFACE
  ${RDMNET_INCLUDE}
  ${RDMNET_SRC}
  ${RDMNET_SRC}/rdmnet/discovery
)

# Include the platform-specific configuration based on our target platform.

option(RDMNET_FORCE_LIGHTWEIGHT_DNS_QUERIER OFF)

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
