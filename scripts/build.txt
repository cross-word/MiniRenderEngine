@echo off
setlocal
:: HowToUse: scripts\build.bat [Debug|Release]
set CFG=%1
if "%CFG%"=="" set CFG=Debug

set GEN="Visual Studio 17 2022"
set ARCH=x64

if defined VCPKG_ROOT (
  set TOOLCHAIN=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
) else (
  if not exist ".vcpkg\vcpkg.exe" (
    echo [vcpkg] cloning...
    git clone --depth=1 https://github.com/microsoft/vcpkg .vcpkg || exit /b 1
    call .vcpkg\bootstrap-vcpkg.bat || exit /b 1
  )
  set TOOLCHAIN=%CD%\.vcpkg\scripts\buildsystems\vcpkg.cmake
)

if exist "vcpkg_cache\" (
  set VCPKG_BINARY_SOURCES=clear;files,%CD%\vcpkg_cache,readwrite
)

cmake -S . -B build -G %GEN% -A %ARCH% ^
  -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN%" ^
  -DVCPKG_FEATURE_FLAGS=manifests,versions || exit /b 1

cmake --build build --config %CFG% || exit /b 1

echo.
echo Output: %CD%\build\bin\%CFG%
endlocal