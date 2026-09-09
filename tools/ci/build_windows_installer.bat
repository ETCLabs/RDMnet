echo OFF

REM Build the RDMnet Windows installer

REM Arguments: none
REM
REM Directory structure
REM   rdmnet
REM     build
REM       install_x86
REM         bin
REM       install_x64
REM         bin
REM     tools
REM       ci
REM       install
REM         windows
REM Requires:
REM - python
REM - batch file called from rdmnet\tools\ci directory
REM - %ProgramFiles(x86)%\NSIS\makensis.exe
REM - environment variable ARTIFACT_TYPE = "x86" or "x64"
REM - environment variable CMAKE_INSTALL = "install_x86" or "install_x64"
REM - rdmnet\build\CMakeCache.txt
REM - rdmnet\cmake\dnssd\windows.cmake
REM - rdmnet\tools\ci\copy_vc_redist_to_install.py
REM - rdmnet\tools\install\windows\uninstall.nsi
REM - rdmnet\tools\install\windows\install.nsi
REM
REM Output:
REM - signed RDMnetSetup_%ARTIFACT_TYPE%.exe
REM
REM Assumptions:
REM - this batch file is called from the rdmnet\tools\ci directory
REM - previous pipeline stage has created (required by install.nsi)
REM     C:\git\rdmnet\build\%CMAKE_INSTALL%\bin\...
REM     C:\git\rdmnet\build\%CMAKE_INSTALL%\include\...
REM     C:\git\rdmnet\build\%CMAKE_INSTALL%\lib\...
REM
REM Example:
REM   C:\git>cd rdmnet\tools\ci
REM   C:\git\rdmnet\tools\ci>build_windows_installer.bat

ECHO cd ..\..
cd ..\..

REM we are now in the rdmnet directory

REM run RDMnet script to download mdnswindows_install\ETC_mDNSInstall.exe
ECHO cmake -P cmake\dnssd\windows.cmake
cmake -P cmake\dnssd\windows.cmake
REM do NOT check error level!

REM rdmnet\mdnswindows_install\ETC_mDNSInstall.exe now exists

REM for some reason the bin\styles directory isn't under install_x86 / install_x64
ECHO mkdir build\%CMAKE_INSTALL%\bin\styles
mkdir build\%CMAKE_INSTALL%\bin\styles
ECHO copy build\examples\controller\Release\styles\*.* build\%CMAKE_INSTALL%\bin\styles
copy build\examples\controller\Release\styles\*.* build\%CMAKE_INSTALL%\bin\styles

ECHO cd tools\ci
cd tools\ci

REM run copy_vc_redist_to_install.py
ECHO python copy_vc_redist_to_install.py ..\..\build\CMakeCache.txt ..\..\tools\install\windows
python copy_vc_redist_to_install.py ..\..\build\CMakeCache.txt ..\..\tools\install\windows

REM vc_redist.x86.exe or vc_redist.x64.exe now exists in tools\install\windows

ECHO cd ..\..
cd ..\..

ECHO Initializing signing tool
set ETC_SIGN_CREDS=%NET_CODESIGN_USER%:%NET_CODESIGN_PASS%
ECHO call build\_deps\etc_sign-src\etc_sign.bat --initcert
call build\_deps\etc_sign-src\etc_sign.bat --initcert

ECHO Signing ETC_mDNSInstall.exe
ECHO call build\_deps\etc_sign-src\etc_sign.bat mdnswindows_install\ETC_mDNSInstall.exe
call build\_deps\etc_sign-src\etc_sign.bat mdnswindows_install\ETC_mDNSInstall.exe

ECHO cd tools\install\windows
cd tools\install\windows

ECHO Build the RDMnet installer

REM During "normal" use NSIS 3.10 does not allow the uninstaller to be signed. It creates the uninstaller
REM in a temporary directory, adds it to the installer, and delete the uninstaller. The next few steps involve
REM two separate NSIS script. The first script generates the uninstaller, wrapped in a "fake" installer. The
REM fake installer only contains the uninstaller, and installs it in the same directory as the installer.
REM
REM The fake installer is run, and then deleted. The uninstaller is then signed and is available for the
REM second NSIS script which generates the "real" RDMnet installer.

REM create a fake RDMnet installer fake_install.exe
ECHO "%ProgramFiles(x86)%\NSIS\makensis.exe" uninstall.nsi
"%ProgramFiles(x86)%\NSIS\makensis.exe" uninstall.nsi
if %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )

REM wait a few seconds for the fake installer to become available
ECHO ping 127.0.0.1 -n 6 >nul
ping 127.0.0.1 -n 6 >nul

REM run the fake installer in silent mode to "install" the uninstaller
ECHO call fake_install.exe /S
call fake_install.exe /S

REM the fake installer does not get checked in to Git
ECHO del fake_install.exe
del fake_install.exe

REM wait a few seconds for the uninstaller to become available
ECHO ping 127.0.0.1 -n 6 >nul
ping 127.0.0.1 -n 6 >nul

if %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )

ECHO Signing RDMnet uninstaller
REM The unstaller file name does NOT include "_x86" / "_x64"
ECHO call ..\..\..\build\_deps\etc_sign-src\etc_sign.bat RDMnet_Uninstall.exe
call ..\..\..\build\_deps\etc_sign-src\etc_sign.bat RDMnet_Uninstall.exe
REM if %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )

REM copy mDNSWindows installer
ECHO copy ..\..\..\build\mdnswindows_install\ETC_mDNSInstall.exe .
copy ..\..\..\build\mdnswindows_install\ETC_mDNSInstall.exe .

REM create the installer
ECHO "%ProgramFiles(x86)%\NSIS\makensis.exe" install.nsi
"%ProgramFiles(x86)%\NSIS\makensis.exe" install.nsi
if %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )

ECHO Signing RDMnet installer
ECHO call ..\..\..\build\_deps\etc_sign-src\etc_sign.bat RDMnetSetup_%ARTIFACT_TYPE%.exe
call ..\..\..\build\_deps\etc_sign-src\etc_sign.bat RDMnetSetup_%ARTIFACT_TYPE%.exe
REM if %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )

REM delete the uninstaller
ECHO del RDMnet_Uninstall.exe
del RDMnet_Uninstall.exe

REM display files
ECHO dir
dir

REM copy installer to root directory
ECHO copy RDMnetSetup_%ARTIFACT_TYPE%.exe ..\..\..
copy RDMnetSetup_%ARTIFACT_TYPE%.exe ..\..\..

REM verify copy
ECHO cd ..\..\..
cd ..\..\..
ECHO dir
dir

REM return to the tools\ci directory where we started
ECHO cd tools\ci
cd tools\ci

