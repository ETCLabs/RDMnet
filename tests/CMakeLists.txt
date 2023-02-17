# Main test module. 

include(${RDMNET_CMAKE}/RdmnetSourceManifest.cmake)

set(TEST_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(data)
add_subdirectory(unit)
