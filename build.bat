@echo off
setlocal enabledelayedexpansion

echo ===================================================
echo  Building City Bloxx (SDL3)
echo ===================================================

where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] CMake was not found in your PATH. Please install CMake or add it to PATH.
    exit /b 1
)

if not exist build (
    mkdir build
)

echo [1/2] Configuring CMake...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release %*
if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed.
    exit /b %errorlevel%
)

echo [2/2] Compiling...
cmake --build build --config Release -j
if %errorlevel% neq 0 (
    echo [ERROR] Build failed.
    exit /b %errorlevel%
)

echo.
echo ===================================================
echo  Build successful!
echo ===================================================
if exist build\citybloxx.exe (
    echo Executable: build\citybloxx.exe
    echo Run with:   .\build\citybloxx.exe
) else if exist build\Release\citybloxx.exe (
    echo Executable: build\Release\citybloxx.exe
    echo Run with:   .\build\Release\citybloxx.exe
) else (
    echo Run the generated citybloxx executable inside build\
)
echo.

endlocal
exit /b 0
