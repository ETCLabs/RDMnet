# RDMnet Discovery library unit tests

add_executable(test_rdmnet_discovery
  # RDMnet Discovery lib unit test sources
  test_discovery_bonjour.cpp

  ${RDMNET_SRC}/rdmnet/core/util.c
)
target_include_directories(test_rdmnet_discovery PRIVATE ${RDMNET_SRC})
target_link_libraries(test_rdmnet_discovery PRIVATE
  RDMnetDiscovery
  dnssd_mock
  lwpa_mock
  gtest_main
  meekrosoft::fff
)
gtest_add_tests(TARGET test_rdmnet_discovery)

if(DEFINED DNS_SD_DLL)
  add_custom_command(
    TARGET test_rdmnet_discovery
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:test_rdmnet_discovery>
    COMMENT "Copying DNS-SD dll to executable directory..."
  )
endif()
