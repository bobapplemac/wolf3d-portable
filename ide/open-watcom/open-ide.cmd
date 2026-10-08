@echo off
setlocal
for %%I in ("%~dp0..\..") do set "W3P_ROOT=%%~fI"

if "%WATCOM%"=="" (
    echo WATCOM is not set. Run the Open Watcom environment setup first.
    exit /b 2
)
if not exist "%W3P_ROOT%\lib\wolf3d\include\WOLF3D.h" (
    echo wolf3d-lib is missing. Run git submodule update --init --recursive.
    exit /b 2
)

set "INCLUDE=%W3P_ROOT%\platforms\dos;%W3P_ROOT%\lib\wolf3d\include;%W3P_ROOT%\lib\wolf3d\src;%W3P_ROOT%\lib\wolf3d\third_party\DBOPL;%W3P_ROOT%\lib\wolf3d\third_party\Nuked-OPL3;%INCLUDE%"
set "WCC386=-mf -w4 -dWOLF3D_STATIC -dWG_OPL_ENABLE_DBOPL=1 -dWG_OPL_ENABLE_SILENT=1 -dWG_OPL_ENABLE_ADLIB=1 -dWG_DEFAULT_OPL_DRIVER="adlib" %WCC386%"

if exist "%WATCOM%\binnt64\ide.exe" set "W3P_IDE=%WATCOM%\binnt64\ide.exe"
if not defined W3P_IDE if exist "%WATCOM%\binnt\ide.exe" set "W3P_IDE=%WATCOM%\binnt\ide.exe"
if not defined W3P_IDE if exist "%WATCOM%\binw\ide.exe" set "W3P_IDE=%WATCOM%\binw\ide.exe"
if not defined W3P_IDE (
    echo Open Watcom IDE.EXE was not found below %WATCOM%.
    exit /b 2
)

start "Open Watcom IDE - wolf3d-portable" "%W3P_IDE%" "%~dp0wolf3d-portable.wpj"
