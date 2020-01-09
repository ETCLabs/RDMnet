add_library(test_data
  ${CMAKE_CURRENT_LIST_DIR}/load_test_data.h
  ${CMAKE_CURRENT_LIST_DIR}/load_test_data.cpp
)
get_target_property(RDMNET_INCLUDE_DIRS RDMnet INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(RDM_INCLUDE_DIRS RDM INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(test_data PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${RDMNET_INCLUDE_DIRS} ${RDM_INCLUDE_DIRS})

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
