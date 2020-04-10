# RDMnet C API unit tests

rdmnet_add_unit_test(test_rdmnet_c_api
  main.cpp
  test_client.cpp
  test_controller.cpp
  test_device.cpp
  test_ept_client.cpp
  test_llrp.cpp
  test_llrp_manager.cpp
  test_llrp_target.cpp
  test_message.cpp

  ${RDMNET_API_SOURCES}
)
target_link_libraries(test_rdmnet_c_api PRIVATE EtcPalMock RDM)
