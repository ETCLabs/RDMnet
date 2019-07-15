# RDMnet Discovery library unit tests

add_executable(test_discovery_bonjour
  # RDMnet Discovery lib unit test sources
  test_discovery_bonjour.cpp

  ${RDMNET_SRC}/rdmnet/core/util.c
  ${RDMNET_SRC}/rdmnet_mock/core.c
)
target_include_directories(test_discovery_bonjour PRIVATE ${RDMNET_SRC})
target_link_libraries(test_discovery_bonjour PRIVATE
  RDMnetDiscovery
  dnssd_mock
  lwpa_mock
  RDM
  gtest_main
  meekrosoft::fff
)
set_target_properties(test_discovery_bonjour PROPERTIES CXX_STANDARD 11)
gtest_add_tests(TARGET test_discovery_bonjour SKIP_DEPENDENCY)

if(DEFINED DNS_SD_DLL)
  add_custom_command(
    TARGET test_discovery_bonjour
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:test_discovery_bonjour>
    COMMENT "Copying DNS-SD dll to executable directory..."
  )
endif()
