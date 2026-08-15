@echo off
setlocal enabledelayedexpansion

REM Locate Visual Studio and toolchain
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do set "VSROOT=%%i"
) else (
    set "VSROOT=D:\Program Files\Microsoft Visual Studio\18\Community"
)
set "VCVARS=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\msys64\mingw64\bin\cmake.exe"
set "NINJA=C:\msys64\mingw64\bin\ninja.exe"

REM Dependency paths (Eigen source + FFTW source/build)
set "DEPS=C:\Users\lxj\deps"
set "EIGEN=%DEPS%\eigen-3.4.0"
set "FFTW_SRC=%DEPS%\fftw-3.3.10"
set "FFTW_BUILD=%DEPS%\fftw-build"
set "FFTW_INC=%FFTW_SRC%\api"
set "FFTW_LIB=%FFTW_BUILD%\fftw3.lib"

REM Project root and native build dir
set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build_windows"

echo [1/5] Setup MSVC environment
call "%VCVARS%" >nul || (echo vcvars64 failed & exit /b 1)

echo [2/5] Build FFTW if needed
if not exist "%FFTW_LIB%" (
    "%CMAKE%" -S "%FFTW_SRC%" -B "%FFTW_BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_TESTS=OFF -DDISABLE_FORTRAN=ON || exit /b 1
    "%NINJA%" -C "%FFTW_BUILD%" || exit /b 1
)

echo [3/5] Configure and build eeg_alg.dll
"%CMAKE%" -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DEIGEN3_INCLUDE_DIR="%EIGEN%" -DFFTW3_INCLUDE_DIR="%FFTW_INC%" -DFFTW3_LIBRARY="%FFTW_LIB%" || exit /b 1
"%CMAKE%" --build "%BUILD_DIR%" || exit /b 1

echo [4/5] Stage DLLs for C# app
set "CS_OUT=%ROOT%\csharp\EegAlg.Tests\bin\x64\Release\net8.0"
dotnet build "%ROOT%\csharp\EegAlg.Tests" -c Release || exit /b 1
if not exist "%CS_OUT%" mkdir "%CS_OUT%"
copy /Y "%BUILD_DIR%\eeg_alg.dll" "%CS_OUT%\" >nul
copy /Y "%FFTW_BUILD%\fftw3.dll" "%CS_OUT%\" >nul

echo [5/5] Run C# interop test
"%CS_OUT%\EegAlg.Tests.exe"
exit /b %errorlevel%
