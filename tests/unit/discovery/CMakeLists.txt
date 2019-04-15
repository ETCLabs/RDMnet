# Test the discovery implementations.

if(MSVC AND CMAKE_SIZEOF_VOID_P STREQUAL 8)
  add_subdirectory(bonjour)
endif()