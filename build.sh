#!/bin/bash

# PostgreSQL RAM Extension Build Script
# This script performs a clean build of all components

set -e  # Exit on any error

echo "Starting clean build of PostgreSQL RAM extension..."

# Check if we're in the right directory
if [ ! -f "configure" ] || [ ! -d "pgraft" ]; then
    echo "Error: Please run this script from the ram project root directory"
    exit 1
fi

# Clean previous build artifacts
echo "Cleaning previous build artifacts..."
if [ -f "Makefile" ]; then
    make clean || true
fi

# Clean object files and binaries
echo "Removing build artifacts..."
find . -name "*.o" -delete 2>/dev/null || true
find . -name "*.dylib" -delete 2>/dev/null || true
find . -name "*.so" -delete 2>/dev/null || true
rm -f ramctrl/ramctrl 2>/dev/null || true
rm -f ramd/ramd 2>/dev/null || true
rm -f pgraft/pgraft.dylib 2>/dev/null || true

# Clean dependency files
find . -name ".deps" -type d -exec rm -rf {} + 2>/dev/null || true

echo "Clean completed."

# Start the build
echo "Starting compilation..."

# Build all components
make

# Verify build results
echo ""
echo "Build completed! Checking results..."

# Check if all expected binaries were created
MISSING_FILES=""

if [ ! -f "pgraft/pgraft.dylib" ]; then
    MISSING_FILES="$MISSING_FILES pgraft/pgraft.dylib"
fi

if [ ! -f "ramctrl/ramctrl" ]; then
    MISSING_FILES="$MISSING_FILES ramctrl/ramctrl"
fi

if [ ! -f "ramd/ramd" ]; then
    MISSING_FILES="$MISSING_FILES ramd/ramd"
fi

if [ -n "$MISSING_FILES" ]; then
    echo "❌ Build failed! Missing files:$MISSING_FILES"
    exit 1
fi

# Show file sizes and timestamps
echo "✅ Build successful! Generated files:"
ls -lh pgraft/pgraft.dylib ramctrl/ramctrl ramd/ramd

echo ""
echo "Build summary:"
echo "- pgraft.dylib: PostgreSQL extension shared library"
echo "- ramctrl: Control utility for cluster management"  
echo "- ramd: Daemon for monitoring and failover"
echo ""
echo "Build completed successfully!"
