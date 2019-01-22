set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(RDMNET_INCLUDE ${RDMNET_ROOT}/include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR})

################################# Dependencies ################################

include(AddDependency)
rdmnet_add_dependency(lwpa LWPA_DIR)
rdmnet_add_dependency(RDM RDM_DIR)

################################## Config file ################################

if(DEFINED RDMNET_CONFIG_LOC)
  get_filename_component(RDMNET_CONFIG_LOC ${RDMNET_CONFIG_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
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
if(DNS_SD_BUILD_DEPENDENCIES)
  add_dependencies(RDMnet ${DNS_SD_BUILD_DEPENDENCIES})
endif()

#################################### Broker ###################################

add_library(Broker
  # Broker public headers
  ${RDMNET_INCLUDE}/rdmnet/broker.h
  ${RDMNET_INCLUDE}/rdmnet/broker/log.h

  broker/broker_core.h
  broker/broker_core.cpp
  broker/broker_client.h
  broker/broker_client.cpp
  broker/broker_discovery.h
  broker/broker_discovery.cpp
  broker/broker_log.cpp
  broker/broker_responder.h
  broker/broker_responder.cpp
  broker/broker_threads.h
  broker/broker_threads.cpp
  broker/broker_uid_manager.h
  broker/broker_uid_manager.cpp
  broker/broker_util.h
  broker/broker_util.cpp
)
target_include_directories(Broker PUBLIC ${RDMNET_INCLUDE})
target_link_libraries(Broker PUBLIC RDMnet)