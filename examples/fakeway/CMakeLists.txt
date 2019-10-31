if(CMAKE_SIZEOF_VOID_P STREQUAL 4)
  list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/../../external/libGadget/Win32)
else()
  list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/../../external/libGadget/x64)
endif()

find_package(GadgetDLL 2.0.4 REQUIRED)

set(FAKEWAY_TARGET fakeway)
add_executable(${FAKEWAY_TARGET}
  src/fakeway_default_responder.h
  src/fakeway_default_responder.cpp
  src/fakeway_log.h
  src/fakeway.h
  src/fakeway.cpp
  src/gadget_interface.h
  src/rdmnet_lib_wrapper.h
  src/rdmnet_lib_wrapper.cpp
)
target_link_libraries(${FAKEWAY_TARGET} PRIVATE GadgetDLL::GadgetDLL)
set_target_properties(${FAKEWAY_TARGET} PROPERTIES FOLDER examples)

if(DEFINED DNS_SD_DLL)
  add_custom_command(
    TARGET ${FAKEWAY_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:${FAKEWAY_TARGET}>
    COMMENT "Copying DNS-SD dll to executable directory..."
  )
endif()

get_target_property(GADGET_DLL_LOCATION GadgetDLL::GadgetDLL IMPORTED_LOCATION)
add_custom_command(
  TARGET ${FAKEWAY_TARGET}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${GADGET_DLL_LOCATION} $<TARGET_FILE_DIR:${FAKEWAY_TARGET}>
  COMMENT "Copying Gadget DLL to executable directory..."
)
