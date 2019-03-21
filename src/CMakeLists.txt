set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(RDMNET_INCLUDE ${RDMNET_ROOT}/include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR})

################################# Dependencies ################################

include(AddDependency)

if(RDMNET_BUILD_TESTS)
  set(LWPA_BUILD_MOCK_LIB ON)
  # set(RDM_BUILD_MOCK_LIB ON)
endif()
rdmnet_add_dependency(lwpa LWPA_DIR)
rdmnet_add_dependency(RDM RDM_DIR)

################################## Config file ################################

if(DEFINED RDMNET_CONFIG_LOC)
  get_filename_component(RDMNET_CONFIG_LOC ${RDMNET_CONFIG_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
endif()

################################ DNS-SD SUPPORT ###############################

include(ResolveDnsSdProvider)

################################### Library ###################################

# RDMnet sources and headers are divided logically into groups - these groups
# are used to create interface libraries which separate portions of RDMnet for
# unit testing. They are then combined into the encompassing RDMnet library for
# external consumption.

# The "RDMnet API" layer: Top-level public API logic called by applications.

set(RDMNET_API_PUBLIC_HEADERS
  ${RDMNET_INCLUDE}/rdmnet/client.h
  ${RDMNET_INCLUDE}/rdmnet/controller.h
  ${RDMNET_INCLUDE}/rdmnet/defs.h
  ${RDMNET_INCLUDE}/rdmnet/device.h
  ${RDMNET_INCLUDE}/rdmnet/llrp.h
  ${RDMNET_INCLUDE}/rdmnet/version.h
)
set(RDMNET_API_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/private/client.h
  ${RDMNET_SRC}/rdmnet/private/controller.h
  ${RDMNET_SRC}/rdmnet/private/device.h
  ${RDMNET_SRC}/rdmnet/private/llrp.h
)
set(RDMNET_API_SOURCES
  ${RDMNET_SRC}/rdmnet/client.c
  # ${RDMNET_SRC}/rdmnet/controller.c
  ${RDMNET_SRC}/rdmnet/device.c
  ""
)

# This target should not be used externally - it is only for use by the automated tests.
add_library(RDMnetApi INTERFACE)
target_sources(RDMnetApi INTERFACE
  ${RDMNET_API_PUBLIC_HEADERS}
  ${RDMNET_API_PRIVATE_HEADERS}
  ${RDMNET_API_SOURCES}
)
target_include_directories(RDMnetApi INTERFACE ${RDMNET_INCLUDE} ${RDMNET_SRC})

# The "RDMnet Core" layer: Lower-level logic shared by the Client APIs and the
# Broker library.

set(RDMNET_CORE_PUBLIC_HEADERS
  ${RDMNET_INCLUDE}/rdmnet/core.h
  ${RDMNET_INCLUDE}/rdmnet/core/broker_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/connection.h
  ${RDMNET_INCLUDE}/rdmnet/core/ept_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/message.h
  ${RDMNET_INCLUDE}/rdmnet/core/rpt_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/util.h
)
set(RDMNET_CORE_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/private/broker_prot.h
  ${RDMNET_SRC}/rdmnet/private/core.h
  ${RDMNET_SRC}/rdmnet/private/connection.h
  ${RDMNET_SRC}/rdmnet/private/llrp_prot.h
  ${RDMNET_SRC}/rdmnet/private/message.h
  ${RDMNET_SRC}/rdmnet/private/msg_buf.h
  ${RDMNET_SRC}/rdmnet/private/opts.h
  ${RDMNET_SRC}/rdmnet/private/rpt_prot.h
)
set(RDMNET_CORE_SOURCES
  ${RDMNET_SRC}/rdmnet/core.c
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/connection.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/message.c
  ${RDMNET_SRC}/rdmnet/core/msg_buf.c
  ${RDMNET_SRC}/rdmnet/core/rpt_prot.c
  ${RDMNET_SRC}/rdmnet/core/util.c
)

# This target should not be used externally - it is only for use by the automated tests.
add_library(RDMnetCore INTERFACE)
target_sources(RDMnetCore INTERFACE
  ${RDMNET_CORE_PUBLIC_HEADERS}
  ${RDMNET_CORE_PRIVATE_HEADERS}
  ${RDMNET_CORE_SOURCES}
)
target_include_directories(RDMnetCore INTERFACE ${RDMNET_INCLUDE} ${RDMNET_SRC})

# Combination variables for convenience

set(RDMNET_LIB_PUBLIC_HEADERS 
  ${RDMNET_API_PUBLIC_HEADERS}
  ${RDMNET_CORE_PUBLIC_HEADERS}
)
set(RDMNET_LIB_PRIVATE_HEADERS
  ${RDMNET_API_PRIVATE_HEADERS}
  ${RDMNET_CORE_PRIVATE_HEADERS}
)
set(RDMNET_LIB_SOURCES
  ${RDMNET_API_SOURCES}
  ${RDMNET_CORE_SOURCES}
)

# The main RDMnet library
add_library(RDMnet
  ${RDMNET_LIB_PUBLIC_HEADERS}
  ${RDMNET_LIB_PRIVATE_HEADERS}
  ${RDMNET_LIB_SOURCES}
)
target_include_directories(RDMnet
  PUBLIC ${RDMNET_INCLUDE}
  PRIVATE ${RDMNET_SRC}
)
target_link_libraries(RDMnet
  PUBLIC lwpa RDM
  PRIVATE RDMnetDiscovery dnssd
)

# Organize sources in IDEs
source_group(include FILES ${RDMNET_API_PUBLIC_HEADERS})
source_group(src FILES ${RDMNET_API_SOURCES})
source_group(src\\core FILES ${RDMNET_CORE_SOURCES})
source_group(include\\core FILES ${RDMNET_CORE_PUBLIC_HEADERS})
source_group(include\\private FILES ${RDMNET_CORE_PRIVATE_HEADERS} ${RDMNET_API_PRIVATE_HEADERS})

################################ Mock Library #################################

# RDMnet mock libraries, for unit testing
option(RDMNET_BUILD_MOCK_LIB "Build the RDMnet mock libraries" ${RDMNET_BUILD_TESTS})
if(RDMNET_BUILD_MOCK_LIB)
  add_library(RDMnetCore_mock
    # Mock sources
    ${RDMNET_INCLUDE}/rdmnet_mock/core.h
    ${RDMNET_INCLUDE}/rdmnet_mock/core/discovery.h
    ${RDMNET_INCLUDE}/rdmnet_mock/core/connection.h

    ${RDMNET_SRC}/rdmnet_mock/core.c
    ${RDMNET_SRC}/rdmnet_mock/core/discovery.c
    ${RDMNET_SRC}/rdmnet_mock/core/connection.c

    # Real sources
    ${RDMNET_SRC}/rdmnet/core/util.c
  )
  target_include_directories(RDMnetCore_mock PUBLIC ${RDMNET_INCLUDE})
  target_link_libraries(RDMnetCore_mock PUBLIC meekrosoft::fff PRIVATE lwpa RDM)
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
