# On Windows, we get mDNS/DNS-SD services from Apple Bonjour for Windows. There are two options for
# consuming Bonjour for Windows:
# - The Bonjour SDK, available from Apple (LICENSING RESTRICTIONS MAY APPLY)
# - ETC's fork of the Bonjour source code, called mDNSWindows, available under the Apache 2.0
#   license.

# Configure-time options for locating various flavors of Bonjour for Windows
option(RDMNET_WINDOWS_USE_BONJOUR_SDK "Use Apple's Bonjour SDK for Windows (LICENSING RESTRICTIONS MAY APPLY)" OFF)
set(RDMNET_MDNSWINDOWS_INSTALL_LOC "" CACHE PATH "Override location for installed mDNSWindows binaries on Windows")
set(RDMNET_MDNSWINDOWS_SRC_LOC "" CACHE PATH "Override location for mDNSWindows to build from source on Windows")

# On Windows, we use Bonjour for Windows, either through the Bonjour SDK or ETC's Bonjour fork.
set(RDMNET_DISC_PLATFORM_SOURCES
  ${RDMNET_SRC}/rdmnet/disc/bonjour/rdmnet_disc_platform_defs.h
  ${RDMNET_SRC}/rdmnet/disc/bonjour/rdmnet_disc_bonjour.c
)
set(RDMNET_DISC_PLATFORM_INCLUDE_DIRS ${RDMNET_SRC}/rdmnet/disc/bonjour)

###################################################################################################
# Download the mDNSWindows binaries from the GitHub repository.
###################################################################################################
function(download_mdnswindows_binaries)
  set(MDNSWINDOWS_VERSION 1.3.0)

  set(MDNSWINDOWS_MERGE_DOWNLOAD_URL "https://github.com/ETCLabs/mDNSWindows/releases/download/v${MDNSWINDOWS_VERSION}/ETC_mDNSInstall.msm")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MDNSWINDOWS_DOWNLOAD_URL "https://github.com/ETCLabs/mDNSWindows/releases/download/v${MDNSWINDOWS_VERSION}/mDNSWindows_x64.zip")
  else()
    set(MDNSWINDOWS_DOWNLOAD_URL "https://github.com/ETCLabs/mDNSWindows/releases/download/v${MDNSWINDOWS_VERSION}/mDNSWindows_x86.zip")
  endif()

  message(STATUS "Neither RDMNET_MDNSWINDOWS_SRC_LOC or RDMNET_MDNSWINDOWS_INSTALL_LOC overrides provided.")
  message(STATUS "Downloading the correct release from GitHub...")

  file(DOWNLOAD ${MDNSWINDOWS_DOWNLOAD_URL} ${CMAKE_BINARY_DIR}/mdnswindows.zip STATUS DOWNLOAD_STATUS)
  list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
  list(GET DOWNLOAD_STATUS 1 DOWNLOAD_STATUS_STR)
  if(NOT DOWNLOAD_STATUS_CODE EQUAL 0)
    message(FATAL_ERROR "Error downloading from ${MDNSWINDOWS_DOWNLOAD_URL}: '${DOWNLOAD_STATUS_STR}'")
  endif()

  message(STATUS "Done. Extracting...")

  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/mdnswindows_install)
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xzf ${CMAKE_BINARY_DIR}/mdnswindows.zip WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/mdnswindows_install)
  set(RDMNET_MDNSWINDOWS_INSTALL_LOC ${CMAKE_BINARY_DIR}/mdnswindows_install CACHE STRING "Override location for mDNSWindows to build from source on Windows" FORCE)

  message(STATUS "Done. Downloading merge module...")

  file(DOWNLOAD ${MDNSWINDOWS_MERGE_DOWNLOAD_URL} ${CMAKE_BINARY_DIR}/mdnswindows_install/ETC_mDNSInstall.msm STATUS DOWNLOAD_STATUS)
  list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
  list(GET DOWNLOAD_STATUS 1 DOWNLOAD_STATUS_STR)
  if(DOWNLOAD_STATUS_CODE EQUAL 0)
    message(STATUS "Done.")
  else()
    message(FATAL_ERROR "Error downloading from ${MDNSWINDOWS_MERGE_DOWNLOAD_URL}: '${DOWNLOAD_STATUS_STR}'")
  endif()
endfunction()

