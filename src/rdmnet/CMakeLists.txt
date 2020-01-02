################################ RDMnet library ###############################

set(RDMNET_LIB_TARGET_NAME RDMnet)
set(RDMNET_BROKER_LIB_TARGET_NAME RDMnetBroker)

set(RDMNET_CONFIG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/config)
set(RDMNET_TARGETS_EXPORT_NAME ${PROJECT_NAME}Targets)

################################## Config file ################################

if(DEFINED RDMNET_CONFIG_LOC)
  get_filename_component(RDMNET_CONFIG_LOC ${RDMNET_CONFIG_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
endif()

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
  ${RDMNET_INCLUDE}/rdmnet/version.h
)
set(RDMNET_API_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/private/client.h
  ${RDMNET_SRC}/rdmnet/private/controller.h
  ${RDMNET_SRC}/rdmnet/private/device.h
)
set(RDMNET_API_SOURCES
  ${RDMNET_SRC}/rdmnet/client.c
  ${RDMNET_SRC}/rdmnet/controller.c
  ${RDMNET_SRC}/rdmnet/device.c
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
  ${RDMNET_INCLUDE}/rdmnet/core/llrp.h
  ${RDMNET_INCLUDE}/rdmnet/core/llrp_manager.h
  ${RDMNET_INCLUDE}/rdmnet/core/llrp_target.h
  ${RDMNET_INCLUDE}/rdmnet/core/message.h
  ${RDMNET_INCLUDE}/rdmnet/core/rpt_prot.h
  ${RDMNET_INCLUDE}/rdmnet/core/util.h
)
set(RDMNET_CORE_PRIVATE_HEADERS
  ${RDMNET_SRC}/rdmnet/private/broker_prot.h
  ${RDMNET_SRC}/rdmnet/private/core.h
  ${RDMNET_SRC}/rdmnet/private/connection.h
  ${RDMNET_SRC}/rdmnet/private/llrp.h
  ${RDMNET_SRC}/rdmnet/private/llrp_manager.h
  ${RDMNET_SRC}/rdmnet/private/llrp_prot.h
  ${RDMNET_SRC}/rdmnet/private/llrp_target.h
  ${RDMNET_SRC}/rdmnet/private/mcast.h
  ${RDMNET_SRC}/rdmnet/private/message.h
  ${RDMNET_SRC}/rdmnet/private/msg_buf.h
  ${RDMNET_SRC}/rdmnet/private/opts.h
  ${RDMNET_SRC}/rdmnet/private/rpt_prot.h
  ${RDMNET_SRC}/rdmnet/private/util.h
)
set(RDMNET_CORE_SOURCES
  ${RDMNET_SRC}/rdmnet/core.c
  ${RDMNET_SRC}/rdmnet/core/broker_prot.c
  ${RDMNET_SRC}/rdmnet/core/client_entry.c
  ${RDMNET_SRC}/rdmnet/core/connection.c
  ${RDMNET_SRC}/rdmnet/core/llrp.c
  ${RDMNET_SRC}/rdmnet/core/llrp_manager.c
  ${RDMNET_SRC}/rdmnet/core/llrp_prot.c
  ${RDMNET_SRC}/rdmnet/core/llrp_target.c
  ${RDMNET_SRC}/rdmnet/core/mcast.c
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
add_library(${RDMNET_LIB_TARGET_NAME}
  ${RDMNET_LIB_PUBLIC_HEADERS}
  ${RDMNET_LIB_PRIVATE_HEADERS}
  ${RDMNET_LIB_SOURCES}
)
target_include_directories(${RDMNET_LIB_TARGET_NAME}
  PUBLIC ${RDMNET_INCLUDE}
  PRIVATE ${RDMNET_SRC}
)
if(DEFINED RDMNET_CONFIG_LOC)
  target_include_directories(${RDMNET_LIB_TARGET_NAME} PRIVATE ${RDMNET_CONFIG_LOC})
  target_compile_definitions(${RDMNET_LIB_TARGET_NAME} PRIVATE RDMNET_HAVE_CONFIG_H)
endif()
target_link_libraries(${RDMNET_LIB_TARGET_NAME} PUBLIC EtcPal RDM)

# Organize sources in IDEs
source_group(TREE ${RDMNET_SRC}/rdmnet PREFIX src FILES
  ${RDMNET_API_SOURCES}
  ${RDMNET_CORE_SOURCES}
  ${RDMNET_API_PRIVATE_HEADERS}
  ${RDMNET_CORE_PRIVATE_HEADERS}
  ${RDMNET_DISC_COMMON_SOURCES}
  ${RDMNET_DISC_PLATFORM_SOURCES}
)
source_group(TREE ${RDMNET_INCLUDE}/rdmnet PREFIX include FILES
  ${RDMNET_API_PUBLIC_HEADERS}
  ${RDMNET_CORE_PUBLIC_HEADERS}
  ${RDMNET_DISC_PUBLIC_HEADERS}
)

target_link_libraries(${RDMNET_LIB_TARGET_NAME} PRIVATE RDMnetDiscoveryCommon RDMnetDiscoveryPlatform dnssd)

# Installation
install(TARGETS ${RDMNET_LIB_TARGET_NAME}
  EXPORT ${RDMNET_TARGETS_EXPORT_NAME}
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)
install(DIRECTORY ${RDMNET_ROOT}/include/rdmnet DESTINATION include FILES_MATCHING PATTERN "*.h")

#################################### Broker ###################################

if(WIN32 OR APPLE OR UNIX)
  set(RDMNET_BROKER_PUBLIC_HEADERS
    ${RDMNET_INCLUDE}/rdmnet/broker.h
    ${RDMNET_INCLUDE}/rdmnet/broker/settings.h
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

  # This target should not be used externally - it is only for use by the automated tests.
  add_library(RDMnetBrokerCore INTERFACE)
  target_sources(RDMnetBrokerCore INTERFACE
    ${RDMNET_BROKER_PUBLIC_HEADERS}
    ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_BROKER_SOURCES}
  )
  target_include_directories(RDMnetBrokerCore INTERFACE ${RDMNET_INCLUDE} ${RDMNET_SRC})

  add_library(${RDMNET_BROKER_LIB_TARGET_NAME}
    ${RDMNET_BROKER_PUBLIC_HEADERS}
    ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_BROKER_SOURCES}
  )

  target_include_directories(${RDMNET_BROKER_LIB_TARGET_NAME} PUBLIC ${RDMNET_INCLUDE} PRIVATE broker ${RDMNET_SRC})
  target_link_libraries(${RDMNET_BROKER_LIB_TARGET_NAME} PUBLIC ${RDMNET_LIB_TARGET_NAME})
  set_target_properties(${RDMNET_BROKER_LIB_TARGET_NAME} PROPERTIES CXX_STANDARD 14)

  # Organize sources in IDEs
  source_group(TREE ${RDMNET_SRC}/rdmnet PREFIX src FILES
    ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_BROKER_SOURCES}
  )
  source_group(TREE ${RDMNET_INCLUDE}/rdmnet PREFIX include FILES
    ${RDMNET_BROKER_PUBLIC_HEADERS}
  )

  install(TARGETS ${RDMNET_BROKER_LIB_TARGET_NAME}
    EXPORT ${RDMNET_TARGETS_EXPORT_NAME}
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
  )

