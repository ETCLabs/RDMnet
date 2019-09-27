# Unit tests for the stateless support modules

add_executable(test_rdmnet_core_support_modules
  # RDMnet core support modules unit test sources
  test_broker_prot.cpp
  test_msg_buf.cpp
  main.cpp
)

target_include_directories(test_rdmnet_core_support_modules PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core_support_modules PRIVATE RDMnetCore RDMnetDiscoveryMock RDM gtest)
set_target_properties(test_rdmnet_core_support_modules PROPERTIES CXX_STANDARD 11 FOLDER tests)
gtest_add_tests(TARGET test_rdmnet_core_support_modules SKIP_DEPENDENCY)
