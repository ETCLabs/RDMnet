# RDMnet core library unit tests

add_executable(test_rdmnet_core
  # RDMnet core lib unit test sources
  test_broker_prot.cpp
  test_msg_buf.cpp
)

target_include_directories(test_rdmnet_core PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core RDMnetCore RDMnetDiscovery_mock lwpa_mock RDM gtest_main)
gtest_add_tests(TARGET test_rdmnet_core)
