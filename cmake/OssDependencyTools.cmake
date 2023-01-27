# Dependency management functions for ETC open-source software CMake projects

# Get CPM
# From https://github.com/cpm-cmake/CPM.cmake/blob/939123d1b42014a65a807450db77b0e623c9a312/cmake/get_cpm.cmake
# Then macro-ized
macro(get_cpm)
  set(CPM_DOWNLOAD_VERSION 0.34.3)

  if(CPM_SOURCE_CACHE)
    # Expand relative path. This is important if the provided path contains a tilde (~)
    get_filename_component(CPM_SOURCE_CACHE ${CPM_SOURCE_CACHE} ABSOLUTE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
  elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
  else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
  endif()

  if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
    )
  endif()

  include(${CPM_DOWNLOAD_LOCATION})
endmacro()

# Determine whether the encompassing ETC project is compiling as internal or open-source.
# Sets COMPILING_AS_OSS to on or off
function(determine_compile_environment)
  # Assume OSS unless we detect the git remote successfully. This includes scenarios where git is not
  # available and/or the project was downloaded as a zip/tarball.
  set(COMPILING_AS_OSS ON)

  find_package(Git QUIET)
  if(GIT_FOUND AND EXISTS ${PROJECT_SOURCE_DIR}/.git)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} remote -v
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
      RESULT_VARIABLE git_remote_result
      OUTPUT_VARIABLE git_remote_output
    )

    if(NOT git_remote_result EQUAL 0)
      message(FATAL_ERROR "Error running git remote -v. Check your git installation.")
    endif()

    set(MATCH_REGEX "origin[ \t]+[^ \t\r\n]+gitlab\\.etcconnect\\.com")
    if(${git_remote_output} MATCHES ${MATCH_REGEX})
      set(COMPILING_AS_OSS OFF)
    endif()
  endif()

  if(COMPILING_AS_OSS)
    message(STATUS "Note: Compiling ${PROJECT_NAME} in open-source configuration")
  endif()
  set(COMPILING_AS_OSS ${COMPILING_AS_OSS} PARENT_SCOPE)
endfunction()

# Add a dependency in an open-source configuration.
#
# Syntax:
# add_oss_dependency(<name>
#                    [CPMAddPackage/FetchContent args...]
#                   )
#
# This is a wrapper around CPMAddPackage() that automatically reads the dependency's version and
# git tag from the etc_project.json file. All other CPMAddPackage() arguments are provided by the
# caller. Note that you will almost certainly need to at least pass a GIT_REPOSITORY argument.
function(add_oss_dependency dep_name)
  if(TARGET ${dep_name})
    message(STATUS "${PROJECT_NAME}: Note: Skipping dependency '${dep_name}' because it is already a CMake target.")
    return()
  endif()

  get_dependency_version(${dep_name})

  string(TOUPPER ${dep_name} dep_name_upper)
  CPMAddPackage(
    NAME ${dep_name}
    VERSION ${${dep_name_upper}_VERSION}
    GIT_TAG ${${dep_name_upper}_GIT_TAG}
    ${ARGN}
  )

  set(${dep_name}_SOURCE_DIR ${${dep_name}_SOURCE_DIR} PARENT_SCOPE)
  set(${dep_name}_BINARY_DIR ${${dep_name}_BINARY_DIR} PARENT_SCOPE)
  set(${dep_name}_ADDED ${${dep_name}_ADDED} PARENT_SCOPE)
endfunction()

# Get dependency version info for <dep_name> in the etc_project.json file.
#
# Sets <dep_name_upper>_GIT_TAG to the correct tag/reference for the dependency to pass to CPM.
#
# Sets <dep_name_upper>_VERSION to the correct version for the dependency to pass to CPM (this will
# be defined to '0' if there's no applicable version number)
#
# Fails if neither gitTag nor version is present in the dependency's JSON object.
function(get_dependency_version dep_name)
  file(READ ${PROJECT_SOURCE_DIR}/etc_project.json project_json_content)

  string(TOUPPER ${dep_name} dep_name_upper)

  if(${CMAKE_VERSION} VERSION_GREATER_EQUAL 3.19)
    get_dependency_version_info_json(${project_json_content} ${dep_name})
  else()
    # Use regex heuristics to get the version
    get_dependency_version_info_regex(${project_json_content} ${dep_name})
  endif()

  if(NOT dep_git_tag AND NOT dep_version)
    message(FATAL_ERROR "Neither version nor gitTag found for dependency ${dep_name}")
  endif()

  if(dep_git_tag)
    set(${dep_name_upper}_GIT_TAG ${dep_git_tag} PARENT_SCOPE)
  endif()

  if(dep_version)
    set(${dep_name_upper}_VERSION ${dep_version} PARENT_SCOPE)
    if(NOT dep_git_tag)
      set(${dep_name_upper}_GIT_TAG v${dep_version} PARENT_SCOPE)
    endif()
  else()
    set(${dep_name_upper}_VERSION 0 PARENT_SCOPE)
  endif()
endfunction()

# Helper for get_dependency_version() for CMake >= 3.19
function(get_dependency_version_info_json project_json_content dep_name)
  # Use CMake JSON functions to get the version
  string(JSON num_dependencies LENGTH ${project_json_content} dependencies)

  foreach(dep_index RANGE ${num_dependencies})
    if(${dep_index} EQUAL ${num_dependencies})
      break()
    endif()

    string(JSON current_dep_obj GET ${project_json_content} dependencies ${dep_index})
    string(JSON current_dep_name GET ${current_dep_obj} name)
    if(${current_dep_name} STREQUAL ${dep_name})
      string(JSON dep_version ERROR_VARIABLE dep_version_err GET ${current_dep_obj} version)

      if(NOT dep_version_err)
        set(dep_version ${dep_version} PARENT_SCOPE)
      endif()

      string(JSON dep_git_tag ERROR_VARIABLE dep_git_tag_err GET ${current_dep_obj} gitTag)
      if(NOT dep_git_tag_err)
        set(dep_git_tag ${dep_git_tag} PARENT_SCOPE)
      endif()

      return()
    endif()
  endforeach()
endfunction()

# Helper for get_dependency_version() for CMake < 3.19
function(get_dependency_version_info_regex project_json_content dep_name)
  set(git_tag_regex "\"dependencies\": *\\[.*{.*\"name\": *\"${dep_name}\"[^}]+\"gitTag\": *\"([^\"]+)\".*}.*\\]")
  set(version_regex "\"dependencies\": *\\[.*{.*\"name\": *\"${dep_name}\"[^}]+\"version\": *\"([^\"]+)\".*}.*\\]")

  if(${project_json_content} MATCHES ${git_tag_regex})
    set(dep_git_tag ${CMAKE_MATCH_1} PARENT_SCOPE)
  endif()

  if(${project_json_content} MATCHES ${version_regex})
    set(dep_version ${CMAKE_MATCH_1} PARENT_SCOPE)
  endif()
endfunction()
