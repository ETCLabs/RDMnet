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

if(DEFINED RDMNET_CONFIG_LOC)
else()
  message("No rdmnet_config.h provided. Creating a default one in the build directory...")
  configure_file(${RDMNET_ROOT}/tools/cmake/rdmnet_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/rdmnet_config.h)
  set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

################################ DNS-SD SUPPORT ###############################

include(ResolveDnsSdProvider)

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