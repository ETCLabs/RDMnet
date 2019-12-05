# Use the lightweight mDNS querier built into the RDMnet library. This discovery provider only
# supports broker discovery, not broker registration.

set(RDMNET_DISC_PLATFORM_SOURCES
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/rdmnet_disc_lightweight.c
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/lwmdns.h
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/lwmdns_message.h
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/lwmdns_message.c
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/lwmdns_recv.h
  ${RDMNET_SRC}/rdmnet/discovery/lightweight/lwmdns_recv.c
)
add_library(RDMnetDiscoveryPlatform INTERFACE)
target_sources(RDMnetDiscoveryPlatform INTERFACE ${RDMNET_DISC_PLATFORM_SOURCES})
target_include_directories(RDMnetDiscoveryPlatform INTERFACE
  ${RDMNET_SRC}/rdmnet/discovery/lightweight
)
