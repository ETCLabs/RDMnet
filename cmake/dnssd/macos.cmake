# On Apple platforms, use native Bonjour. Nothing needs to be added to the
# linker line to use Bonjour.

set(RDMNET_DISC_PLATFORM_SOURCES
  ${RDMNET_SRC}/rdmnet/disc/bonjour/rdmnet_disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/disc/bonjour/rdmnet_disc_bonjour.c
)
set(RDMNET_DISC_PLATFORM_INCLUDE_DIRS ${RDMNET_SRC}/rdmnet/disc/bonjour)
