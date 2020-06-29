# Unit tests for the RDMnet Core Connection module

rdmnet_add_unit_test(test_rdmnet_core_connection
  # RDMnet connection unit test sources
  test_connection.cpp
  main.cpp

  # Source under test
  ${RDMNET_SRC}/rdmnet/core/connection.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet_mock/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet_mock/core/message.c
  ${RDMNET_SRC}/rdmnet_mock/core/msg_buf.c
  ${RDMNET_MOCK_DISCOVERY_SOURCES}

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_link_libraries(test_rdmnet_core_connection PRIVATE EtcPalMock RDM)
