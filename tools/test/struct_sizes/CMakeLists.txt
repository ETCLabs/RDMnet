# struct_sizes, a tool which prints the byte size of various structs used in the RDMnet library
# This is mostly for optimizing around stack usage in embedded applications.

add_executable(struct_sizes struct_sizes.cpp)
# To see the private headers
target_include_directories(struct_sizes PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../../../src)
target_link_libraries(struct_sizes PRIVATE RDMnet)
