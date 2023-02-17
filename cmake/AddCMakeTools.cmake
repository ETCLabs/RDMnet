# Add CMake Tools
include(FetchContent)

set(CMAKE_TOOLS_GIT_TAG d34e598c3e54f7ed89ab8cd6c281ffeb5a6e635a)
if(DEFINED ENV{CI})
  if(DEFINED ENV{JENKINS_CMAKE_TOOLS_TOKEN})
    set(CMAKE_TOOLS_GIT_REPO https://$ENV{JENKINS_CMAKE_TOOLS_TOKEN}@gitlab.etcconnect.com/etc/common-tech/tools/cmake-tools.git)
  elseif(DEFINED ENV{CI_JOB_TOKEN})
    set(CMAKE_TOOLS_GIT_REPO https://gitlab-ci-token:$ENV{CI_JOB_TOKEN}@gitlab.etcconnect.com/etc/common-tech/tools/cmake-tools.git)
  else()
    message(FATAL_ERROR "Could not determine the access token to use for fetching CMake Tools.")
  endif()
else()
  # If your project does not use GitLab CI or Jenkins, you can remove this if/else and simply use this URL
  set(CMAKE_TOOLS_GIT_REPO git@gitlab.etcconnect.com:etc/common-tech/tools/cmake-tools.git)
endif()

FetchContent_Declare(
  cmake_tools
  GIT_REPOSITORY ${CMAKE_TOOLS_GIT_REPO}
  GIT_TAG ${CMAKE_TOOLS_GIT_TAG}
)
FetchContent_MakeAvailable(cmake_tools)
set(CMAKE_TOOLS_MODULES ${cmake_tools_SOURCE_DIR}/modules)
