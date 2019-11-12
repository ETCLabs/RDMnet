  # On Unix-like platforms, use Avahi.
  set(RDMNET_DISCOVERY_ADDITIONAL_SOURCES
    ${RDMNET_SRC}/rdmnet/discovery/avahi/disc_platform_defs.h
    ${RDMNET_SRC}/rdmnet/discovery/avahi/rdmnet_disc_avahi.c
  )
  set(RDMNET_DISCOVERY_ADDITIONAL_INCLUDE_DIRS
    ${RDMNET_SRC}/rdmnet/discovery/avahi
  )
  target_link_libraries(dnssd INTERFACE avahi-client avahi-common)
