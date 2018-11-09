################################ RDMnet Broker ################################

add_executable(RDMnetBroker
  broker_log.h
  iflist.h
  service_shell.h

  broker_main.cpp
  broker_log.cpp
  iflist.cpp
  service_shell.cpp
  service_utils.cpp
)
target_include_directories(RDMnetBroker PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(RDMnetBroker PRIVATE Broker)
if(WIN32)
  target_compile_definitions(RDMnetBroker PRIVATE UNICODE _UNICODE)
endif()