function(rdmnet_add_dependency target loc_variable)
  # Step 1 - see if the target has already been included
  if(NOT TARGET ${target})
    message(STATUS "Attempting to locate dependency ${target}...")
    message(STATUS "Checking user-provided variable ${loc_variable}...")
    # Step 2 - if the user has provided a location for the dependency, use that
    if(DEFINED ${loc_variable})
      message(STATUS "Found. Adding dependency from ${${loc_variable}}.")
      get_filename_component(${loc_variable} ${${loc_variable}}
        ABSOLUTE
        BASE_DIR ${CMAKE_BINARY_DIR}
      )
      add_subdirectory(${${loc_variable}} ${target})
    else()
      message(STATUS "Not defined. Checking if already downloaded at the same level as this directory...")
      # Step 3 - look for the dependency in a folder with the same name as the
      # target, at the same level as this folder
      if (EXISTS ${RDMNET_ROOT}/../${target})
        message(STATUS "Found. Adding dependency from RDMNET_DIR/../${target}.")
        add_subdirectory(${RDMNET_ROOT}/../${target} ${target})
      else()
        message(STATUS "Not found.")
        # TODO: add package manager step
        message(FATAL_ERROR "Dependency ${target} not satisfied.")
      endif()
    endif()
  endif()
endfunction()
