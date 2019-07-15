# Determines the provider of mDNS and DNS-SD support based on the compilation
# platform and user input variables.
#
# Sets the following variables which are used by source builds:
# - DNS_SD_ADDITIONAL_SOURCES: Source files to be added to the RDMnet library compilation
# - DNS_SD_ADDITIONAL_INCLUDE_DIRS: Include directories to be added to the RDMnet library compilation
# - DNS_SD_ADDITIONAL_LIBS: Static libs to be added to the RDMnet library compilation

set(MDNSWINDOWS_VERSION 1.2.0.3)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(MDNSWINDOWS_DOWNLOAD_URL "https://dl.bintray.com/etclabs/mdnswindows_bin/mDNSWindows_${MDNSWINDOWS_VERSION}_x64.zip")
else()
  set(MDNSWINDOWS_DOWNLOAD_URL "https://dl.bintray.com/etclabs/mdnswindows_bin/mDNSWindows_${MDNSWINDOWS_VERSION}_x86.zip")
endif()

option(RDMNET_WINDOWS_USE_BONJOUR_SDK
  "Use Apple's Bonjour SDK for Windows (LICENSING RESTRICTIONS MAY APPLY)" OFF)
set(MDNSWINDOWS_INSTALL_LOC "" CACHE STRING "Override location for installed mDNSWindows binaries on Windows")
set(MDNSWINDOWS_SRC_LOC "" CACHE STRING "Override location for mDNSWindows to build from source on Windows")

# The imported DNS-SD library. This will have its properties set by the various options below.
add_library(dnssd INTERFACE)

# The RDMnet discovery layer required to interface with the imported library.
set(RDMNET_DISCOVERY_HEADERS ${RDMNET_INCLUDE}/rdmnet/core/discovery.h)

add_library(RDMnetDiscovery INTERFACE)
target_sources(RDMnetDiscovery INTERFACE ${RDMNET_DISCOVERY_HEADERS})
target_include_directories(RDMnetDiscovery INTERFACE ${RDMNET_INCLUDE} ${RDMNET_SRC})

# A version of the DNS-SD library with certain symbols removed for mocking.
add_library(dnssd_mock INTERFACE)

