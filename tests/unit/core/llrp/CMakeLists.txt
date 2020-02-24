# Unit tests for the RDMnet LLRP module

rdmnet_add_unit_test(test_llrp
  # RDMnet LLRP unit test sources
  test_target.cpp
  main.cpp

  # Sources under test
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_link_libraries(test_llrp PRIVATE EtcPalMock RDM)
