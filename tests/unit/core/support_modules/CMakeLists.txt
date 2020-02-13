# Unit tests for the stateless support modules

rdmnet_add_unit_test(test_rdmnet_core_support_modules
  # RDMnet core support modules unit test sources
  test_broker_prot.cpp
  test_mcast.cpp
  test_msg_buf.cpp
  main.cpp

  ${RDMNET_CORE_SOURCES}
  ${RDMNET_MOCK_DISCOVERY_SOURCES}
)

target_include_directories(test_rdmnet_core_support_modules PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core_support_modules PRIVATE
  test_data
  RDMMock
  EtcPalMock
)
