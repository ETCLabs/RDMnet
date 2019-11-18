find_package(Git QUIET)

option(RDMNET_FORCE_SUBMODULE_DEPS "Disable overriding of submodule dependencies with clones at same level as RDMnet" OFF)

function(rdmnet_add_dependency target loc_variable)

  # Step 1 - see if the target has already been included
  if(NOT TARGET ${target})

    # Step 2 - if the user has provided a location for the dependency, use that
    if(DEFINED ${loc_variable})

      message(STATUS "${loc_variable} provided; attempting to add ${target} from ${${loc_variable}}...")
      get_filename_component(${loc_variable} ${${loc_variable}}
        ABSOLUTE
        BASE_DIR ${CMAKE_BINARY_DIR}
      )
      add_subdirectory(${${loc_variable}} ${target})

    # Step 3 - look for the dependency in a folder with the same name as the
    # target, at the same level as this folder
    elseif(NOT RDMNET_FORCE_SUBMODULE_DEPS AND EXISTS "${RDMNET_ROOT}/../${target}")

      message(STATUS "Found directory for ${target} at same level as RDMnet. Overriding submodule dependency with that directory.")
      add_subdirectory(${RDMNET_ROOT}/../${target} ${target})

    else()

      if(GIT_FOUND AND EXISTS ${RDMNET_ROOT}/.git)
        # Update submodules as needed
        option(GIT_SUBMODULE "Check submodules during build" ON)
        if(GIT_SUBMODULE)
          execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init
                          WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                          RESULT_VARIABLE GIT_SUBMOD_RESULT)
          if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
          endif()
        endif()
      endif()

      if(NOT EXISTS ${RDMNET_ROOT}/external/${target}/CMakeLists.txt)
          message(FATAL_ERROR "The submodules were not downloaded! GIT_SUBMODULE was turned off or failed. Please update submodules and try again.")
      endif()

      add_subdirectory(${RDMNET_ROOT}/external/${target} ${CMAKE_BINARY_DIR}/external/${target})
    endif()

    # Organize the dependency nicely in IDEs
    set_target_properties(${target} PROPERTIES FOLDER dependencies)
  endif()
endfunction()
