# Main test module. 

enable_testing()

message(STATUS "Adding GoogleTest dependency...")
include(AddGoogleTest)

add_subdirectory(unit)