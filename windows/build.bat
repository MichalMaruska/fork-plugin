::
::  msbuild . /p:Platform=x64

ECHO "Build the Driver & CLI configuration tool"

msbuild .\sys  /p:Platform=x64   /p:Configuration="Debug"
msbuild .\exe /p:Platform=x64 /p:Configuration="Release"

::
ECHO "Copy to my SMB share"


$SERVER="192.168.1.3"
:: /y to not ask to overwrite
:: /b binary
copy /b /y sys\x64\Debug\kbfiltr\* \\$SERVER\Public\

:: copy --update
copy /b /y exe\x64\Release\kbftest.exe \\$SERVER\Public\


:: todo use vcxproj command to copy?
