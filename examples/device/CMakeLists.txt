################################ RDMnet Device ################################

set(DEVICE_TARGET rdmnet_device_example)

add_executable(${DEVICE_TARGET}
  src/example_device.h
  src/example_device.c
  src/default_responder.h
  src/default_responder.c
)

target_include_directories(${DEVICE_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(${DEVICE_TARGET} PRIVATE RDMnet)
if(WIN32)
  target_sources(${DEVICE_TARGET} PRIVATE
    src/windows/win_device_log.h
    src/windows/win_device_log.c
    src/windows/main.c
  )
  target_compile_definitions(${DEVICE_TARGET} PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(${DEVICE_TARGET} PRIVATE rpcrt4)
  if(DEFINED DNS_SD_DLL)
    add_custom_command(
      TARGET ${DEVICE_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:${DEVICE_TARGET}>
      COMMENT "Copying DNS-SD dll to executable directory..."
    )
  endif()
endif()

install(TARGETS ${DEVICE_TARGET}
  RUNTIME DESTINATION bin
)
