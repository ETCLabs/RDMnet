
if(NOT DEFINED QTDIR)
  set(QTDIR $ENV{QTDIR})
endif()
set(CMAKE_PREFIX_PATH ${QTDIR})

find_package(Qt5Widgets 5.9.1 CONFIG)

if(NOT Qt5Widgets_FOUND)
  message(FATAL_ERROR
    "Qt was not found. Supply the Qt installation directory in one of the following ways:\n"
    "- As a command-line argument, e.g. cmake -DQTDIR=C:\\Qt\\5.9.1\\msvc2015\n"
    "- As an environment variable named QTDIR"
  )
endif()

################################# CMake <-> Qt ################################

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
# Create code from a list of Qt designer ui files
set(CMAKE_AUTOUIC ON)

############################ RDMnet Controller GUI ############################ 

add_executable(RDMnetControllerGUI
  AboutGUI.cpp
  AboutGUI.h
  AboutGUI.ui
  BrokerItem.cpp
  BrokerItem.h
  BrokerStaticAddGUI.cpp
  BrokerStaticAddGUI.h
  BrokerStaticAddGUI.ui
  EndpointItem.cpp
  EndpointItem.h
  LogWindowGUI.cpp
  LogWindowGUI.h
  LogWindowGUI.ui
  main.cpp
  NetworkDetailsProxyModel.cpp
  NetworkDetailsProxyModel.h
  PropertyEditorsDelegate.cpp
  PropertyEditorsDelegate.h
  PropertyItem.cpp
  PropertyItem.h
  PropertyPushButton.cpp
  PropertyPushButton.h
  PropertyValueItem.cpp
  PropertyValueItem.h
  RDMnetClientItem.cpp
  RDMnetClientItem.h
  RDMnetControllerGUI.cpp
  RDMnetControllerGUI.h
  RDMnetControllerGUI.ui
  RDMnetNetworkItem.cpp
  RDMnetNetworkItem.h
  RDMnetNetworkModel.cpp
  RDMnetNetworkModel.h
  ResponderItem.cpp
  ResponderItem.h
  SearchingStatusItem.cpp
  SearchingStatusItem.h
  SimpleNetworkProxyModel.cpp
  SimpleNetworkProxyModel.h
)
target_link_libraries(RDMnetControllerGUI PRIVATE Qt5::Widgets RDMnet)