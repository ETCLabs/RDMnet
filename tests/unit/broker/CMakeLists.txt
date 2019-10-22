# RDMnet Broker library unit tests

add_executable(test_rdmnet_broker
  # RDMnet Broker lib unit test sources
  main.cpp
  test_broker_core.cpp
  test_broker_discovery.cpp
  # test_broker_log.cpp
  test_broker_threads.cpp
  test_broker_uid_manager.cpp
  test_broker_settings.cpp
)
target_include_directories(test_rdmnet_broker PRIVATE ${RDMNET_SRC}/rdmnet/broker)
target_link_libraries(test_rdmnet_broker PRIVATE
  RDMnetBrokerCore
  gmock
  RDMnetCoreMock
  RDMnetDiscoveryMock
  EtcPalMock
  RDMMock
)
set_target_properties(test_rdmnet_broker PROPERTIES CXX_STANDARD 14 FOLDER tests)
gtest_add_tests(TARGET test_rdmnet_broker SKIP_DEPENDENCY)
