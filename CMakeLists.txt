cmake_minimum_required(VERSION 3.15)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.12 CACHE STRING "Minimum macOS version targeted by RDMnet on macOS")
project(RDMnet)

set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR})
set(RDMNET_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR}/src)
set(RDMNET_CMAKE ${CMAKE_CURRENT_LIST_DIR}/cmake)

###################### Compile Options and Configuration ######################

if(DEFINED RDMNET_CONFIG_LOC)
  get_filename_component(RDMNET_CONFIG_LOC ${RDMNET_CONFIG_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
endif()

option(RDMNET_BUILD_TESTS "Build the RDMnet unit tests" OFF)
option(RDMNET_BUILD_GUI_EXAMPLES "Build the RDMnet GUI example applications" OFF)
option(RDMNET_BUILD_CONSOLE_EXAMPLES "Build the RDMnet console example applications" OFF)
option(RDMNET_BUILD_TEST_TOOLS "Build the RDMnet test tools (typically used in development only)" OFF)
option(RDMNET_INSTALL_PDBS "Include PDBs in RDMnet install target" ON)

################################ DNS-SD SUPPORT ###############################

include(${RDMNET_CMAKE}/ResolveDnsSdProvider.cmake)

################################# Dependencies ################################

include(${RDMNET_CMAKE}/OssDependencyTools.cmake)
determine_compile_environment()
if(NOT COMPILING_AS_OSS)
  include(${RDMNET_CMAKE}/AddCMakeTools.cmake)
endif()
include(${RDMNET_CMAKE}/ResolveDependencies.cmake)

############################## Standalone support #############################

if(NOT COMPILING_AS_OSS)
  include(${CMAKE_TOOLS_MODULES}/QualityGateHelpers.cmake)

  setup_standalone_compile()
  setup_clang_format()
  # TODO: setup_clang_tidy()
  setup_address_sanitizer()
endif()

################################ Main libraries ###############################

add_subdirectory(src)

#################################### Tests ####################################

if(RDMNET_BUILD_TESTS)
  include(GoogleTest)
  enable_testing()
  add_subdirectory(external/fff)
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
