# RDMnet Broker library unit tests

add_executable(test_rdmnet_broker
  # RDMnet Broker lib unit test sources
  main.cpp
  test_broker_uid_manager.cpp
)
target_include_directories(test_rdmnet_broker PRIVATE ${RDMNET_SRC}/rdmnet/broker)
target_link_libraries(test_rdmnet_broker Broker gtest)
gtest_add_tests(TARGET test_rdmnet_broker)
