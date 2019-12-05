# Test the discovery implementations.

add_subdirectory(common)
if(RDMNET_FORCE_LIGHTWEIGHT_DNS_QUERIER)
  add_subdirectory(lightweight)
elseif(MSVC AND CMAKE_SIZEOF_VOID_P STREQUAL 8)
  add_subdirectory(bonjour)
endif()
