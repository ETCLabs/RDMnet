# Determines the provider of mDNS and DNS-SD support based on the compilation
# platform and user input variables.
#
# Sets the following variables which are used by source builds:
# - DNS_SD_ADDITIONAL_SOURCES: Source files to be added to the RDMnet library compilation
# - DNS_SD_ADDITIONAL_INCLUDE_DIRS: Include directories to be added to the RDMnet library compilation
# - DNS_SD_ADDITIONAL_LIBS: Static libs to be added to the RDMnet library compilation

# The imported DNS-SD library. This will have its properties set by the various options below.
add_library(dnssd INTERFACE)

add_library(RDMnetDiscovery INTERFACE)
target_sources(RDMnetDiscovery INTERFACE
  ${RDMNET_INCLUDE}/rdmnet/core/discovery.h

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
target_include_directories(RDMnetDiscovery INTERFACE
  ${RDMNET_INCLUDE}
  ${RDMNET_SRC}
  ${RDMNET_SRC}/rdmnet/discovery
)

if(WIN32)
  include(${RDMNET_ROOT}/cmake/dnssd/windows.cmake)
elseif(APPLE)
  include(dnssd/macos.cmake)
elseif(UNIX)
  include(dnssd/unix.cmake)
else()
  message(FATAL_ERROR "There is currently no DNS-SD provider supported for this platform.")
endif()

target_sources(RDMnetDiscovery INTERFACE ${RDMNET_DISCOVERY_ADDITIONAL_SOURCES})
target_include_directories(RDMnetDiscovery INTERFACE ${RDMNET_DISCOVERY_ADDITIONAL_INCLUDE_DIRS})
