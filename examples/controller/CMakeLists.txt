
if(NOT DEFINED QTDIR)
  set(QTDIR $ENV{QTDIR})
endif()
set(CMAKE_PREFIX_PATH ${QTDIR})
set(QT_REQUIRED_VERSION 5.9.7)

find_package(Qt5Widgets ${QT_REQUIRED_VERSION} CONFIG)

if(NOT Qt5Widgets_FOUND)
  message(FATAL_ERROR
    "Qt (>= ${QT_REQUIRED_VERSION}) was not found.\n"
    "Supply the Qt installation directory in one of the following ways:\n"
    "- As a command-line argument, e.g. cmake -G \"Visual Studio 15 2017 Win64\" "
    " -DQTDIR=C:\\Qt\\${QT_REQUIRED_VERSION}\\msvc2017_64\n"
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

set(CONTROLLER_TARGET RDMnetExampleController)

add_executable(${CONTROLLER_TARGET}
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
target_link_libraries(${CONTROLLER_TARGET} PRIVATE Qt5::Widgets RDMnet)
if(WIN32)
  target_compile_definitions(${CONTROLLER_TARGET} PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  target_include_directories(${CONTROLLER_TARGET} PRIVATE windows)
  target_link_libraries(${CONTROLLER_TARGET} PRIVATE rpcrt4)
endif()

############################## Allow Qt Debugging #############################

# Help out the Visual Studio debugging environment by pointing it at the Qt
# DLLs.
if(MSVC)
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/../../tools/cmake/ExampleController.vcxproj.user.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CONTROLLER_TARGET}.vcxproj.user
    @ONLY
  )
endif()