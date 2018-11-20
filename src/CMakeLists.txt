set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(RDMNET_INCLUDE ${RDMNET_ROOT}/include)

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

if(WIN32)

  #NB-W TESTING
  add_subdirectory(c:/p4/nb-w_mdns/Bonjour c:/p4/nb-w_mdns/Bonjour/build)
  set(DNS_SD_ADDITIONAL_INCLUDE_DIRS c:/p4/nb-w_mdns/Bonjour/mDNSShared)
  set(DNS_SD_ADDITIONAL_SOURCES
    common/discovery/bonjour/rdmnet_discovery_bonjour.h
    common/discovery/bonjour/rdmnet_discovery_bonjour.c
  )
  set(DNS_SD_ADDITIONAL_LIBS C:/p4/nb-w_mdns/Bonjour/build/mDNSWindows/DLLStub/Release/dnssdStatic.lib)

else()
  message(FATAL_ERROR "There is currently no DNS-SD provider supported for this platform.")
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
  ${RDMNET_INCLUDE}/rdmnet/common/opts.h
  ${RDMNET_INCLUDE}/rdmnet/common/rpt_prot.h

  # Private headers
  common/broker_prot_priv.h
  common/llrp_priv.h
  common/llrp_prot_priv.h
  common/rdmnet_conn_priv.h
  common/rdmnet_message_priv.h
  common/rdmnet_msg_buf.h
  common/rpt_prot_priv.h

  # Sources
  common/broker_prot.c
  common/llrp_prot.c
  common/llrp.c
  common/rdmnet_client.c
  common/rdmnet_connection.c
  common/rdmnet_message.c
  common/rdmnet_msg_buf.c
  common/rpt_prot.c

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