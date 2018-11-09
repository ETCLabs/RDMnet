################################ LLRP Manager #################################

set(MANAGER_TARGET LLRPExampleManager)

add_executable(${MANAGER_TARGET}
  manager.cpp
)
target_include_directories(${MANAGER_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${MANAGER_TARGET} PRIVATE RDMnet)
if(WIN32)
  target_compile_definitions(${MANAGER_TARGET} PRIVATE UNICODE _UNICODE)
  target_link_libraries(${MANAGER_TARGET} PRIVATE rpcrt4)
endif()