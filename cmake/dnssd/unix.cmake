# On Unix-like platforms, use Avahi.

set(RDMNET_DISC_PLATFORM_SOURCES
  ${RDMNET_SRC}/rdmnet/disc/avahi/rdmnet_disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/disc/avahi/rdmnet_disc_avahi.c
)
set(RDMNET_DISC_PLATFORM_INCLUDE_DIRS ${RDMNET_SRC}/rdmnet/disc/avahi)
set(RDMNET_DISC_PLATFORM_LIBS avahi-client avahi-common)
