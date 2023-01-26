# RDMnet Bonjour Discovery library unit tests

add_executable(test_discovery_bonjour
  test_discovery_bonjour.cpp

  ${RDMNET_DISC_COMMON_SOURCES}
  ${RDMNET_DISC_PLATFORM_SOURCES}

  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core/common.c
)
target_include_directories(test_discovery_bonjour PRIVATE
  ${UNIT_TEST_DIR}/shared/configs/dynamic
  ${UNIT_TEST_DIR}/shared
  ${RDMNET_INCLUDE}
  ${RDMNET_SRC}
  ${RDMNET_DISC_PLATFORM_INCLUDE_DIRS}
)
target_compile_definitions(test_discovery_bonjour PRIVATE RDMNET_HAVE_CONFIG_H)
target_link_libraries(test_discovery_bonjour PRIVATE
  ${RDMNET_BONJOUR_MOCK_LIB}
  EtcPalMock
  RDM
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_discovery_bonjour PROPERTIES CXX_STANDARD 17 FOLDER tests)
gtest_discover_tests(test_discovery_bonjour)

if(DEFINED DNS_SD_DLL)
  add_custom_command(
    TARGET test_discovery_bonjour
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:test_discovery_bonjour>
    COMMENT "Copying DNS-SD dll to executable directory..."
  )
endif()
