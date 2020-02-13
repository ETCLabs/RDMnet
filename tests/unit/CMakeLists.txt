################################### testing ###################################

set(RDMNET_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/../../include)
set(RDMNET_SRC ${CMAKE_CURRENT_LIST_DIR}/../../src)

include(GoogleTest)

include_directories(test_shared)
add_compile_definitions(RDMNET_HAVE_CONFIG_H)

function(rdmnet_add_unit_test test_name)
  add_executable(${test_name} ${ARGN})
  target_include_directories(${test_name} PRIVATE ${RDMNET_INCLUDE} ${RDMNET_SRC})
  target_link_libraries(${test_name} PRIVATE gmock meekrosoft::fff)
  set_target_properties(${test_name} PROPERTIES CXX_STANDARD 17 FOLDER tests)
  gtest_discover_tests(${test_name})
endfunction()

add_subdirectory(core)
add_subdirectory(api)
add_subdirectory(broker)
add_subdirectory(discovery)
