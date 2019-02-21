################################### testing ###################################

# RDMnet core library unit tests

add_executable(test_rdmnet_core
  # RDMnet core lib unit test sources
  core/main.cpp
  core/test_broker_prot.cpp
)

target_include_directories(test_rdmnet_core PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src)
target_link_libraries(test_rdmnet_core RDMnet gtest)

# RDMnet Broker library unit tests

add_executable(test_rdmnet_broker
  # RDMnet Broker lib unit test sources
  broker/main.cpp
  broker/test_broker_uid_manager.cpp
)
target_include_directories(test_rdmnet_broker PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../src/broker)
target_link_libraries(test_rdmnet_broker Broker gtest)

# RDMnet Discovery library unit tests

add_executable(test_rdmnet_discovery
  # RDMnet Discovery lib unit test sources
  discovery/test_discovery_bonjour.cpp
)
target_include_directories(test_rdmnet_discovery PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/../../src
  ${CMAKE_CURRENT_LIST_DIR}/c_mock
)
target_link_libraries(test_rdmnet_discovery RDMnet gtest_main)
