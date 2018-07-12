@echo off

SET msbuild="C:\Program Files (x86)\MSBuild\14.0\Bin\msbuild.exe"
SET option=%1

REM APPS
SET broker_proj="%~dp0\apps\windows\broker\msvc2015\broker.vcxproj"
SET broker_bin_32="%~dp0\apps\windows\broker\msvc2015\Release\broker.exe"
SET broker_bin_64="%~dp0\apps\windows\broker\msvc2015\x64\Release\broker.exe"
SET controller_proj="%~dp0\apps\windows\controller\msvc2015\RDMnetControllerGUI.vcxproj"
SET controller_bin_32="%~dp0\apps\windows\controller\msvc2015\Win32\Release\RDMnetControllerGUI.exe"
SET device_proj="%~dp0\apps\windows\device\msvc2015\device.vcxproj"
SET device_bin_32="%~dp0\apps\windows\device\msvc2015\Release\device.exe"
SET device_bin_64="%~dp0\apps\windows\device\msvc2015\x64\Release\device.exe"
SET manager_proj="%~dp0\apps\windows\llrp_manager\msvc2015\manager.vcxproj"
SET manager_bin_32="%~dp0\apps\windows\llrp_manager\msvc2015\Release\manager.exe"
SET manager_bin_64="%~dp0\apps\windows\llrp_manager\msvc2015\x64\Release\manager.exe"

SET deploy_loc_32="%~dp0\bin\Win32"
SET deploy_loc_64="%~dp0\bin\x64"

IF NOT "%option%"=="-r" (
  IF NOT "%option%"=="" (
    echo "Usage: %0 [-r]"
    EXIT /B
  )
)
IF "%option%"=="-r" SET rebuild=1

IF NOT EXIST %deploy_loc_32% (
  mkdir %deploy_loc_32%
)
IF NOT EXIST %deploy_loc_32%\Controller (
  mkdir %deploy_loc_32%\Controller
)
IF NOT EXIST %deploy_loc_64% (
  mkdir %deploy_loc_64%
)

IF DEFINED rebuild (
  %msbuild% /p:Configuration=Release;Platform=Win32 /t:Rebuild %broker_proj%
  %msbuild% /p:Configuration=Release;Platform=x64 /t:Rebuild %broker_proj%
  %msbuild% /p:Configuration=Release;Platform=Win32 /t:Rebuild %controller_proj%
  %msbuild% /p:Configuration=Release;Platform=Win32 /t:Rebuild %device_proj%
  %msbuild% /p:Configuration=Release;Platform=x64 /t:Rebuild %device_proj%
  %msbuild% /p:Configuration=Release;Platform=Win32 /t:Rebuild %manager_proj%
  %msbuild% /p:Configuration=Release;Platform=x64 /t:Rebuild %manager_proj%
) ELSE (
  %msbuild% /p:Configuration=Release;Platform=Win32 %broker_proj%
  %msbuild% /p:Configuration=Release;Platform=x64 %broker_proj%
  %msbuild% /p:Configuration=Release;Platform=Win32 %controller_proj%
  %msbuild% /p:Configuration=Release;Platform=Win32 %device_proj%
  %msbuild% /p:Configuration=Release;Platform=x64 %device_proj%
  %msbuild% /p:Configuration=Release;Platform=Win32 %manager_proj%
  %msbuild% /p:Configuration=Release;Platform=x64 %manager_proj%
)

IF %errorlevel%==0 (
  copy %broker_bin_32% %deploy_loc_32%
  copy %broker_bin_64% %deploy_loc_64%
  copy %controller_bin_32% %deploy_loc_32%\Controller
  %QTDIR%\bin\windeployqt %deploy_loc_32%\Controller\RDMnetControllerGUI.exe
  copy %device_bin_32% %deploy_loc_32%
  copy %device_bin_64% %deploy_loc_64%
  copy %manager_bin_32% %deploy_loc_32%
  copy %manager_bin_64% %deploy_loc_64%
)

