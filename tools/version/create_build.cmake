if(NOT NEW_VERSION_NUMBER)
  message(FATAL_ERROR "You must pass a version number!")
endif()

set(RDMNET_VERSION_STRING ${NEW_VERSION_NUMBER})
string(REPLACE "." ";" RDMNET_VERSION_LIST ${RDMNET_VERSION_STRING})

# Build the variables we will need to configure the version header
list(GET RDMNET_VERSION_LIST 0 RDMNET_VERSION_MAJOR)
list(GET RDMNET_VERSION_LIST 1 RDMNET_VERSION_MINOR)
list(GET RDMNET_VERSION_LIST 2 RDMNET_VERSION_PATCH)
list(GET RDMNET_VERSION_LIST 3 RDMNET_VERSION_BUILD)
string(TIMESTAMP RDMNET_VERSION_DATESTR "%Y-%m-%d")
set(RDMNET_VERSION_COPYRIGHT "Copyright 2018 ETC Inc.")

# Configure the version header
message(STATUS "Configuring versioned build for ${RDMNET_VERSION_STRING}...")
get_filename_component(VERSION_DIR ${CMAKE_SCRIPT_MODE_FILE} DIRECTORY)
configure_file(${VERSION_DIR}/templates/version.h.in ${VERSION_DIR}/../../include/rdmnet/version.h)
configure_file(${VERSION_DIR}/templates/vars.wxi.in ${VERSION_DIR}/../install/windows/vars.wxi)
configure_file(${VERSION_DIR}/templates/current_version.txt.in ${VERSION_DIR}/current_version.txt)

# Stage the changed files
execute_process(COMMAND
  git add include/rdmnet/version.h
  WORKING_DIRECTORY ${VERSION_DIR}/../..
)
execute_process(COMMAND
  git add tools/install/windows/vars.wxi
  WORKING_DIRECTORY ${VERSION_DIR}/../..
)
configure_file(${VERSION_DIR}/templates/commit_msg.txt.in ${VERSION_DIR}/commit_msg.txt)