message(STATUS "Adding GoogleTest...")

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG 96f4ce02a3a78d63981c67acbd368945d11d7d70
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

mark_as_advanced(
  gmock_build_tests
  gtest_build_samples
  gtest_build_tests
  gtest_disable_pthreads
  gtest_force_shared_crt
  gtest_hide_internal_symbols
  BUILD_GMOCK
  BUILD_GTEST
)

set_target_properties(gtest gtest_main gmock gmock_main
  PROPERTIES FOLDER "tests"
)
