# RDMnet API layer unit tests

add_executable(test_rdmnet_api
  test_rdmnet_client.cpp
)
target_link_libraries(test_rdmnet_api PRIVATE
  RDMnetApi
  RDMnetCore_mock
  lwpa_mock
  RDM
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_rdmnet_api PROPERTIES CXX_STANDARD 11)
gtest_add_tests(TARGET test_rdmnet_api SKIP_DEPENDENCY)

