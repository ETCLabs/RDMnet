# Unit tests for the RDMnet Core LLRP modules (including the common LLRP
# definitions, LLRP Target module and LLRP protocol module).

rdmnet_add_unit_test(test_rdmnet_core_llrp
  # RDMnet LLRP unit test sources
  ${UNIT_TEST_DIR}/test_shared/fake_mcast.h
  ${UNIT_TEST_DIR}/test_shared/fake_mcast.cpp
  test_llrp_manager.cpp
  test_llrp_target.cpp
  main.cpp

  # Sources under test
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/llrp_manager.c
  ${RDMNET_SRC}/rdmnet/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_link_libraries(test_rdmnet_core_llrp PRIVATE EtcPalMock RDM)
