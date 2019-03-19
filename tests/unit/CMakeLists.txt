################################### testing ###################################

set(RDMNET_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/../../include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR}/../../src)

# RDMnet core library unit tests

add_executable(test_rdmnet_core
  # RDMnet core lib unit test sources
  core/main.cpp
  core/test_broker_prot.cpp
)

target_include_directories(test_rdmnet_core PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core RDMnet gtest)

# RDMnet Broker library unit tests

add_executable(test_rdmnet_broker
  # RDMnet Broker lib unit test sources
  broker/main.cpp
  broker/test_broker_uid_manager.cpp
)
target_include_directories(test_rdmnet_broker PRIVATE ${RDMNET_SRC}/broker)
target_link_libraries(test_rdmnet_broker Broker gtest)

# RDMnet Discovery library unit tests

add_executable(test_rdmnet_discovery
  # RDMnet Discovery lib unit test sources
  discovery/test_discovery_bonjour.cpp

  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_include_directories(test_rdmnet_discovery PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/c_mock
)
target_link_libraries(test_rdmnet_discovery PRIVATE
  RDMnetDiscovery
  dnssd_mock
  lwpa_mock
  gtest_main
)

if(DEFINED DNS_SD_DLL)
  add_custom_command(
    TARGET test_rdmnet_discovery
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:test_rdmnet_discovery>
    COMMENT "Copying DNS-SD dll to executable directory..."
  )
endif()
