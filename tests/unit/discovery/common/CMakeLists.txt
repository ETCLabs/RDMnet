# RDMnet Discovery library unit tests

rdmnet_add_unit_test(test_discovery_common
  disc_platform_defs.h
  test_disc_common_fakes.h
  test_disc_common_fakes.c

  test_discovery_common.cpp
  test_discovered_broker.cpp
  test_monitored_scope.cpp
  test_registered_broker.cpp

  main.cpp

  ${RDMNET_INCLUDE}/rdmnet_mock/core.h
  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_include_directories(test_discovery_common PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(test_discovery_common PRIVATE RDMnetDiscoveryCommon EtcPalMock)
