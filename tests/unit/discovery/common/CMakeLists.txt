# RDMnet Discovery library unit tests

rdmnet_add_unit_test(test_discovery_common
  test_disc_common_fakes.h
  test_disc_common_fakes.c

  test_discovery_common.cpp
  test_discovered_broker.cpp
  test_monitored_scope.cpp
  test_registered_broker.cpp

  main.cpp

  ${RDMNET_DISC_COMMON_SOURCES}

  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_include_directories(test_discovery_common PRIVATE ${RDMNET_SRC}/rdmnet_mock/disc/mock_platform)
target_link_libraries(test_discovery_common PRIVATE RDM EtcPalMock)
