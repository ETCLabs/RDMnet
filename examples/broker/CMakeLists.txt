################################ RDMnet Broker ################################

set(BROKER_TARGET rdmnet_broker_example)

add_executable(${BROKER_TARGET}
  src/broker_shell.h
  src/broker_shell.cpp
)

set_target_properties(${BROKER_TARGET} PROPERTIES CXX_STANDARD 14 FOLDER examples)
target_include_directories(${BROKER_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
#target_link_libraries(${BROKER_TARGET} PRIVATE RDMnetBroker)
target_link_libraries(${BROKER_TARGET} PRIVATE RDMnet)
if(WIN32)
  target_sources(${BROKER_TARGET} PRIVATE
    src/windows/main.cpp
    src/windows/win_broker_log.h
    src/windows/win_broker_log.cpp
  )
  target_compile_definitions(${BROKER_TARGET} PRIVATE UNICODE _UNICODE)
  if(DEFINED DNS_SD_DLL)
    add_custom_command(
      TARGET ${BROKER_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:${BROKER_TARGET}>
      COMMENT "Copying DNS-SD dll to executable directory..."
    )
  endif()
elseif(APPLE)
  target_sources(${BROKER_TARGET} PRIVATE
    src/macos/main.cpp
    src/macos/macos_broker_log.h
    src/macos/macos_broker_log.cpp
  )
  target_link_libraries(${BROKER_TARGET} PRIVATE pthread)
elseif(UNIX)
  target_sources(${BROKER_TARGET} PRIVATE
    src/linux/main.cpp
    src/linux/linux_broker_log.h
    src/linux/linux_broker_log.cpp
  )
  target_link_libraries(${BROKER_TARGET} PRIVATE pthread)
endif()

install(TARGETS ${BROKER_TARGET}
  RUNTIME DESTINATION bin
)
