# Somewhat messy way to download and build the Google Test library at configure time.
# Taken directly from the Google Test README,
# and generalized here: https://crascit.com/2015/07/25/cmake-gtest/

function(get_googletest RDMNET_REPO_ROOT)
  # Download and unpack googletest at configure time
  configure_file(${RDMNET_REPO_ROOT}/tools/cmake/CMakeLists_gtest.txt.in googletest-download/CMakeLists.txt)

  execute_process(COMMAND "${CMAKE_COMMAND}" -G "${CMAKE_GENERATOR}" .
                  RESULT_VARIABLE result
                  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/googletest-download" )
  if (result)
    message(FATAL_ERROR "CMake step for googletest failed: ${result}")
  endif()

  execute_process(COMMAND "${CMAKE_COMMAND}" --build .
                  RESULT_VARIABLE result
                  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/googletest-download" )

  if (result)
    message(FATAL_ERROR "Build step for googletest failed: ${result}")
  endif()

  # Prevent GoogleTest from overriding our compiler/linker options
  # when building with Visual Studio
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

  # Add googletest directly to our build. This adds the following targets:
  # gtest and gtest_main
  add_subdirectory("${CMAKE_CURRENT_BINARY_DIR}/googletest-src/googletest"
                   "${CMAKE_CURRENT_BINARY_DIR}/googletest-build")

endfunction(get_googletest)
