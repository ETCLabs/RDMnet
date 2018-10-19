
option(BUILD_CONTROLLER_EXAMPLE "Build the RDMnet Controller GUI example" OFF)
if(BUILD_CONTROLLER_EXAMPLE)
  add_subdirectory(controller)
endif()
