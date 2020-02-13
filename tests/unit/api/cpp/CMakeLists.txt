# RDMnet C++ Client API unit tests

rdmnet_add_unit_test(test_rdmnet_cpp_api EXCLUDE_FROM_ALL
  test_controller.cpp
  main.cpp

  ${RDMNET_MOCK_API_SOURCES}
  ${RDMNET_MOCK_DISCOVERY_SOURCES}
)
target_link_libraries(test_rdmnet_cpp_api PRIVATE EtcPalMock RDM)
