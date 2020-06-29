# Updates the libGadget submodule.

find_package(Git QUIET)

if(GIT_FOUND AND EXISTS ${RDMNET_ROOT}/.git)
  execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init external/libGadget
                  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                  RESULT_VARIABLE GIT_SUBMOD_RESULT)
  if(NOT GIT_SUBMOD_RESULT EQUAL "0")
    message(FATAL_ERROR "git submodule update --init failed for external/libGadget with ${GIT_SUBMOD_RESULT}, please checkout submodules")
  endif()
endif()
