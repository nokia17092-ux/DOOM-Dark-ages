#!/bin/bash
# DOOM Dark Ages - Build Script for Linux

echo "===================================="
echo "DOOM Dark Ages - Linux Build"
echo "===================================="

# Check Python
python3 --version
if [ $? -ne 0 ]; then
    echo "ERROR: Python not found"
    exit 1
fi

# Install dependencies
echo ""
echo "Installing dependencies..."
pip3 install pygame numpy pillow pyinstaller

# Build
echo ""
echo "Building executable..."
pyinstaller main.py --onefile --name DOOMDarkAges

if [ -f "dist/DOOMDarkAges" ]; then
    chmod +x dist/DOOMDarkAges
    echo ""
    echo "===================================="
    echo "BUILD SUCCESSFUL!"
    echo "File: dist/DOOMDarkAges"
    echo "===================================="
else
    echo ""
    echo "BUILD FAILED!"
fi
