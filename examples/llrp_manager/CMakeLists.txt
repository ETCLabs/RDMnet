################################ LLRP Manager #################################

set(MANAGER_TARGET llrp_manager_example)

add_executable(${MANAGER_TARGET}
  src/manager.cpp
)

target_include_directories(${MANAGER_TARGET} PRIVATE src)
target_link_libraries(${MANAGER_TARGET} PRIVATE RDMnet)
set_target_properties(${MANAGER_TARGET} PROPERTIES CXX_STANDARD 14 FOLDER examples)
if(WIN32)
  target_sources(${MANAGER_TARGET} PRIVATE src/windows/main.cpp)
  target_compile_definitions(${MANAGER_TARGET} PRIVATE UNICODE _UNICODE)
  target_link_libraries(${MANAGER_TARGET} PRIVATE rpcrt4)
else()
  target_sources(${MANAGER_TARGET} PRIVATE src/unix/main.cpp)
endif()

install(TARGETS ${MANAGER_TARGET}
  RUNTIME DESTINATION bin
)
