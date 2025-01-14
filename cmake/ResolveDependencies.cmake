if(RDMNET_BUILD_TESTS)
  set(ETCPAL_BUILD_MOCK_LIB ON CACHE BOOL "Build the EtcPal mock library" FORCE)
  set(RDM_BUILD_MOCK_LIB ON CACHE BOOL "Build the RDM mock library" FORCE)
endif()

if(COMPILING_AS_OSS)
  get_cpm()

  add_oss_dependency(EtcPal GIT_REPOSITORY https://github.com/ETCLabs/EtcPal.git)
  add_oss_dependency(RDM GIT_REPOSITORY https://github.com/ETCLabs/RDM.git)

  if(RDMNET_BUILD_TESTS)
    add_oss_dependency(googletest GIT_REPOSITORY https://github.com/google/googletest.git)
  endif()
else()
  include(${CMAKE_TOOLS_MODULES}/DependencyManagement.cmake)
  add_project_dependencies()

  if(RDMNET_BUILD_TESTS)
    add_project_dependency(googletest)
  endif()

  if(RDMNET_INCLUDE_SIGN_TOOLS)
    add_project_dependency(ETC_Sign)
    add_project_dependency(ETC_Mac_Notarize)
  endif()
endif()

if(TARGET EtcPalMock)
  set_target_properties(EtcPalMock PROPERTIES FOLDER dependencies)
endif()
if(TARGET RDMMock)
  set_target_properties(RDMMock PROPERTIES FOLDER dependencies)
endif()
