if(CMAKE_SIZEOF_VOID_P STREQUAL 4)
  list(APPEND CMAKE_PREFIX_PATH ${libgadget_SOURCE_DIR}/Win32)
else()
  list(APPEND CMAKE_PREFIX_PATH ${libgadget_SOURCE_DIR}/x64)
endif()

find_package(GadgetDLL 3.0.0 REQUIRED)

set(FAKEWAY_TARGET rdmnet_gateway_example)
add_executable(${FAKEWAY_TARGET}
  src/fakeway_default_responder.h
  src/fakeway_default_responder.cpp
  src/fakeway.h
  src/fakeway.cpp
  src/gadget_interface.h
  src/gadget_interface.cpp

  src/windows/main.cpp
)
target_include_directories(${FAKEWAY_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_link_libraries(${FAKEWAY_TARGET} PRIVATE RDMnet GadgetDLL::GadgetDLL)
set_target_properties(${FAKEWAY_TARGET} PROPERTIES FOLDER examples)

if(DEFINED DNS_SD_DLL)
  add_custom_command(
    TARGET ${FAKEWAY_TARGET}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:${FAKEWAY_TARGET}>
    COMMENT "Copying DNS-SD dll to executable directory..."
  )
endif()

get_target_property(GADGET_DLL_LOCATION GadgetDLL::GadgetDLL IMPORTED_LOCATION_RELEASE)
add_custom_command(
  TARGET ${FAKEWAY_TARGET}
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${GADGET_DLL_LOCATION} $<TARGET_FILE_DIR:${FAKEWAY_TARGET}>
  COMMENT "Copying Gadget DLL to executable directory..."
)
install(FILES ${GADGET_DLL_LOCATION} DESTINATION bin)

install(TARGETS ${FAKEWAY_TARGET}
  RUNTIME DESTINATION bin
)
