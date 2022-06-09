# RDMnet Core client module unit tests

rdmnet_add_unit_test(test_rdmnet_core_client
  test_rpt_client_api.cpp
  test_rpt_client_connection_handling.cpp
  test_rpt_client_rdm_handling.cpp

  rdmnet_client_fake_callbacks.h
  rdmnet_client_fake_callbacks.c

  ${UNIT_TEST_DIR}/shared/test_rdm_commands.h

  main.cpp

  # Source under test
  ${RDMNET_SRC}/rdmnet/core/client.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet_mock/core/connection.c
  ${RDMNET_SRC}/rdmnet_mock/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet_mock/core/rpt_prot.c
  ${RDMNET_MOCK_DISCOVERY_SOURCES}

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_link_libraries(test_rdmnet_core_client PRIVATE EtcPalMock RDM)

