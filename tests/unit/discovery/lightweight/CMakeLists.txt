# RDMnet Lightweight Discovery library unit tests

rdmnet_add_unit_test(test_discovery_lightweight
  main.cpp
  test_lwmdns_send.cpp
  test_lwmdns_domain_parsing.cpp
  test_lwmdns_txt_record_parsing.cpp

  ${UNIT_TEST_DIR}/shared/fake_mcast.h
  ${UNIT_TEST_DIR}/shared/fake_mcast.cpp

  ${RDMNET_DISC_COMMON_SOURCES}
  ${RDMNET_DISC_PLATFORM_SOURCES}

  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet_mock/core/mcast.c
)
target_include_directories(test_discovery_lightweight PRIVATE ${RDMNET_SRC}/rdmnet/disc/lightweight)
target_link_libraries(test_discovery_lightweight PRIVATE EtcPalMock RDM)
