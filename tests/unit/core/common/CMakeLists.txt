# RDMnet core common module unit tests

rdmnet_add_unit_test(test_rdmnet_core_common
  test_common.cpp
  main.cpp

  # Source under test
  ${RDMNET_SRC}/rdmnet/core/common.c

  #Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core/client.c
  ${RDMNET_SRC}/rdmnet_mock/core/connection.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_manager.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.c
  ${RDMNET_MOCK_DISCOVERY_SOURCES}
)
target_link_libraries(test_rdmnet_core_common PRIVATE EtcPalMock RDM)
