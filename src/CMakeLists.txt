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

  ${RDMNET_INCLUDE}/rdmnet/core/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/connection.h
  ${RDMNET_INCLUDE}/rdmnet/core/discovery.h
  ${RDMNET_INCLUDE}/rdmnet/core/ept_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/message.h
  ${RDMNET_INCLUDE}/rdmnet/core/rpt_prot.h

  # Private headers
  ${RDMNET_SRC}/rdmnet/core/rdmnet_opts.h
  ${RDMNET_SRC}/rdmnet/core/broker_prot_priv.h
  ${RDMNET_SRC}/rdmnet/core/llrp_priv.h
  ${RDMNET_SRC}/rdmnet/core/llrp_prot_priv.h
  ${RDMNET_SRC}/rdmnet/core/connection_priv.h
  ${RDMNET_SRC}/rdmnet/core/message_priv.h
  ${RDMNET_SRC}/rdmnet/core/msg_buf.h
  ${RDMNET_SRC}/rdmnet/core/rpt_prot_priv.h

  # Sources
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/client.c
  ${RDMNET_SRC}/rdmnet/core/connection.c
  ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.c

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

  rdmnet/broker/broker_core.h
  rdmnet/broker/broker_core.cpp
  rdmnet/broker/broker_client.h
  rdmnet/broker/broker_client.cpp
  rdmnet/broker/broker_discovery.h
  rdmnet/broker/broker_discovery.cpp
  rdmnet/broker/broker_log.cpp
  rdmnet/broker/broker_responder.h
  rdmnet/broker/broker_responder.cpp
  rdmnet/broker/broker_threads.h
  rdmnet/broker/broker_threads.cpp
  rdmnet/broker/broker_uid_manager.h
  rdmnet/broker/broker_uid_manager.cpp
  rdmnet/broker/broker_util.h
  rdmnet/broker/broker_util.cpp
)
target_include_directories(Broker PUBLIC ${RDMNET_INCLUDE})
target_link_libraries(Broker PUBLIC RDMnet)