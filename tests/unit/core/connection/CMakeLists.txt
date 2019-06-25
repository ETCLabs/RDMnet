# Unit tests for the RDMnet connection module

add_executable(test_rdmnet_connection
  # RDMnet connection unit test sources
  test_connection.cpp
  main.cpp

  # Source under test
  ${RDMNET_SRC}/rdmnet/core/connection.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core.c
  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)

target_include_directories(test_rdmnet_connection PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_connection RDMnetDiscovery_mock lwpa_mock RDM gtest)
set_target_properties(test_rdmnet_connection PROPERTIES CXX_STANDARD 11)
gtest_add_tests(TARGET test_rdmnet_connection)
