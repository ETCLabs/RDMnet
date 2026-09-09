# uninstall.nsi
#
# Do not edit uninstall.nsi, edit uninstall.nsi.in
#
# Create the Windows uninstaller for RDMnet
#
# Usage: "C:\Program Files (x86)\NSIS\makensis.exe" uninstall.nsi

!include "WinVer.nsh"
!include "WordFunc.nsh"
!include "Registry.nsh"

# ; MUI 1.67 compatible ------
!include "MUI.nsh"

# ; MUI Settings
!define MUI_ABORTWARNING
!define MUI_UNICON "ETCIconDark_NSIS.ico"
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
!insertmacro MUI_LANGUAGE "English"
# MUI end ------

!if "$%ARTIFACT_TYPE%" == "${U+24}%ARTIFACT_TYPE%"
  !error "Error environment variable ARTIFACT_TYPE not defined"
!endif

!define PRODUCT_NAME "RDMnet"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\{9F1AE589-20F1-49BD-A8D7-83DCD5C01CC1}"
!define PRODUCT_VERSION "1.0.0.11"

# the name of the uninstaller
Outfile "fake_install.exe"

# Request application privileges for Windows Vista
RequestExecutionLevel admin

# define the friendly name displayed in the installer pages
Name "ETC ${PRODUCT_NAME} ${PRODUCT_VERSION}"

# Show install details
ShowInstDetails show

# default section
Section Uninstall
  # this will affect all users
  SetShellVarContext all

  !if "$%ARTIFACT_TYPE%" == "x64"  
    SetRegView 64
  !endif

  # delete keys used by Control Panel "Uninstall" screen
  DeleteRegKey HKLM "${UNINSTALL_KEY}"

  RMDir /r "$PROGRAMFILES64\ETC\RDMnet"

  # finally, delete the uninstaller itself
  Delete "$PROGRAMFILES64\ETC\RDMnet\RDMnet_Uninstall.exe"

  SetAutoClose true
SectionEnd

Section "Application"
  # only "install" the uninstaller
  SetOutPath "$PROGRAMFILES64\ETC\RDMnet"
  WriteUninstaller "$EXEDIR\RDMnet_Uninstall.exe"
SectionEnd
