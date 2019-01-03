################################ RDMnet Device ################################

set(DEVICE_TARGET rdmnet_device_example)

add_executable(${DEVICE_TARGET}
  device.h
  device_llrp.h
  device_log.h
  default_responder.h

  device.c
  device_llrp.c
  device_log.c
  device_main.c
  default_responder.c
)

target_include_directories(${DEVICE_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${DEVICE_TARGET} PRIVATE RDMnet)
if(WIN32)
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