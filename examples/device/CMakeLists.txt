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
endif()