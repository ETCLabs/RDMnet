# Unit tests for the RDMnet connection module

add_executable(test_rdmnet_connection
  # RDMnet connection unit test sources
  test_connection.cpp
  main.cpp

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core.c
)

target_include_directories(test_rdmnet_core_support_modules PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core_support_modules RDMnetCore RDMnetDiscovery_mock lwpa_mock RDM gtest)
gtest_add_tests(TARGET test_rdmnet_core_support_modules)