###################################################################################################
# Use Apple's Bonjour SDK for Windows
###################################################################################################
macro(use_bonjour_sdk)
  # The location of the Bonjour SDK
  if(DEFINED BONJOUR_SDK_ROOT)
    get_filename_component(BONJOUR_SDK_ROOT ${BONJOUR_SDK_ROOT}
      ABSOLUTE
      BASE_DIR ${CMAKE_BINARY_DIR}
    )
  else()
    file(TO_CMAKE_PATH $ENV{BONJOUR_SDK_HOME} BONJOUR_SDK_ROOT)
  endif()

  # Setup the Bonjour SDK lib
  add_library(RDMnetBonjourStatic STATIC IMPORTED)
  target_include_directories(RDMnetBonjourStatic INTERFACE ${BONJOUR_SDK_ROOT}/Include)

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BONJOUR_LIB_DIR ${BONJOUR_SDK_ROOT}/Lib/x64)
  else()
    set(BONJOUR_LIB_DIR ${BONJOUR_SDK_ROOT}/Lib/Win32)
  endif()

  if(NOT EXISTS ${BONJOUR_LIB_DIR})
    message(FATAL_ERROR "Make sure the Bonjour SDK is installed, and try again.")
  endif()
  set_target_properties(RDMnetBonjourStatic PROPERTIES
    IMPORTED_LOCATION ${BONJOUR_LIB_DIR}/dnssd.lib
  )
  set(RDMNET_DISC_PLATFORM_LIBS RDMnetBonjourStatic)
endmacro()

###################################################################################################
# Add mDNSWindows as a source dependency
###################################################################################################
macro(use_mdnswindows_from_source)
  get_filename_component(RDMNET_MDNSWINDOWS_SRC_LOC ${RDMNET_MDNSWINDOWS_SRC_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
  if(NOT TARGET dnssdStatic)
    add_subdirectory(${RDMNET_MDNSWINDOWS_SRC_LOC}/mDNSWindows/DLLStub mDNSWindows/static)
  endif()
  if(NOT TARGET dnssd_etc)
    add_subdirectory(${RDMNET_MDNSWINDOWS_SRC_LOC}/mDNSWindows/DLL mDNSWindows/DLL)
  endif()
  set(RDMNET_DISC_PLATFORM_DEPENDENCIES dnssd_etc)
  set(RDMNET_DISC_PLATFORM_LIBS dnssdStatic)
  set(RDMNET_BONJOUR_MOCK_LIB dnssdStaticMock)
  set(DNS_SD_DLL $<TARGET_FILE:dnssd_etc>)
endmacro()

###################################################################################################
# Add mDNSWindows as a downloaded binary package
###################################################################################################
macro(use_mdnswindows_from_install)
  get_filename_component(RDMNET_MDNSWINDOWS_INSTALL_LOC ${RDMNET_MDNSWINDOWS_INSTALL_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
  if(NOT EXISTS ${RDMNET_MDNSWINDOWS_INSTALL_LOC}/lib)
    message(FATAL_ERROR "${RDMNET_MDNSWINDOWS_INSTALL_LOC} does not seem to contain a valid installation.")
  endif()

  # Setup the imported mDNSWindows library
  add_library(RDMnetMdnsWindows STATIC IMPORTED)
  set_target_properties(RDMnetMdnsWindows PROPERTIES IMPORTED_LOCATION ${RDMNET_MDNSWINDOWS_INSTALL_LOC}/lib/dnssd.lib)
  target_include_directories(RDMnetMdnsWindows INTERFACE ${RDMNET_MDNSWINDOWS_INSTALL_LOC}/include)
  set(RDMNET_DISC_PLATFORM_LIBS RDMnetMdnsWindows)

  # Setup the mock mDNSWindows library
  add_library(RDMnetMdnsWindowsMock STATIC IMPORTED)
  set_target_properties(RDMnetMdnsWindowsMock PROPERTIES IMPORTED_LOCATION ${RDMNET_MDNSWINDOWS_INSTALL_LOC}/lib/dnssd_mock.lib)
  target_include_directories(RDMnetMdnsWindowsMock INTERFACE ${RDMNET_MDNSWINDOWS_INSTALL_LOC}/include)
  set(RDMNET_BONJOUR_MOCK_LIB RDMnetMdnsWindowsMock)

  set(DNS_SD_DLL ${RDMNET_MDNSWINDOWS_INSTALL_LOC}/dll/dnssd.dll)
  configure_file(${RDMNET_ROOT}/tools/templates/mdnsmerge.wxi.in
    ${RDMNET_ROOT}/tools/install/windows/GeneratedFiles/mdnsmerge.wxi
  )
endmacro()

###################################################################################################
### The main logic:
###################################################################################################

if(RDMNET_WINDOWS_USE_BONJOUR_SDK)
  use_bonjour_sdk()
else()

  # Typical case - no configuration specified, download default binaries
  if(NOT RDMNET_MDNSWINDOWS_SRC_LOC AND NOT RDMNET_MDNSWINDOWS_INSTALL_LOC)
    download_mdnswindows_binaries()
  endif()

  if(RDMNET_MDNSWINDOWS_SRC_LOC)
    use_mdnswindows_from_source()
  elseif(RDMNET_MDNSWINDOWS_INSTALL_LOC)
    use_mdnswindows_from_install()
  endif()
endif()
