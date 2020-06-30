################################### testing ###################################

include(GoogleTest)

set(UNIT_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})

option(RDMNET_BUILD_TESTS_STATIC "Build RDMnet's unit tests using static memory" OFF)

function(rdmnet_add_unit_test test_name)
  add_executable(${test_name} ${ARGN})
  target_include_directories(${test_name} PRIVATE
    ${RDMNET_INCLUDE}
    ${RDMNET_SRC}
    ${UNIT_TEST_DIR}/shared
  )
  if(RDMNET_BUILD_TESTS_STATIC)
    target_include_directories(${test_name} PRIVATE ${UNIT_TEST_DIR}/shared/configs/static)
  else()
    target_include_directories(${test_name} PRIVATE ${UNIT_TEST_DIR}/shared/configs/dynamic)
  endif()
  target_compile_definitions(${test_name} PRIVATE RDMNET_HAVE_CONFIG_H)
  target_link_libraries(${test_name} PRIVATE gmock meekrosoft::fff)
  set_target_properties(${test_name} PROPERTIES CXX_STANDARD 17 FOLDER tests)
  gtest_discover_tests(${test_name} NO_PRETTY_VALUES)
endfunction()

add_subdirectory(api)
add_subdirectory(core)
add_subdirectory(discovery)
if(NOT RDMNET_BUILD_TESTS_STATIC)
  add_subdirectory(broker)
endif()
