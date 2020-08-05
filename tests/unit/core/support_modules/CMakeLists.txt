# Unit tests for the stateless support modules

rdmnet_add_unit_test(test_rdmnet_core_support_modules
  # RDMnet core support modules unit test sources
  test_broker_prot.cpp
  test_mcast.cpp
  test_msg_buf.cpp
  test_rpt_prot.cpp
  main.cpp

  # Sources under test
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/mcast.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.c
  ${RDMNET_SRC}/rdmnet/core/util.c

  # Real dependencies
  ${RDMNET_SRC}/rdmnet/core/message.c

  # Mock dependencies
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_MOCK_DISCOVERY_SOURCES}
)

if(RDMNET_BUILD_TESTS_STATIC)
  target_sources(test_rdmnet_core_support_modules PRIVATE
    test_buf_static.cpp
    test_ref_lists_static.cpp
  )
else()
  target_sources(test_rdmnet_core_support_modules PRIVATE
    test_buf_dynamic.cpp
    test_ref_lists_dynamic.cpp
  )
endif()

target_include_directories(test_rdmnet_core_support_modules PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_core_support_modules PRIVATE
  test_data
  RDMMock
  EtcPalMock
)
