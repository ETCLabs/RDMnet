# On Apple platforms, use native Bonjour. Nothing needs to be added to the
# linker line to use Bonjour.

add_library(RDMnetDiscoveryPlatform INTERFACE)
target_sources(RDMnetDiscoveryPlatform INTERFACE
  ${RDMNET_SRC}/rdmnet/discovery/bonjour/disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/discovery/bonjour/rdmnet_disc_bonjour.c
)
target_include_directories(RDMnetDiscoveryPlatform INTERFACE
  ${RDMNET_SRC}/rdmnet/discovery/bonjour
)
