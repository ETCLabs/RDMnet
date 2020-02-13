# RDMnet Broker library unit tests

rdmnet_add_unit_test(test_rdmnet_broker
  # RDMnet Broker lib unit test sources
  main.cpp
  test_broker_core.cpp
  test_broker_discovery.cpp
  test_broker_threads.cpp
  test_broker_uid_manager.cpp
  test_broker_settings.cpp

  ${RDMNET_BROKER_SOURCES}
  ${RDMNET_MOCK_ALL_SOURCES}
)
target_include_directories(test_rdmnet_broker PRIVATE ${RDMNET_SRC}/rdmnet/broker)
target_link_libraries(test_rdmnet_broker PRIVATE EtcPalMock RDMMock)
