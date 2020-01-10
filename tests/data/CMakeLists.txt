add_library(test_data
  ${CMAKE_CURRENT_LIST_DIR}/load_test_data.h
  ${CMAKE_CURRENT_LIST_DIR}/load_test_data.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_data_util.h
)
target_include_directories(test_data PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(test_data PRIVATE RDMnet gtest)
set_target_properties(test_data PROPERTIES FOLDER tests)

# Generate the test file manifest
file(GLOB TEST_FILE_LIST messages/*.data.txt)
foreach(FILE_NAME ${TEST_FILE_LIST})
  get_filename_component(FILE_BASE_NAME ${FILE_NAME} NAME)
  string(REPLACE ".data.txt" "" FILE_BASE_NAME ${FILE_BASE_NAME})

  set(RDMNET_VALIDATION_MESSAGE_DECLARATIONS "${RDMNET_VALIDATION_MESSAGE_DECLARATIONS}extern \"C\" const RdmnetMessage ${FILE_BASE_NAME};\n")
  set(RDMNET_TEST_DATA_PAIRS "${RDMNET_TEST_DATA_PAIRS}  {\"${FILE_NAME}\", ${FILE_BASE_NAME}},\n")
  target_sources(test_data PRIVATE messages/${FILE_BASE_NAME}.validation.c)
endforeach()

configure_file(test_file_manifest.h.in ${CMAKE_CURRENT_LIST_DIR}/GeneratedFiles/test_file_manifest.h)
