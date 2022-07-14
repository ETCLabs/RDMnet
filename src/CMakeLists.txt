################################# Dependencies ################################

include(RdmnetAddDependency)

if(RDMNET_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
  set(RDM_BUILD_MOCK_LIB ON CACHE BOOL "Build the RDM mock library" FORCE)
endif()
rdmnet_add_dependency(EtcPal ETCPAL_DIR)
rdmnet_add_dependency(RDM RDM_DIR)

if(TARGET EtcPalMock)
  set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)
endif()
if(TARGET RDMMock)
  set_target_properties(RDMMock PROPERTIES FOLDER dependencies)
endif()

################################## Config file ################################

if(DEFINED RDMNET_CONFIG_LOC)
  get_filename_component(RDMNET_CONFIG_LOC ${RDMNET_CONFIG_LOC}
    ABSOLUTE
    BASE_DIR ${CMAKE_BINARY_DIR}
  )
endif()

################################ RDMnet library ###############################

set(RDMNET_LIB_TARGET_NAME RDMnet)
set(RDMNET_BROKER_LIB_TARGET_NAME RDMnetBroker)

set(RDMNET_CONFIG_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/config)
set(RDMNET_TARGETS_EXPORT_NAME ${PROJECT_NAME}Targets)

################################## Libraries ##################################

# Get the local source variables
include(rdmnet/sources.cmake)

# The main RDMnet library
add_library(${RDMNET_LIB_TARGET_NAME} 
  ${RDMNET_API_PUBLIC_HEADERS}
  ${RDMNET_API_PRIVATE_HEADERS}
  ${RDMNET_API_SOURCES}
  ${RDMNET_CORE_HEADERS}
  ${RDMNET_CORE_SOURCES}
  ${RDMNET_DISC_COMMON_SOURCES}
  ${RDMNET_DISC_PLATFORM_SOURCES}
)
target_include_directories(${RDMNET_LIB_TARGET_NAME}
  PUBLIC ${RDMNET_INCLUDE}
  PRIVATE ${RDMNET_SRC} ${RDMNET_DISC_PLATFORM_INCLUDE_DIRS}
)
if(DEFINED RDMNET_CONFIG_LOC)
  target_include_directories(${RDMNET_LIB_TARGET_NAME} PRIVATE ${RDMNET_CONFIG_LOC})
  target_compile_definitions(${RDMNET_LIB_TARGET_NAME} PRIVATE RDMNET_HAVE_CONFIG_H)
endif()
target_link_libraries(${RDMNET_LIB_TARGET_NAME} PUBLIC EtcPal RDM ${RDMNET_DISC_PLATFORM_LIBS})
if(RDMNET_DISC_PLATFORM_DEPENDENCIES)
  add_dependencies(${RDMNET_LIB_TARGET_NAME} ${RDMNET_DISC_PLATFORM_DEPENDENCIES})
