cmake_minimum_required(VERSION 3.3)

set(RDMNET_VERSION_MAJOR 0)
set(RDMNET_VERSION_MINOR 2)
set(RDMNET_VERSION_PATCH 0)
set(RDMNET_VERSION ${RDMNET_VERSION_MAJOR}.${RDMNET_VERSION_MINOR}.${RDMNET_VERSION_PATCH})
project(RDMnet VERSION ${RDMNET_VERSION})

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

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/tools/cmake)

############################### Versioned builds ############################## 

option(RDMNET_CREATE_VERSIONED_BUILD "Create a new versioned build of RDMnet (should only be set by CI)" OFF)
if(RDMNET_CREATE_VERSIONED_BUILD)
  if(NOT RDMNET_VERSION_BUILD)
    message(FATAL_ERROR "RDMNET_CREATE_VERSIONED_BUILD was specified but no RDMNET_VERSION_BUILD was supplied.")
  endif()

  # Build the variables we will need to configure the version header
  set(RDMNET_VERSION_STRING_FULL ${RDMNET_VERSION}.${RDMNET_VERSION_BUILD})
  string(TIMESTAMP CURRENT_YEAR "%Y")
  string(TIMESTAMP RDMNET_VERSION_DATESTR "%Y-%m-%d")
  set(RDMNET_VERSION_COPYRIGHT "Copyright ${CURRENT_YEAR} ETC Inc.")

  # Configure the version header
  message(STATUS "Configuring versioned build for ${RDMNET_VERSION_STRING_FULL}...")
  configure_file(${CMAKE_CURRENT_LIST_DIR}/tools/ci/version.h.in ${CMAKE_CURRENT_LIST_DIR}/include/rdmnet/version.h)
endif()

################################ Main libraries ###############################

add_subdirectory(src)

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(OPTION_DEFAULTS ON)
else()
  set(OPTION_DEFAULTS OFF)
endif()

#################################### Tests ####################################

option(RDMNET_BUILD_TESTS "Build the RDMnet unit tests" ${OPTION_DEFAULTS})
if(RDMNET_BUILD_TESTS)
  add_subdirectory(tests)
endif()

################################### Examples ################################## 

option(RDMNET_BUILD_EXAMPLES "Build the RDMnet example applications" ${OPTION_DEFAULTS})
option(RDMNET_BUILD_CONSOLE_EXAMPLES "Build only the console RDMnet example applications" OFF)
if(RDMNET_BUILD_CONSOLE_EXAMPLES)
  set(RDMNET_BUILD_EXAMPLES OFF)
endif()

if(RDMNET_BUILD_EXAMPLES OR RDMNET_BUILD_CONSOLE_EXAMPLES)
  add_subdirectory(examples)
endif()
