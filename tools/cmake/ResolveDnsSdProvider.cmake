# Determines the provider of mDNS and DNS-SD support based on the compilation
# platform and user input variables.
#
# Sets the following variables which are used by source builds:
# - DNS_SD_ADDITIONAL_SOURCES: Source files to be added to the RDMnet library compilation
# - DNS_SD_ADDITIONAL_INCLUDE_DIRS: Include directories to be added to the RDMnet library compilation
# - DNS_SD_ADDITIONAL_LIBS: Static libs to be added to the RDMnet library compilation

option(RDMNET_MOCK_DISCOVERY "Use the empty mock files for the DNS-SD provider" OFF)
option(RDMNET_WINDOWS_USE_BONJOUR_SDK
  "Use Apple's Bonjour SDK for Windows (LICENSING RESTRICTIONS MAY APPLY)" OFF)

if(RDMNET_MOCK_DISCOVERY)
  set(DNS_SD_ADDITIONAL_SOURCES ${RDMNET_SRC}/common/discovery/mock/rdmnet_mock_discovery.c)
else()
  if(WIN32)
    # On Windows, we use Bonjour for Windows, either through the Bonjour SDK or ETC's Bonjour fork.
    set(DNS_SD_ADDITIONAL_SOURCES
      ${RDMNET_SRC}/common/discovery/bonjour/rdmnet_discovery_bonjour.h
      ${RDMNET_SRC}/common/discovery/bonjour/rdmnet_discovery_bonjour.c
    )

    # Using Apple's Bonjour SDK for Windows
    if(RDMNET_WINDOWS_USE_BONJOUR_SDK)

      # The location of the Bonjour SDK
      if(NOT DEFINED BONJOUR_SDK_ROOT)
        set(BONJOUR_SDK_ROOT $ENV{BONJOUR_SDK_HOME})
      endif()

      # Find the Bonjour SDK lib
      set(DNS_SD_ADDITIONAL_INCLUDE_DIRS ${BONJOUR_SDK_ROOT}/Include)
      # Is this really the way to determine if we are 64-bit? Seems dumb...
      if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(BONJOUR_LIB_DIR ${BONJOUR_SDK_ROOT}/Lib/x64)
      else()
        set(BONJOUR_LIB_DIR ${BONJOUR_SDK_ROOT}/Lib/Win32)
      endif()
      find_library(BONJOUR_LIB
        NAMES dnssd
        HINTS ${BONJOUR_LIB_DIR}
      )

      if(BONJOUR_LIB_NOTFOUND)
        message(FATAL_ERROR "Make sure the Bonjour SDK is installed, and try again.")
      endif()

      set(DNS_SD_ADDITIONAL_LIBS ${BONJOUR_LIB})

    else() # Using ETC's Bonjour fork

      if(NOT DEFINED RDMNET_WINDOWS_BONJOUR_SRC_LOC AND NOT DEFINED RDMNET_WINDOWS_BONJOUR_INSTALL_LOC)
        message(STATUS
          "Neither RDMNET_WINDOWS_BONJOUR_SRC_LOC or RRDMNET_WINDOWS_BONJOUR_INSTALL_LOC provided.\n"
          "Looking for source repository named 'mDNSWindows' at same level as RDMnet..."
        )
        if(EXISTS ${RDMNET_ROOT}/../mDNSWindows)
          message(STATUS "Found. Adding dependency from RDMNET_DIR/../mDNSWindows.")
          set(RDMNET_WINDOWS_BONJOUR_SRC_LOC ${RDMNET_ROOT}/../mDNSWindows)
        else()
          message(FATAL_ERROR
            "You must provide ETC's Bonjour for Windows fork (mDNSWindows) in one of the following ways:\n"
            " - Use RDMNET_WINDOWS_BONJOUR_SRC_LOC to specify the location of the source repository\n"
            " - Use RDMNET_WINDOWS_BONJOUR_INSTALL_LOC to specify the location of the installed binaries\n"
            " - Clone the ETCLabs/mDNSWindows repository at the same directory level as the RDMnet repository\n"
          )
        endif()
      endif()

      if(RDMNET_WINDOWS_BONJOUR_SRC_LOC)
        add_subdirectory(${RDMNET_WINDOWS_BONJOUR_SRC_LOC}/mDNSWindows/DLLStub Bonjour)
        set(DNS_SD_ADDITIONAL_LIBS dnssdStatic)
      elseif(RDMNET_WINDOWS_BONJOUR_INSTALL_LOC)
        file(TO_CMAKE_PATH ${RDMNET_WINDOWS_BONJOUR_INSTALL_LOC} RDMNET_WINDOWS_BONJOUR_INSTALL_LOC)
        find_library(BONJOUR_LIB
          NAMES dnssd
          HINTS ${RDMNET_WINDOWS_BONJOUR_INSTALL_LOC}/lib
        )
        if(BONJOUR_LIB_NOTFOUND)
          message(FATAL_ERROR "${RDMNET_WINDOWS_BONJOUR_INSTALL_LOC} does not seem to contain a valid installation.")
        endif()

        set(DNS_SD_ADDITIONAL_INCLUDE_DIRS ${RDMNET_WINDOWS_BONJOUR_INSTALL_LOC}/include)
        set(DNS_SD_ADDITIONAL_LIBS ${BONJOUR_LIB})
        set(DNS_SD_DLL ${RDMNET_WINDOWS_BONJOUR_INSTALL_LOC}/dll/dnssd.dll PARENT_SCOPE)
        file(TO_NATIVE_PATH "${RDMNET_WINDOWS_BONJOUR_INSTALL_LOC}/merge_module" MDNS_MERGE_MODULE_LOC)
        configure_file(${RDMNET_ROOT}/tools/ci/mdnsmerge.wxi.in
          ${RDMNET_ROOT}/tools/install/windows/GeneratedFiles/mdnsmerge.wxi
        )

      endif()
    endif()
  else()
    message(FATAL_ERROR "There is currently no DNS-SD provider supported for this platform.")
  endif()
endif()
