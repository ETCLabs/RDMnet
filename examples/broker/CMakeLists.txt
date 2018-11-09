################################ RDMnet Broker ################################

set(BROKER_TARGET RDMnetExampleBroker)

add_executable(${BROKER_TARGET}
  broker_log.h
  iflist.h
  service_shell.h

  broker_main.cpp
  broker_log.cpp
  iflist.cpp
  service_shell.cpp
  service_utils.cpp
)
target_include_directories(${BROKER_TARGET} PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${BROKER_TARGET} PRIVATE Broker)
if(WIN32)
  target_compile_definitions(${BROKER_TARGET} PRIVATE UNICODE _UNICODE)
endif()