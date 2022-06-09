# RDMnet Broker library unit tests

rdmnet_add_unit_test(test_rdmnet_broker
  # RDMnet Broker lib unit test sources
  broker_mocks.h
  test_broker_client.cpp
  test_broker_core_connect_handling.cpp
  test_broker_core_rpt_handling.cpp
  test_broker_core_startup.cpp
  test_broker_message_handling.cpp
  test_broker_discovery.cpp
  test_broker_threads.cpp
  test_broker_uid_manager.cpp
  test_broker_settings.cpp
  test_broker_util.cpp
  test_dns_txt_record_item.cpp
  main.cpp

  ${RDMNET_BROKER_SOURCES}
  # ${RDMNET_MOCK_ALL_SOURCES}
  ${RDMNET_SRC}/rdmnet/common.c
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.c
  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
  ${RDMNET_SRC}/rdmnet_mock/discovery.c
)
target_include_directories(test_rdmnet_broker PRIVATE ${RDMNET_SRC}/rdmnet/broker)
target_link_libraries(test_rdmnet_broker PRIVATE EtcPalMock RDM)