endif()

## Create the CMake config files that will be exported with the built DLL

# Below section mostly working but still TODO, need to clean up use of interface libraries
# somehow

# include(CMakePackageConfigHelpers)
# 
# install(TARGETS EtcPal EXPORT ${RDMNET_TARGETS_EXPORT_NAME})
# install(TARGETS RDM EXPORT ${RDMNET_TARGETS_EXPORT_NAME})
# 
# # Establish the names of the config files
# set(RDMNET_CMAKE_VERSION_CONFIG_FILE ${RDMNET_CONFIG_OUTPUT_DIR}/${PROJECT_NAME}ConfigVersion.cmake)
# set(RDMNET_CMAKE_PROJECT_CONFIG_FILE ${RDMNET_CONFIG_OUTPUT_DIR}/${PROJECT_NAME}Config.cmake)
# 
# # Create the config files in a staging directory
# write_basic_package_version_file(${RDMNET_CMAKE_VERSION_CONFIG_FILE} COMPATIBILITY SameMajorVersion)
# configure_file(${RDMNET_ROOT}/cmake/config.cmake.in ${RDMNET_CMAKE_PROJECT_CONFIG_FILE} @ONLY)
# 
# # Specify the installation locations of the CMake config files
# install(FILES
#   ${RDMNET_CMAKE_PROJECT_CONFIG_FILE}
#   ${RDMNET_CMAKE_VERSION_CONFIG_FILE}
#   DESTINATION lib/cmake/${PROJECT_NAME}
# )
# install(EXPORT
#   ${RDMNET_TARGETS_EXPORT_NAME}
#   NAMESPACE ${PROJECT_NAME}::
#   DESTINATION lib/cmake/${PROJECT_NAME}
# )
