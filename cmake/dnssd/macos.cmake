# On Apple platforms, use native Bonjour. The 
set(RDMNET_DISCOVERY_ADDITIONAL_SOURCES
  ${RDMNET_SRC}/rdmnet/discovery/bonjour/disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/discovery/bonjour/rdmnet_disc_bonjour.c
)
set(RDMNET_DISCOVERY_ADDITIONAL_INCLUDE_DIRS
  ${RDMNET_SRC}/rdmnet/discovery/bonjour
)
