# RDMnet source manifest

# RDMnet sources and headers are divided logically into groups - these groups are used to separate
# portions of RDMnet for unit testing. They are then combined into the encompassing RDMnet library
# for external consumption.

# The "RDMnet API" layer: Top-level public API logic called by applications.

set(RDMNET_API_PUBLIC_HEADERS
  ${RDMNET_INCLUDE}/rdmnet/client.h
  ${RDMNET_INCLUDE}/rdmnet/common.h
  ${RDMNET_INCLUDE}/rdmnet/controller.h
  ${RDMNET_INCLUDE}/rdmnet/defs.h
  ${RDMNET_INCLUDE}/rdmnet/device.h
  ${RDMNET_INCLUDE}/rdmnet/discovery.h
  ${RDMNET_INCLUDE}/rdmnet/ept_client.h
  ${RDMNET_INCLUDE}/rdmnet/llrp_manager.h
  ${RDMNET_INCLUDE}/rdmnet/llrp_target.h
  ${RDMNET_INCLUDE}/rdmnet/llrp.h
  ${RDMNET_INCLUDE}/rdmnet/message.h
  ${RDMNET_INCLUDE}/rdmnet/version.h
)
set(RDMNET_API_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/private/controller.h
  ${RDMNET_SRC}/rdmnet/private/device.h
  ${RDMNET_SRC}/rdmnet/private/llrp_manager.h
  ${RDMNET_SRC}/rdmnet/private/llrp_target.h
  ${RDMNET_SRC}/rdmnet/private/opts.h
)
set(RDMNET_API_SOURCES
  ${RDMNET_SRC}/rdmnet/common.c
  ${RDMNET_SRC}/rdmnet/controller.c
  ${RDMNET_SRC}/rdmnet/device.c
  ${RDMNET_SRC}/rdmnet/llrp.c
  ${RDMNET_SRC}/rdmnet/llrp_manager.c
  ${RDMNET_SRC}/rdmnet/llrp_target.c
  ${RDMNET_SRC}/rdmnet/message.c
)

# The "RDMnet Core" layer: Lower-level logic shared by the Client APIs and the Broker library.

set(RDMNET_CORE_HEADERS
  ${RDMNET_SRC}/rdmnet/core/broker_prot.h
  ${RDMNET_SRC}/rdmnet/core/client.h
  ${RDMNET_SRC}/rdmnet/core/client_entry.h
  ${RDMNET_SRC}/rdmnet/core/common.h
  ${RDMNET_SRC}/rdmnet/core/connection.h
  ${RDMNET_SRC}/rdmnet/core/ept_prot.h
  ${RDMNET_SRC}/rdmnet/core/llrp.h
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.h
  ${RDMNET_SRC}/rdmnet/core/mcast.h
  ${RDMNET_SRC}/rdmnet/core/message.h
  ${RDMNET_SRC}/rdmnet/core/msg_buf.h
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.h
  ${RDMNET_SRC}/rdmnet/core/util.h
)
set(RDMNET_CORE_SOURCES
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/client.c
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
  ${RDMNET_SRC}/rdmnet/core/common.c
  ${RDMNET_SRC}/rdmnet/core/connection.c
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c
  ${RDMNET_SRC}/rdmnet/core/mcast.c
  # ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)

# Combination variables for convenience

set(RDMNET_LIB_PUBLIC_HEADERS 
  ${RDMNET_API_PUBLIC_HEADERS}
)
set(RDMNET_LIB_PRIVATE_HEADERS
  ${RDMNET_API_PRIVATE_HEADERS}
  ${RDMNET_CORE_HEADERS}
)
set(RDMNET_LIB_SOURCES
  ${RDMNET_API_SOURCES}
  ${RDMNET_CORE_SOURCES}
)

set(RDMNET_BROKER_PUBLIC_HEADERS
  ${RDMNET_INCLUDE}/rdmnet/broker.h
)
set(RDMNET_BROKER_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/broker/broker_core.h
  ${RDMNET_SRC}/rdmnet/broker/broker_client.h
  ${RDMNET_SRC}/rdmnet/broker/broker_discovery.h
  ${RDMNET_SRC}/rdmnet/broker/broker_responder.h
  ${RDMNET_SRC}/rdmnet/broker/broker_socket_manager.h
  ${RDMNET_SRC}/rdmnet/broker/broker_threads.h
  ${RDMNET_SRC}/rdmnet/broker/broker_uid_manager.h
  ${RDMNET_SRC}/rdmnet/broker/broker_util.h
  ${RDMNET_SRC}/rdmnet/broker/rdmnet_conn_wrapper.h
)
set(RDMNET_BROKER_SOURCES
  ${RDMNET_SRC}/rdmnet/broker/broker_api.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_core.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_client.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_discovery.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_responder.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_threads.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_uid_manager.cpp
  ${RDMNET_SRC}/rdmnet/broker/broker_util.cpp
  ${RDMNET_SRC}/rdmnet/broker/rdmnet_conn_wrapper.cpp
)

if(WIN32)
  set(RDMNET_BROKER_PRIVATE_HEADERS ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_SRC}/rdmnet/broker/windows/win_socket_manager.h
  )
  set(RDMNET_BROKER_SOURCES ${RDMNET_BROKER_SOURCES}
    ${RDMNET_SRC}/rdmnet/broker/windows/win_socket_manager.cpp
  )
elseif(APPLE)
  set(RDMNET_BROKER_PRIVATE_HEADERS ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_SRC}/rdmnet/broker/macos/macos_socket_manager.h
  )
  set(RDMNET_BROKER_SOURCES ${RDMNET_BROKER_SOURCES}
    ${RDMNET_SRC}/rdmnet/broker/macos/macos_socket_manager.cpp
  )
elseif(UNIX)
  set(RDMNET_BROKER_PRIVATE_HEADERS ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_SRC}/rdmnet/broker/linux/linux_socket_manager.h
  )
  set(RDMNET_BROKER_SOURCES ${RDMNET_BROKER_SOURCES}
    ${RDMNET_SRC}/rdmnet/broker/linux/linux_socket_manager.cpp
  )
endif()
