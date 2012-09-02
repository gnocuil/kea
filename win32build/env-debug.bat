@rem Setting environment for BIND10 - Debug

@echo off

setlocal
c:\cygwin\bin\cygpath -w %BIND10HOME% | set /p b10home=
c:\cygwin\bin\cygpath -w %BIND10HOME%/.. | set /p b10parent=
endlocal

set B10_FROM_BUILD=%BIND10HOME%
set VSVER=VS2010
set RTVER=v100
set PATH=%PATH%;%b10parent%\gtest\%RTVER%\Debug
set PATH=%PATH%;%b10parent%\botan\%RTVER%\Debug
set PATH=%PATH%;%b10parent%\log4cplus\%RTVER%\Debug
set PATH=%PATH%;%b10home%\win32build\%VSVER%\Debug
set PYTHONPATH=%PYTHONPATH%;%b10home%\win32build\%VSVER%\Debug
set PYTHONPATH=%PYTHONPATH%;%b10home%\src\lib\python\isc\util
set PYTHON=C:\Python32\python_d.exe
set BIND10_PATH=%BIND10HOME%/src/bin/bind10
