# RDMnet C++ API unit tests

rdmnet_add_unit_test(test_rdmnet_cpp_api
  test_client.cpp
  test_controller.cpp
  test_device.cpp
  test_ept_client.cpp
  test_llrp_manager.cpp

  test_message_dynamic_uid.cpp
  test_message_ept_client.cpp
  test_message_ept_data.cpp
  test_message_ept_status.cpp
  test_message_llrp_rdm_command.cpp
  test_message_llrp_rdm_response.cpp
  test_message_rdm_command.cpp
  test_message_rdm_response.cpp
  test_message_rpt_client.cpp
  test_message_rpt_status.cpp

  main.cpp

  ${RDMNET_MOCK_API_SOURCES}
  # ${RDMNET_MOCK_DISCOVERY_SOURCES}
)
target_link_libraries(test_rdmnet_cpp_api PRIVATE EtcPalMock RDM)
