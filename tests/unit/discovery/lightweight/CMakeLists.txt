# RDMnet Lightweight Discovery library unit tests

rdmnet_add_unit_test(test_discovery_lightweight
  test_lwmdns_message.cpp
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c
)
target_link_libraries(test_discovery_lightweight PRIVATE
  RDMnetDiscoveryCommon
  RDMnetDiscoveryPlatform
  EtcPalMock
  RDM
)
