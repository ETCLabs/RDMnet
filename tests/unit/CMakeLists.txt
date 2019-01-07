################################### testing ###################################

# RDMnet library unit tests

add_executable(test_rdmnet_lib
  # lwpa unit test sources
  common_lib/main.cpp
  common_lib/test_broker_prot.cpp
)

target_link_libraries(test_rdmnet_lib RDMnet gtest)