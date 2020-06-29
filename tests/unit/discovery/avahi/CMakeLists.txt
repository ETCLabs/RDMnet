# RDMnet Avahi Discovery library unit tests

add_executable(test_discovery_avahi
  test_discovery_avahi.cpp

  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
)
target_include_directories(test_discovery_avahi PRIVATE
  ${RDMNET_INCLUDE}
  ${RDMNET_SRC}
  ${RDMNET_DISC_PLATFORM_INCLUDE_DIRS}
)
target_link_libraries(test_discovery_avahi PRIVATE
  EtcPalMock
  RDM
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_discovery_avahi PROPERTIES CXX_STANDARD 17 FOLDER tests)
gtest_discover_tests(test_discovery_avahi)
