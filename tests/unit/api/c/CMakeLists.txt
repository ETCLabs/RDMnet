# RDMnet C API unit tests

rdmnet_add_unit_test(test_rdmnet_c_api EXCLUDE_FROM_ALL
  main.cpp
  test_controller.cpp

  # TODO replace this with ${RDMNET_API_SOURCES}
  ${RDMNET_SRC}/rdmnet/controller.c
  ${RDMNET_SRC}/rdmnet/common.c
)
target_link_libraries(test_rdmnet_c_api PRIVATE EtcPalMock RDM)
