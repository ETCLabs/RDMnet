# RDMnet API layer unit tests

add_executable(test_rdmnet_api
  test_rpt_client_api.cpp
  test_rpt_client_behavior.cpp

  rdmnet_client_fake_callbacks.h
  rdmnet_client_fake_callbacks.c

  main.cpp
)
target_link_libraries(test_rdmnet_api PRIVATE
  RDMnetApi
  RDMnetCoreMock
  EtcPalMock
  RDM
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_rdmnet_api PROPERTIES CXX_STANDARD 11 FOLDER tests)
gtest_add_tests(TARGET test_rdmnet_api SKIP_DEPENDENCY)

