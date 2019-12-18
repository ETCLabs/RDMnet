# RDMnet C++ Client API unit tests

add_executable(test_rdmnet_cpp_api
  test_controller.cpp

  main.cpp
)
target_link_libraries(test_rdmnet_cpp_api
  RDMnetCoreMock
  RDMnetDiscoveryMock
  EtcPalMock
  RDM
  gmock
  meekrosoft::fff
)
set_target_properties(test_rdmnet_cpp_api PROPERTIES CXX_STANDARD 14 FOLDER tests)
gtest_add_tests(TARGET test_rdmnet_cpp_api SKIP_DEPENDENCY)
