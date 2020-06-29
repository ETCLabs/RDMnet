# Use the lightweight mDNS querier built into the RDMnet library. This discovery provider only
# supports broker discovery, not broker registration.

set(RDMNET_DISC_PLATFORM_SOURCES
  ${RDMNET_SRC}/rdmnet/disc/lightweight/rdmnet_disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/disc/lightweight/rdmnet_disc_lightweight.c
  ${RDMNET_SRC}/rdmnet/disc/lightweight/lwmdns_common.h
  ${RDMNET_SRC}/rdmnet/disc/lightweight/lwmdns_common.c
  ${RDMNET_SRC}/rdmnet/disc/lightweight/lwmdns_recv.h
  ${RDMNET_SRC}/rdmnet/disc/lightweight/lwmdns_recv.c
  ${RDMNET_SRC}/rdmnet/disc/lightweight/lwmdns_send.h
  ${RDMNET_SRC}/rdmnet/disc/lightweight/lwmdns_send.c
)
set(RDMNET_DISC_PLATFORM_INCLUDE_DIRS ${RDMNET_SRC}/rdmnet/disc/lightweight)
