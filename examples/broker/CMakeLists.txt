################################ RDMnet Broker ################################

set(BROKER_TARGET rdmnet_broker_example)

add_executable(${BROKER_TARGET}
  broker_log.h
  iflist.h
  service_shell.h

  broker_main.cpp
  broker_log.cpp
  iflist.cpp
  service_shell.cpp
  service_utils.cpp
)

target_include_directories(${BROKER_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${BROKER_TARGET} PRIVATE Broker)
if(WIN32)
  target_compile_definitions(${BROKER_TARGET} PRIVATE UNICODE _UNICODE)
  if(DEFINED DNS_SD_DLL)
    add_custom_command(
      TARGET ${BROKER_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:${BROKER_TARGET}>
      COMMENT "Copying DNS-SD dll to executable directory..."
    )
  endif()
endif()

install(TARGETS ${BROKER_TARGET}
  RUNTIME DESTINATION bin
)