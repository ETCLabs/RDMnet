
if(RDMNET_BUILD_GUI_EXAMPLES)
  add_subdirectory(controller)
endif()

if(RDMNET_BUILD_CONSOLE_EXAMPLES)
  if(WIN32)
    include(${RDMNET_CMAKE}/AddLibGadget.cmake)
    add_subdirectory(fakeway)
  endif()
  add_subdirectory(broker)
  add_subdirectory(device)
  add_subdirectory(discovery)
  add_subdirectory(llrp_manager)
endif()
