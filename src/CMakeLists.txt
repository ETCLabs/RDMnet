set(RDMNET_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(RDMNET_INCLUDE ${RDMNET_ROOT}/include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR})

################################ DNS-SD SUPPORT ###############################

include(ResolveDnsSdProvider)

################################# Dependencies ################################

include(RdmnetAddDependency)

if(RDMNET_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
  # set(RDM_BUILD_MOCK_LIB ON)
endif()
rdmnet_add_dependency(EtcPal ETCPAL_DIR)
rdmnet_add_dependency(RDM RDM_DIR)

set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)

################################## Libraries ##################################

# The main library
add_subdirectory(rdmnet)

# The mock libraries
option(RDMNET_BUILD_MOCK_LIB "Build the RDMnet mock libraries" ${RDMNET_BUILD_TESTS})
if(RDMNET_BUILD_MOCK_LIB)
  add_subdirectory(rdmnet_mock)
endif()
