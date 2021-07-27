# Main test module. 

include(RdmnetSourceManifest)

set(TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(data)
add_subdirectory(unit)
