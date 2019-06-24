
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

set(CONTROLLER_TARGET rdmnet_controller_example)

# Note - "WIN32" argument signals to Windows that this app is linked as a
# windowed (not console) executable, and has no effect on other platforms. This
# is a cross-platform Qt app.
add_executable(${CONTROLLER_TARGET} WIN32
  src/AboutGUI.cpp
  src/AboutGUI.h
  src/BrokerItem.cpp
  src/BrokerItem.h
  src/BrokerStaticAddGUI.cpp
  src/BrokerStaticAddGUI.h
  src/ControllerDefaultResponder.h
  src/ControllerDefaultResponder.cpp
  src/ControllerLog.h
  src/ControllerLog.cpp
  src/ControllerUtils.h
  src/EndpointItem.cpp
  src/EndpointItem.h
  src/LogWindowGUI.cpp
  src/LogWindowGUI.h
  src/main.cpp
  src/NetworkDetailsProxyModel.cpp
  src/NetworkDetailsProxyModel.h
  src/PropertyEditorsDelegate.cpp
  src/PropertyEditorsDelegate.h
  src/PropertyItem.cpp
  src/PropertyItem.h
  src/PropertyPushButton.cpp
  src/PropertyPushButton.h
  src/PropertyValueItem.cpp
  src/PropertyValueItem.h
  src/RDMnetClientItem.cpp
  src/RDMnetClientItem.h
  src/RDMnetControllerGUI.cpp
  src/RDMnetControllerGUI.h
  src/RDMnetLibInterface.h
  src/RDMnetLibWrapper.h
  src/RDMnetLibWrapper.cpp
  src/RDMnetNetworkItem.cpp
  src/RDMnetNetworkItem.h
  src/RDMnetNetworkModel.cpp
  src/RDMnetNetworkModel.h
  src/ResponderItem.cpp
  src/ResponderItem.h
  src/SearchingStatusItem.cpp
  src/SearchingStatusItem.h
  src/SimpleNetworkProxyModel.cpp
  src/SimpleNetworkProxyModel.h

  ui/AboutGUI.ui
  ui/BrokerStaticAddGUI.ui
  ui/LogWindowGUI.ui
  ui/RDMnetControllerGUI.ui

  res/RDMnetControllerGUI.qrc
)

set_target_properties(${CONTROLLER_TARGET} PROPERTIES AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_LIST_DIR}/ui)
target_link_libraries(${CONTROLLER_TARGET} PRIVATE Qt5::Widgets RDMnet)
if(WIN32)
  target_compile_definitions(${CONTROLLER_TARGET} PRIVATE UNICODE _UNICODE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(${CONTROLLER_TARGET} PRIVATE rpcrt4)
  if(DEFINED DNS_SD_DLL)
    add_custom_command(
      TARGET ${CONTROLLER_TARGET}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy ${DNS_SD_DLL} $<TARGET_FILE_DIR:${CONTROLLER_TARGET}>
      COMMENT "Copying DNS-SD dll to executable directory..."
    )
  endif()
endif()

install(TARGETS ${CONTROLLER_TARGET}
  RUNTIME DESTINATION controller
)
if(WIN32)
  get_filename_component(QTDIR_ESCAPED ${QTDIR} ABSOLUTE)
  install(CODE
    "execute_process(COMMAND ${QTDIR_ESCAPED}/bin/windeployqt.exe --no-compiler-runtime ${CONTROLLER_TARGET}.exe WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/controller)"
  )
endif()

############################## Allow Qt Debugging #############################

# Help out the Visual Studio debugging environment by pointing it at the Qt DLLs.
if(MSVC)
  configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/../../tools/cmake/ExampleController.vcxproj.user.in
    ${CMAKE_CURRENT_BINARY_DIR}/${CONTROLLER_TARGET}.vcxproj.user
    @ONLY
  )
endif()
