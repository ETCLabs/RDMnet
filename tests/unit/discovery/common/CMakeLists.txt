# RDMnet Bonjour Discovery library unit tests

add_executable(test_discovery_common
  disc_platform_defs.h
  test_disc_common_fakes.h
  test_disc_common_fakes.c

  test_discovery_common.cpp
  test_discovered_broker.cpp

  ${RDMNET_INCLUDE}/rdmnet_mock/core.h
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_include_directories(test_discovery_common PRIVATE
  ${RDMNET_INCLUDE}
  ${RDMNET_SRC}
  ${CMAKE_CURRENT_LIST_DIR}
)
target_link_libraries(test_discovery_common PRIVATE
  RDMnetDiscoveryCommon
  EtcPalMock
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_discovery_common PROPERTIES CXX_STANDARD 11 FOLDER tests)
gtest_add_tests(TARGET test_discovery_common SKIP_DEPENDENCY)