if(WIN32)
  # On Windows, we use Bonjour for Windows, either through the Bonjour SDK or ETC's Bonjour fork.
  set(RDMNET_DISCOVERY_ADDITIONAL_SOURCES
    ${RDMNET_SRC}/rdmnet/discovery/bonjour.h
    ${RDMNET_SRC}/rdmnet/discovery/bonjour.c
  )

  if(RDMNET_WINDOWS_USE_BONJOUR_SDK) # Using Apple's Bonjour SDK for Windows

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
    add_library(bonjour_static STATIC IMPORTED)
    target_include_directories(bonjour_static INTERFACE ${BONJOUR_SDK_ROOT}/Include)
    # Is this really the way to determine if we are 64-bit? Seems dumb...
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(BONJOUR_LIB_DIR ${BONJOUR_SDK_ROOT}/Lib/x64)
    else()
      set(BONJOUR_LIB_DIR ${BONJOUR_SDK_ROOT}/Lib/Win32)
    endif()
    if(NOT EXISTS ${BONJOUR_LIB_DIR})
      message(FATAL_ERROR "Make sure the Bonjour SDK is installed, and try again.")
    endif()
    set_target_properties(bonjour_static PROPERTIES
      IMPORTED_LOCATION ${BONJOUR_LIB_DIR}/dnssd.lib
    )
    target_link_libraries(dnssd INTERFACE bonjour_static)

  else() # Using ETC's Bonjour fork

    if(NOT MDNSWINDOWS_SRC_LOC AND NOT MDNSWINDOWS_INSTALL_LOC)
      message(STATUS "Neither MDNSWINDOWS_SRC_LOC or MDNSWINDOWS_INSTALL_LOC overrides provided.")
      message(STATUS "Downloading the latest release from Bintray...")

      file(DOWNLOAD ${MDNSWINDOWS_DOWNLOAD_URL} ${CMAKE_BINARY_DIR}/mdnswindows.zip STATUS DOWNLOAD_STATUS)
      list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
      list(GET DOWNLOAD_STATUS 1 DOWNLOAD_STATUS_STR)
      if(NOT DOWNLOAD_STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Error downloading from ${MDNSWINDOWS_DOWNLOAD_URL}: '${DOWNLOAD_STATUS_STR}'")
      endif()

      message(STATUS "Done. Extracting...")

      file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/mdnswindows_install)
      execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xzf ${CMAKE_BINARY_DIR}/mdnswindows.zip WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/mdnswindows_install)
      set(MDNSWINDOWS_INSTALL_LOC ${CMAKE_BINARY_DIR}/mdnswindows_install CACHE STRING "Override location for mDNSWindows to build from source on Windows" FORCE)

      message(STATUS "Done.")

    endif()

    if(MDNSWINDOWS_SRC_LOC) # Add mDNSWindows as a source dependency

      get_filename_component(MDNSWINDOWS_SRC_LOC ${MDNSWINDOWS_SRC_LOC}
        ABSOLUTE
        BASE_DIR ${CMAKE_BINARY_DIR}
      )
      add_subdirectory(${MDNSWINDOWS_SRC_LOC}/mDNSWindows/DLLStub mDNSWindows/static)
      add_subdirectory(${MDNSWINDOWS_SRC_LOC}/mDNSWindows/DLL mDNSWindows/DLL)
      add_dependencies(dnssd dnssd_etc)
      target_link_libraries(dnssd INTERFACE dnssdStatic)
      target_link_libraries(dnssd_mock INTERFACE dnssdStaticMock)
      # set_target_properties(dnssd PROPERTIES IMPORTED_LOCATION $<TARGET_FILE:dnssdStatic>)
      set(DNS_SD_DLL $<TARGET_FILE:dnssd_etc> PARENT_SCOPE)

    elseif(MDNSWINDOWS_INSTALL_LOC) # Add mDNSWindows as a downloaded binary package

      get_filename_component(MDNSWINDOWS_INSTALL_LOC ${MDNSWINDOWS_INSTALL_LOC}
        ABSOLUTE
        BASE_DIR ${CMAKE_BINARY_DIR}
      )
      if(NOT EXISTS ${MDNSWINDOWS_INSTALL_LOC}/lib)
        message(FATAL_ERROR "${MDNSWINDOWS_INSTALL_LOC} does not seem to contain a valid installation.")
      endif()

      # Setup the imported mDNSWindows library
      add_library(mdnswin STATIC IMPORTED)
      set_target_properties(mdnswin PROPERTIES IMPORTED_LOCATION ${MDNSWINDOWS_INSTALL_LOC}/lib/dnssd.lib)
      target_include_directories(mdnswin INTERFACE ${MDNSWINDOWS_INSTALL_LOC}/include)
      target_link_libraries(dnssd INTERFACE mdnswin)

      # Setup the mock mDNSWindows library
      add_library(mdnswin_mock STATIC IMPORTED)
      set_target_properties(mdnswin_mock PROPERTIES IMPORTED_LOCATION ${MDNSWINDOWS_INSTALL_LOC}/lib/dnssd_mock.lib)
      target_include_directories(mdnswin_mock INTERFACE ${MDNSWINDOWS_INSTALL_LOC}/include)
      target_link_libraries(dnssd_mock INTERFACE mdnswin_mock)

      set(DNS_SD_DLL ${MDNSWINDOWS_INSTALL_LOC}/dll/dnssd.dll PARENT_SCOPE)
      configure_file(${RDMNET_ROOT}/tools/ci/mdnsmerge.wxi.in
        ${RDMNET_ROOT}/tools/install/windows/GeneratedFiles/mdnsmerge.wxi
      )

    endif()
  endif()
elseif(APPLE)
  set(RDMNET_DISCOVERY_ADDITIONAL_SOURCES
    ${RDMNET_SRC}/rdmnet/discovery/bonjour.h
    ${RDMNET_SRC}/rdmnet/discovery/bonjour.c
  )
else()
  message(FATAL_ERROR "There is currently no DNS-SD provider supported for this platform.")
endif()

target_sources(RDMnetDiscovery INTERFACE ${RDMNET_DISCOVERY_ADDITIONAL_SOURCES})
