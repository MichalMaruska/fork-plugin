::
::  msbuild . /p:Platform=x64

ECHO "Build the Driver & CLI configuration tool"

msbuild .\sys  /p:Platform=x64   /p:Configuration="Debug"  && (
msbuild .\exe /p:Platform=x64 /p:Configuration="Release"
)

IF ERRORLEVEL 1 GOTO FAILED


ECHO "verify the .inf file"
"%WindowsSdkDir%\tools\%WindowsSdkVersion%\x64\infverif.exe" /v /h sys/x64/Release/kbfilter.inf

IF ERRORLEVEL 1 GOTO FAILED


::
ECHO "Copy to my SMB share"


set SERVER="192.168.1.3"
:: /y to not ask to overwrite
:: /b binary
copy /b /y sys\x64\Debug\kbfiltr\* \\%SERVER%\Public\

:: copy --update
copy /b /y exe\x64\Release\kbftest.exe \\%SERVER%\Public\
EXIT /b 0

:: todo use vcxproj command to copy?

:FAILED
ECHO "*** Build failed! ***"
EXIT /b 1
