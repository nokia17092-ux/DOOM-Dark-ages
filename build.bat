@echo off
REM DOOM Dark Ages - Build Script for Windows
REM Run this on Windows to create DOOMDarkAges.exe

echo ====================================
echo DOOM Dark Ages - Windows Build
echo ====================================

REM Check Python
python --version
if errorlevel 1 (
    echo ERROR: Python not found. Install from https://python.org
    pause
    exit /b 1
)

REM Install dependencies
echo.
echo Installing dependencies...
pip install pygame numpy pillow pyinstaller

REM Build
echo.
echo Building executable...
pyinstaller main.py --onefile --name DOOMDarkAges --icon=doom_icon.ico --add-data "doom_engine;doom_engine" --add-data "levels;levels" --add-data "assets;assets"

if exist "dist\DOOMDarkAges.exe" (
    echo.
    echo ====================================
    echo BUILD SUCCESSFUL!
    echo File: dist\DOOMDarkAges.exe
    echo ====================================
) else (
    echo.
    echo BUILD FAILED!
)

pause
