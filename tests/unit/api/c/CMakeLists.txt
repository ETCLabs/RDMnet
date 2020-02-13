# RDMnet C Client API unit tests

rdmnet_add_unit_test(test_rdmnet_api
  test_rpt_client_api.cpp
  test_rpt_client_behavior.cpp

  rdmnet_client_fake_callbacks.h
  rdmnet_client_fake_callbacks.c

  main.cpp

  ${RDMNET_API_SOURCES}
  ${RDMNET_MOCK_CORE_SOURCES}
  ${RDMNET_MOCK_DISCOVERY_SOURCES}
)
target_link_libraries(test_rdmnet_api PRIVATE EtcPalMock RDM)

