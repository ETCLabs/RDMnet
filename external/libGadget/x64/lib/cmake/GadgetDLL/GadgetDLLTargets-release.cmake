#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "GadgetDLL::GadgetDLL" for configuration "Release"
set_property(TARGET GadgetDLL::GadgetDLL APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(GadgetDLL::GadgetDLL PROPERTIES
  IMPORTED_IMPLIB_RELEASE "${_IMPORT_PREFIX}/lib/GadgetDLL.lib"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/bin/GadgetDLL.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS GadgetDLL::GadgetDLL )
list(APPEND _IMPORT_CHECK_FILES_FOR_GadgetDLL::GadgetDLL "${_IMPORT_PREFIX}/lib/GadgetDLL.lib" "${_IMPORT_PREFIX}/bin/GadgetDLL.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
