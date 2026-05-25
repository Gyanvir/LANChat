@echo off
echo ===================================================
echo Building LANChat using VS 2019 and Qt 5.15.2 (x64)
echo ===================================================

:: Ensure we are in the script's directory
cd /d "%~dp0"

:: Initialize MSVC 2019 Environment
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Prepend Qt binaries to PATH
set "PATH=C:\Users\Dell\miniconda3\Library\bin;%PATH%"

:: Check if qmake is available
qmake -v >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: qmake is not found in PATH or environment setup failed.
    exit /b 1
)

:: Run qmake
echo [1/2] Running qmake...
qmake -makefile -spec win32-msvc LANChat.pro
if %ERRORLEVEL% neq 0 (
    echo QMake failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

:: Run nmake
echo [2/2] Running nmake...
nmake
if %ERRORLEVEL% neq 0 (
    echo NMake build failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo ===================================================
echo Build Succeeded!
echo Executable is located in:
echo   - release\LANChat.exe (Release build)
echo   - or debug\LANChat.exe (Debug build)
echo ===================================================
