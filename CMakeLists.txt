cmake_minimum_required(VERSION 3.3)

project(RDMnet VERSION 0.2.0)

if(PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(OPTION_DEFAULTS ON)
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
else()
  set(OPTION_DEFAULTS OFF)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/tools/cmake)

option(RDMNET_BUILD_TESTS "Build the RDMnet unit tests" ${OPTION_DEFAULTS})
option(RDMNET_BUILD_EXAMPLES "Build the RDMnet example applications" ${OPTION_DEFAULTS})
option(RDMNET_BUILD_CONSOLE_EXAMPLES "Build only the console RDMnet example applications" OFF)
option(RDMNET_BUILD_TEST_TOOLS "Build the RDMnet test tools (typically used in development only)" OFF)

if(RDMNET_BUILD_TESTS)
  add_subdirectory(external/fff)
endif()

################################ Main libraries ###############################

add_subdirectory(src)

#################################### Tests ####################################

if(RDMNET_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

################################### Examples ################################## 

if(RDMNET_BUILD_CONSOLE_EXAMPLES)
  set(RDMNET_BUILD_EXAMPLES OFF)
endif()

if(RDMNET_BUILD_EXAMPLES OR RDMNET_BUILD_CONSOLE_EXAMPLES)
  add_subdirectory(examples)
endif()

##################################### Misc ####################################

if(RDMNET_BUILD_TEST_TOOLS)
  add_subdirectory(tools/test)
endif()

# On Windows, we put the DLL in the install directory to later be packaged with
# the installer.
if(DEFINED DNS_SD_DLL)
  install(FILES ${DNS_SD_DLL} DESTINATION bin)
  install(FILES ${DNS_SD_DLL} DESTINATION controller)
endif()
