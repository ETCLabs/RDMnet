# RDMnet core library unit tests

add_executable(test_rdmnet_core
  # RDMnet core lib unit test sources
  main.cpp
  test_broker_prot.cpp
)

target_include_directories(test_rdmnet_core PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core RDMnet gtest)
gtest_add_tests(TARGET test_rdmnet_core)
