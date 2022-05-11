cmake_minimum_required(VERSION 3.10)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum macOS version targeted by RDMnet on macOS")
project(RDMnet VERSION 0.4.0)

# Sometimes the seems to be some weirdness with drive letter capitalization on Windows, so do a
# case-insensitive comparision
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL Windows)
  string(TOLOWER ${PROJECT_SOURCE_DIR} PROJECT_SOURCE_DIR_LC)
  string(TOLOWER ${CMAKE_SOURCE_DIR} CMAKE_SOURCE_DIR_LC)
else()
  set(PROJECT_SOURCE_DIR_LC ${PROJECT_SOURCE_DIR})
  set(CMAKE_SOURCE_DIR_LC ${CMAKE_SOURCE_DIR})
endif()

if(PROJECT_SOURCE_DIR_LC STREQUAL CMAKE_SOURCE_DIR_LC)
  set(BUILDING_RDMNET_STANDALONE TRUE)

  if (MSVC)
    add_compile_options(/W4 /permissive-)
  elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
      add_compile_options(-Wno-sign-conversion)
    else()
      add_compile_options(-Wno-sign-compare)
    endif()
  endif()

  # Allow folder grouping in generated IDE projects
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)

  # Add a target to run Clang Format
  file(GLOB_RECURSE FILES_TO_REFORMAT 
    examples/*.c
    examples/*.cpp
    examples/*.h
    include/*.h
    src/*.c
    src/*.cpp
    src/*.h
    tests/*.c
    tests/*.cpp
    tests/*.h
  )
  add_custom_target(reformat_all COMMAND clang-format -i ${FILES_TO_REFORMAT})
  set_target_properties(reformat_all PROPERTIES FOLDER tools)
endif()

set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(RDMNET_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR}/src)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

option(RDMNET_BUILD_TESTS "Build the RDMnet unit tests" OFF)
option(RDMNET_BUILD_GUI_EXAMPLES "Build the RDMnet GUI example applications" OFF)
option(RDMNET_BUILD_CONSOLE_EXAMPLES "Build the RDMnet console example applications" OFF)
option(RDMNET_BUILD_TEST_TOOLS "Build the RDMnet test tools (typically used in development only)" OFF)
option(RDMNET_INSTALL_PDBS "Include PDBs in RDMnet install target" ON)

if(RDMNET_BUILD_TESTS)
  add_subdirectory(external/fff)
endif()

################################ DNS-SD SUPPORT ###############################

include(ResolveDnsSdProvider)

################################ Main libraries ###############################

add_subdirectory(src)

#################################### Tests ####################################

if(RDMNET_BUILD_TESTS)
  enable_testing()
  include(AddGoogleTest)
  add_subdirectory(tests)
endif()

################################### Examples ################################## 

if(RDMNET_BUILD_GUI_EXAMPLES OR RDMNET_BUILD_CONSOLE_EXAMPLES)
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
endif()
