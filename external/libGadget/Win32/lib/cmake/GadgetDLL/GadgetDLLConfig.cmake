include(FindPackageHandleStandardArgs)
set(${CMAKE_FIND_PACKAGE_NAME}_CONFIG ${CMAKE_CURRENT_LIST_FILE})
find_package_handle_standard_args(GadgetDLL CONFIG_MODE)

if(NOT TARGET GadgetDLL::GadgetDLL)
    include("${CMAKE_CURRENT_LIST_DIR}/GadgetDLLTargets.cmake")
endif()
