# Test the discovery implementations.

add_subdirectory(common)
if(RDMNET_FORCE_LIGHTWEIGHT_DNS_QUERIER)
  add_subdirectory(lightweight)
elseif(MSVC AND CMAKE_SIZEOF_VOID_P STREQUAL 8 AND NOT RDMNET_BUILD_TESTS_STATIC)
  add_subdirectory(bonjour)
elseif(UNIX AND NOT APPLE)
  add_subdirectory(avahi)
endif()
