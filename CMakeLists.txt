cmake_minimum_required(VERSION 3.3)

project(RDMnet VERSION 0.2.0)

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

##################################### Misc ####################################

# On Windows, we put the DLL in the install directory to later be packaged with
# the installer.
if(DEFINED DNS_SD_DLL)
  install(FILES ${DNS_SD_DLL} DESTINATION bin)
  install(FILES ${DNS_SD_DLL} DESTINATION controller)
endif()