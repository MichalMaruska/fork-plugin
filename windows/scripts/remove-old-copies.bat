rem https://stackoverflow.com/questions/77643057/batch-script-that-finds-oem-number-of-driver-in-windows-command-pnputil-and-dele
@echo off
rem pnputil.exe /enum-drivers /class Keyboard | findstr "maruska Published Original"

rem findstr ' ' does not work!
rem echo %string:~4,3%

rem pnputil -f -d oem41.inf
rem ('pnputil -e ^| findstr /b /c:"Published name :" /c:"Driver date and version :"')

rem !var!
SETLOCAL ENABLEDELAYEDEXPANSION

SET "MM_SIGNER=WDKTestCert Gebruiker,133751023496814809"
SET MM_DRIVER=kbfiltr.inf

rem %%b, %%c ...
rem /f passes the first blank separated token from each line of each file. Blank lines are skipped.
rem tokens=<x,y,m–n>
rem asterisk (*), an additional variable is allocated, and it receives the remaining text
rem on the line after the last token that is parsed.
rem
FOR /f "tokens=1*delims=:" %%b IN ('pnputil /enum-drivers /class Keyboard') DO (
 FOR /f "tokens=*delims= " %%e IN ("%%c") DO (

 ECHO processing ">%%b<"
 ECHO value ">%%e<"

  rem Save the oemNR.inf
  IF "%%b" == "Published Name" (
   @ECHO ===============================
   SET published=%%e
   echo saving as !published!
  )


  rem criterion 2
  IF "%%b" == "Original Name" (
     IF "%%e" == "!MM_DRIVER!" (
        set "original=%%e"
        ECHO found !published! which is !original!
        rem EXIT /b 1
        )
  )

  rem criterion 3
  IF "%%b"=="Signer Name" (
     echo looking at
     echo %%e
     echo !MM_SIGNER!
     IF "%%e" == "!MM_SIGNER!" (
        ECHO Removing !published!

        pnputil /delete-driver  !published!
        )
     )
  )
)
