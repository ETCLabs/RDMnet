set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(RDMNET_INCLUDE ${RDMNET_ROOT}/include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR})

################################# Dependencies ################################

if(NOT TARGET lwpa)
  if(NOT DEFINED LWPA_DIR)
    # Set a default for the lwpa location; assume it is named 'lwpa' and at the same level as
    # RDMnet.
    set(LWPA_DIR ${RDMNET_ROOT}/../lwpa)
  endif()
  add_subdirectory(${LWPA_DIR} lwpa)
endif()

if(NOT TARGET RDM)
  if(NOT DEFINED RDM_DIR)
    # Set a default for the RDM location; assume it is named 'RDM' and at the same level as RDMnet.
    set(RDM_DIR ${RDMNET_ROOT}/../RDM)
  endif()
  add_subdirectory(${RDM_DIR} RDM)
endif()

################################## Config file ################################

if(WIN32)
  set(RDMNET_BASE_CONFIG "#define RDMNET_PLATFORM_WINDOWS 1")
endif()

if(DEFINED RDMNET_CONFIG_LOC)
else()
  message("No rdmnet_config.h provided. Creating a default one in the build directory...")
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/rdmnet_config.h ${RDMNET_BASE_CONFIG})
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

################################ DNS-SD SUPPORT ###############################

option(RDMNET_MOCK_DISCOVERY "Use the empty mock files for the DNS-SD provider" OFF)
option(RDMNET_WINDOWS_USE_BONJOUR_SDK
  "Use Apple's Bonjour SDK for Windows (LICENSING RESTRICTIONS MAY APPLY)" ON)

if(RDMNET_MOCK_DISCOVERY)
  set(DNS_SD_ADDITIONAL_SOURCES ${RDMNET_SRC}/common/discovery/mock/rdmnet_mock_discovery.c)
else()
  if(WIN32)
    set(DNS_SD_ADDITIONAL_SOURCES
      ${RDMNET_SRC}/common/discovery/bonjour/rdmnet_discovery_bonjour.h
      ${RDMNET_SRC}/common/discovery/bonjour/rdmnet_discovery_bonjour.c
    )
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
    else()
      if(NOT RDMNET_WINDOWS_BONJOUR_SRC_LOC AND NOT RDMNET_WINDOWS_BONJOUR_INSTALL_LOC)
        message(FATAL_ERROR
          "You must provide Bonjour for Windows from source in one of the following ways:\n"
          " - Use RDMNET_WINDOWS_BONJOUR_SRC_LOC to specify the location of the source repository\n"
          " - Use RDMNET_WINDOWS_BONJOUR_INSTALL_LOC to specify the location of the installed binaries\n"
        )
      endif()

      if (RDMNET_WINDOWS_BONJOUR_SRC_LOC)
        add_subdirectory(${RDMNET_WINDOWS_BONJOUR_SRC_LOC}/mDNSWindows/DLLStub Bonjour)
        set(DNS_SD_ADDITIONAL_LIBS dnssdStatic)
      elseif(RDMNET_WINDOWS_BONJOUR_INSTALL_LOC)
        message(FATAL_ERROR "This option is not yet implemented.")
      endif()
    endif()
  else()
    message(FATAL_ERROR "There is currently no DNS-SD provider supported for this platform.")
  endif()
endif()

################################### Library ###################################

add_library(RDMnet
  # Public headers
  ${RDMNET_INCLUDE}/rdmnet/client.h
  ${RDMNET_INCLUDE}/rdmnet/llrp.h
  ${RDMNET_INCLUDE}/rdmnet/version.h

  ${RDMNET_INCLUDE}/rdmnet/common/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet/common/connection.h
  ${RDMNET_INCLUDE}/rdmnet/common/discovery.h
  ${RDMNET_INCLUDE}/rdmnet/common/ept_prot.h
  ${RDMNET_INCLUDE}/rdmnet/common/message.h
  ${RDMNET_INCLUDE}/rdmnet/common/rpt_prot.h

  # Private headers
  ${RDMNET_SRC}/common/rdmnet_opts.h
  ${RDMNET_SRC}/common/broker_prot_priv.h
  ${RDMNET_SRC}/common/llrp_priv.h
  ${RDMNET_SRC}/common/llrp_prot_priv.h
  ${RDMNET_SRC}/common/rdmnet_conn_priv.h
  ${RDMNET_SRC}/common/rdmnet_message_priv.h
  ${RDMNET_SRC}/common/rdmnet_msg_buf.h
  ${RDMNET_SRC}/common/rpt_prot_priv.h

  # Sources
  ${RDMNET_SRC}/common/broker_prot.c
  ${RDMNET_SRC}/common/llrp_prot.c
  ${RDMNET_SRC}/common/llrp.c
  ${RDMNET_SRC}/common/rdmnet_client.c
  ${RDMNET_SRC}/common/rdmnet_connection.c
  ${RDMNET_SRC}/common/rdmnet_message.c
  ${RDMNET_SRC}/common/rdmnet_msg_buf.c
  ${RDMNET_SRC}/common/rpt_prot.c

  ${DNS_SD_ADDITIONAL_SOURCES}
)
target_include_directories(RDMnet
  PUBLIC ${RDMNET_INCLUDE}
  PRIVATE ${DNS_SD_ADDITIONAL_INCLUDE_DIRS}
)
target_link_libraries(RDMnet PUBLIC lwpa RDM ${DNS_SD_ADDITIONAL_LIBS})

#################################### Broker ###################################

add_library(Broker
  # Broker public headers
  ${RDMNET_INCLUDE}/rdmnet/broker.h
  ${RDMNET_INCLUDE}/rdmnet/broker/client.h
  ${RDMNET_INCLUDE}/rdmnet/broker/discovery.h
  ${RDMNET_INCLUDE}/rdmnet/broker/responder.h
  ${RDMNET_INCLUDE}/rdmnet/broker/threads.h
  ${RDMNET_INCLUDE}/rdmnet/broker/util.h

  broker/broker.cpp
  broker/broker_client.cpp
  broker/broker_consts.h
  broker/broker_discovery.cpp
  broker/broker_responder.cpp
  broker/broker_threads.cpp
  broker/broker_util.cpp
)
target_include_directories(Broker PUBLIC ${RDMNET_INCLUDE})
target_link_libraries(Broker PUBLIC RDMnet)