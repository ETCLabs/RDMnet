cmake_minimum_required(VERSION 3.3)

project(RDMnet VERSION 0.1.0)

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  if (MSVC)
    add_compile_options(/W4)
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Wconversion)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      add_compile_options(-Wno-sign-conversion)
    else()
      add_compile_options(-Wno-sign-compare)
    endif()
  endif()
endif()

# Main library

add_subdirectory(src)

# Tests

# option(RDMNET_BUILD_TESTS "Build the RDMnet unit tests" OFF)
# if(RDMNET_BUILD_TESTS OR PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
#   add_subdirectory(tests)
# endif()

# Examples

# option(RDMNET_BUILD_EXAMPLES "Build the RDMnet example applications" OFF)
# if(RDMNET_BUILD_EXAMPLES OR PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
#   add_subdirectory(examples)
# endif()
