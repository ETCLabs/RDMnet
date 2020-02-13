# Unit tests for the RDMnet connection module

rdmnet_add_unit_test(test_rdmnet_connection
  # RDMnet connection unit test sources
  test_connection.cpp
  main.cpp

  # Source under test
  ${RDMNET_SRC}/rdmnet/core/connection.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_MOCK_DISCOVERY_SOURCES}

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_link_libraries(test_rdmnet_connection PRIVATE EtcPalMock RDM)
