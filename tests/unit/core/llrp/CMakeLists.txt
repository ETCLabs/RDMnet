# Unit tests for the RDMnet LLRP module

add_executable(test_llrp
  # RDMnet LLRP unit test sources
  test_target.cpp
  main.cpp

  # Sources under test
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet/core/llrp_manager.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core.c

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/util.c
)

target_include_directories(test_llrp PRIVATE ${RDMNET_INCLUDE} ${RDMNET_SRC})
target_link_libraries(test_llrp EtcPalMock RDM gtest)
set_target_properties(test_llrp PROPERTIES CXX_STANDARD 11 FOLDER tests)
gtest_add_tests(TARGET test_llrp SKIP_DEPENDENCY)
