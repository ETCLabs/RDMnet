# Generate the test file manifest
file(GLOB TEST_FILE_LIST messages/*.data.txt)
source_group(TREE ${CMAKE_CURRENT_LIST_DIR}/messages PREFIX validation_files FILES ${TEST_FILE_LIST})

foreach(FILE_NAME ${TEST_FILE_LIST})
  get_filename_component(FILE_BASE_NAME ${FILE_NAME} NAME)
  string(REPLACE ".data.txt" "" FILE_BASE_NAME ${FILE_BASE_NAME})

  set(RDMNET_VALIDATION_MESSAGE_DECLARATIONS "${RDMNET_VALIDATION_MESSAGE_DECLARATIONS}extern \"C\" const RdmnetMessage ${FILE_BASE_NAME};\n")
  set(RDMNET_TEST_DATA_PAIRS "${RDMNET_TEST_DATA_PAIRS}  {\"${FILE_NAME}\", ${FILE_BASE_NAME}},\n")
endforeach()

configure_file(test_file_manifest.cpp.in ${CMAKE_CURRENT_LIST_DIR}/GeneratedFiles/test_file_manifest.cpp)

# Add the test_data library
add_library(test_data
  ${CMAKE_CURRENT_LIST_DIR}/load_test_data.h
  ${CMAKE_CURRENT_LIST_DIR}/load_test_data.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_data_util.h
  ${CMAKE_CURRENT_LIST_DIR}/test_file_manifest.h
  ${CMAKE_CURRENT_LIST_DIR}/GeneratedFiles/test_file_manifest.cpp
)
get_target_property(ETCPAL_INCLUDE EtcPal INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(RDM_INCLUDE RDM INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(test_data 
  PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${RDMNET_INCLUDE} ${RDMNET_SRC}
  PRIVATE ${RDM_INCLUDE} ${ETCPAL_INCLUDE}
)
target_link_libraries(test_data PRIVATE gtest)
set_target_properties(test_data PROPERTIES FOLDER tests CXX_STANDARD 14)

# Add the validation files to the library to be compiled
foreach(FILE_NAME ${TEST_FILE_LIST})
  get_filename_component(FILE_BASE_NAME ${FILE_NAME} NAME)
  string(REPLACE ".data.txt" "" FILE_BASE_NAME ${FILE_BASE_NAME})
  target_sources(test_data PRIVATE messages/${FILE_BASE_NAME}.validation.c)
endforeach()
