call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" %VCVARSALL_PLATFORM%

MSBuild.exe -property:Configuration=Release -property:Platform=%ARTIFACT_TYPE% .\tools\install\windows\RDMnetInstall_%ARTIFACT_TYPE%.wixproj
IF %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )

copy tools\install\windows\bin\Release\RDMnetSetup_%ARTIFACT_TYPE%.msi .\RDMnetSetup_%ARTIFACT_TYPE%.msi
IF %ERRORLEVEL% NEQ 0 ( EXIT /B %ERRORLEVEL% )
