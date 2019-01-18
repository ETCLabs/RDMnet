################################### testing ###################################

# RDMnet core library unit tests

add_executable(test_rdmnet_lib
  # RDMnet core lib unit test sources
  common_lib/main.cpp
  common_lib/test_broker_prot.cpp
)

target_include_directories(test_rdmnet_lib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src/common)
target_link_libraries(test_rdmnet_lib RDMnet gtest)

# RDMnet Broker library unit tests

add_executable(test_broker_lib
  # RDMnet Broker lib unit test sources
  broker_lib/main.cpp
  broker_lib/test_broker_uid_manager.cpp
)
target_include_directories(test_broker_lib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src/broker)
target_link_libraries(test_broker_lib Broker gtest)