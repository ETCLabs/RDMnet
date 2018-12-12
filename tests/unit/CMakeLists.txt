################################### testing ###################################

enable_testing()

# Get and build the Googletest library at configure time: 
include(../../tools/cmake/get_googletest.cmake)
get_googletest(${CMAKE_CURRENT_LIST_DIR}/../..)

add_executable(test_rdmnet_lib
  # lwpa unit test sources
  common_lib/main.cpp
  common_lib/test_broker_prot.cpp
)

target_link_libraries(test_rdmnet_lib RDMnet gtest)