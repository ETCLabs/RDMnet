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

set(RDMNET_LIB_PUBLIC_HEADERS 
  ${RDMNET_INCLUDE}/rdmnet/client.h
  ${RDMNET_INCLUDE}/rdmnet/controller.h
  ${RDMNET_INCLUDE}/rdmnet/defs.h
  ${RDMNET_INCLUDE}/rdmnet/device.h
  ${RDMNET_INCLUDE}/rdmnet/llrp.h
  ${RDMNET_INCLUDE}/rdmnet/version.h

  ${RDMNET_INCLUDE}/rdmnet/core/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/connection.h
  ${RDMNET_INCLUDE}/rdmnet/core/discovery.h
  ${RDMNET_INCLUDE}/rdmnet/core/ept_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/message.h
  ${RDMNET_INCLUDE}/rdmnet/core/rpt_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/util.h
)

set(RDMNET_LIB_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/private/broker_prot.h
  ${RDMNET_SRC}/rdmnet/private/client.h
  ${RDMNET_SRC}/rdmnet/private/connection.h
  ${RDMNET_SRC}/rdmnet/private/controller.h
  ${RDMNET_SRC}/rdmnet/private/device.h
  ${RDMNET_SRC}/rdmnet/private/llrp_prot.h
  ${RDMNET_SRC}/rdmnet/private/llrp.h
  ${RDMNET_SRC}/rdmnet/private/message.h
  ${RDMNET_SRC}/rdmnet/private/msg_buf.h
  ${RDMNET_SRC}/rdmnet/private/opts.h
  ${RDMNET_SRC}/rdmnet/private/rpt_prot.h
)

set(RDMNET_LIB_SOURCES
  ${RDMNET_SRC}/rdmnet/client.c
  ${RDMNET_SRC}/rdmnet/controller.c
  ${RDMNET_SRC}/rdmnet/device.c

  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/connection.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)

add_library(RDMnet
  ${RDMNET_LIB_PUBLIC_HEADERS}
  ${RDMNET_LIB_PRIVATE_HEADERS}
  ${RDMNET_LIB_SOURCES}
  ${DNS_SD_ADDITIONAL_SOURCES}
)

# Organize sources in IDEs
source_group(TREE ${RDMNET_SRC}/rdmnet PREFIX src FILES
  ${RDMNET_LIB_SOURCES}
  ${DNS_SD_ADDITIONAL_SOURCES}
)
source_group(TREE ${RDMNET_INCLUDE}/rdmnet PREFIX include FILES ${RDMNET_LIB_PUBLIC_HEADERS})
source_group(TREE ${RDMNET_SRC}/rdmnet PREFIX include FILES ${RDMNET_LIB_PRIVATE_HEADERS})

target_include_directories(RDMnet
  PUBLIC ${RDMNET_INCLUDE}
  PRIVATE ${RDMNET_SRC} ${DNS_SD_ADDITIONAL_INCLUDE_DIRS}
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
