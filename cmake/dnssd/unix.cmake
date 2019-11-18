# On Unix-like platforms, use Avahi.
add_library(RDMnetDiscoveryPlatform INTERFACE)
target_sources(RDMnetDiscoveryPlatform INTERFACE
  ${RDMNET_SRC}/rdmnet/discovery/avahi/disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/discovery/avahi/rdmnet_disc_avahi.c
)
target_include_directories(RDMnetDiscoveryPlatform INTERFACE
  ${RDMNET_SRC}/rdmnet/discovery/avahi
)
target_link_libraries(dnssd INTERFACE avahi-client avahi-common)
