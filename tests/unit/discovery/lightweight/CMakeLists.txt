# RDMnet Lightweight Discovery library unit tests

add_executable(test_discovery_lightweight
  test_lwmdns_message.cpp
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c
)
target_link_libraries(test_discovery_lightweight PRIVATE
  RDMnetDiscoveryCommon
  RDMnetDiscoveryPlatform
  EtcPalMock
  RDM
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_discovery_lightweight PROPERTIES CXX_STANDARD 11 FOLDER tests)
gtest_add_tests(TARGET test_discovery_lightweight SKIP_DEPENDENCY)