endif()
set_target_properties(${RDMNET_LIB_TARGET_NAME} PROPERTIES COMPILE_PDB_NAME "${RDMNET_LIB_TARGET_NAME}")
set_target_properties(${RDMNET_LIB_TARGET_NAME} PROPERTIES COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# Organize sources in IDEs
source_group(TREE ${RDMNET_SRC}/rdmnet PREFIX src FILES
   ${RDMNET_API_SOURCES}
   ${RDMNET_CORE_SOURCES}
   ${RDMNET_API_PRIVATE_HEADERS}
   ${RDMNET_CORE_HEADERS}
   ${RDMNET_DISC_COMMON_SOURCES}
   ${RDMNET_DISC_PLATFORM_SOURCES}
)
source_group(TREE ${RDMNET_INCLUDE}/rdmnet PREFIX include FILES
   ${RDMNET_API_PUBLIC_HEADERS}
)

# Write a pkg-config file
set(RDMNET_PKGCONFIG_INTERMEDIATE_FILE ${CMAKE_BINARY_DIR}/${RDMNET_LIB_TARGET_NAME}.pc)
configure_file(${RDMNET_ROOT}/cmake/RDMnet.pc.in ${RDMNET_PKGCONFIG_INTERMEDIATE_FILE} @ONLY)

# Installation
install(TARGETS ${RDMNET_LIB_TARGET_NAME}
  EXPORT ${RDMNET_TARGETS_EXPORT_NAME}
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)
install(DIRECTORY ${RDMNET_ROOT}/include/rdmnet DESTINATION include FILES_MATCHING PATTERN "*.h")
install(FILES ${RDMNET_PKGCONFIG_INTERMEDIATE_FILE} DESTINATION lib/pkgconfig)

if(RDMNET_INSTALL_PDBS)
  get_target_property(RDMNET_PDB_OUTPUT_DIRECTORY ${RDMNET_LIB_TARGET_NAME} COMPILE_PDB_OUTPUT_DIRECTORY)
  get_target_property(RDMNET_PDB_NAME ${RDMNET_LIB_TARGET_NAME} COMPILE_PDB_NAME)
  install(FILES "${RDMNET_PDB_OUTPUT_DIRECTORY}/Debug/${RDMNET_PDB_NAME}.pdb" DESTINATION lib CONFIGURATIONS Debug OPTIONAL)
  install(FILES "${RDMNET_PDB_OUTPUT_DIRECTORY}/RelWithDebInfo/${RDMNET_PDB_NAME}.pdb" DESTINATION lib CONFIGURATIONS RelWithDebInfo OPTIONAL)
endif()

#################################### Broker ###################################

if(WIN32 OR APPLE OR UNIX)
  add_library(${RDMNET_BROKER_LIB_TARGET_NAME}
    ${RDMNET_BROKER_PUBLIC_HEADERS}
    ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_BROKER_SOURCES}
  )

  target_include_directories(${RDMNET_BROKER_LIB_TARGET_NAME} PUBLIC ${RDMNET_INCLUDE} PRIVATE ${RDMNET_SRC} ${RDMNET_SRC}/rdmnet/broker)
  target_link_libraries(${RDMNET_BROKER_LIB_TARGET_NAME} PUBLIC ${RDMNET_LIB_TARGET_NAME})
  set_target_properties(${RDMNET_BROKER_LIB_TARGET_NAME} PROPERTIES CXX_STANDARD 11)
  set_target_properties(${RDMNET_BROKER_LIB_TARGET_NAME} PROPERTIES COMPILE_PDB_NAME "${RDMNET_BROKER_LIB_TARGET_NAME}")
  set_target_properties(${RDMNET_BROKER_LIB_TARGET_NAME} PROPERTIES COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

  # Organize sources in IDEs
  source_group(TREE ${RDMNET_SRC}/rdmnet PREFIX src FILES
    ${RDMNET_BROKER_PRIVATE_HEADERS}
    ${RDMNET_BROKER_SOURCES}
  )
  source_group(TREE ${RDMNET_INCLUDE}/rdmnet PREFIX include FILES
    ${RDMNET_BROKER_PUBLIC_HEADERS}
  )

  # Write a pkg-config file
  set(RDMNET_BROKER_PKGCONFIG_INTERMEDIATE_FILE ${CMAKE_BINARY_DIR}/${RDMNET_BROKER_LIB_TARGET_NAME}.pc)
  configure_file(${RDMNET_ROOT}/cmake/RDMnetBroker.pc.in ${RDMNET_BROKER_PKGCONFIG_INTERMEDIATE_FILE} @ONLY)

  # Installation
  install(TARGETS ${RDMNET_BROKER_LIB_TARGET_NAME}
    EXPORT ${RDMNET_TARGETS_EXPORT_NAME}
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
  )
  install(FILES ${RDMNET_BROKER_PKGCONFIG_INTERMEDIATE_FILE} DESTINATION lib/pkgconfig)

  if(RDMNET_INSTALL_PDBS)
    get_target_property(RDMNET_BROKER_PDB_OUTPUT_DIRECTORY ${RDMNET_BROKER_LIB_TARGET_NAME} COMPILE_PDB_OUTPUT_DIRECTORY)
    get_target_property(RDMNET_BROKER_PDB_NAME ${RDMNET_BROKER_LIB_TARGET_NAME} COMPILE_PDB_NAME)
    install(FILES "${RDMNET_BROKER_PDB_OUTPUT_DIRECTORY}/Debug/${RDMNET_BROKER_PDB_NAME}.pdb" DESTINATION lib CONFIGURATIONS Debug OPTIONAL)
    install(FILES "${RDMNET_BROKER_PDB_OUTPUT_DIRECTORY}/RelWithDebInfo/${RDMNET_BROKER_PDB_NAME}.pdb" DESTINATION lib CONFIGURATIONS RelWithDebInfo OPTIONAL)
  endif()
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
