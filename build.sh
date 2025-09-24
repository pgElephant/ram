#!/bin/bash
# RAM PostgreSQL High Availability System - Build Script
# Copyright (c) 2024-2025, pgElephant, Inc.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Build configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || echo 4)}"
PREFIX="${PREFIX:-/usr/local}"
PG_CONFIG="${PG_CONFIG:-$(which pg_config 2>/dev/null || echo "")}"

# Validate PostgreSQL installation
validate_postgresql() {
    log_info "Validating PostgreSQL installation..."
    
    if [ -z "$PG_CONFIG" ]; then
        log_error "pg_config not found. Please install PostgreSQL development headers."
        exit 1
    fi
    
    local pg_version=$($PG_CONFIG --version | grep -oE '[0-9]+\.[0-9]+' | head -1)
    local pg_major=$(echo $pg_version | cut -d. -f1)
    
    if [ "$pg_major" -lt 15 ]; then
        log_error "PostgreSQL version $pg_version is not supported. Please install PostgreSQL 15 or later."
        exit 1
    fi
    
    log_success "PostgreSQL version: $pg_version"
    
    # Get PostgreSQL paths
    PG_INCLUDEDIR=$($PG_CONFIG --includedir)
    PG_LIBDIR=$($PG_CONFIG --libdir)
    PG_PKGLIBDIR=$($PG_CONFIG --pkglibdir)
    PG_SHAREDIR=$($PG_CONFIG --sharedir)
    
    log_info "PostgreSQL paths:"
    log_info "  Include: $PG_INCLUDEDIR"
    log_info "  Lib: $PG_LIBDIR"
    log_info "  PkgLib: $PG_PKGLIBDIR"
    log_info "  Share: $PG_SHAREDIR"
}

# Clean previous builds
clean_build() {
    log_info "Cleaning previous builds..."
    
    # Clean PGRaft
    if [ -d "pgraft" ]; then
        cd pgraft
        make clean 2>/dev/null || true
        cd ..
    fi
    
    # Clean RAMD
    if [ -d "ramd" ]; then
        cd ramd
        make clean 2>/dev/null || true
        cd ..
    fi
    
    # Clean RAMCTRL
    if [ -d "ramctrl" ]; then
        cd ramctrl
        make clean 2>/dev/null || true
        cd ..
    fi
    
    log_success "Clean completed"
}

# Build PGRaft extension
build_pgraft() {
    log_info "Building PGRaft extension..."
    
    cd pgraft
    
    # Set PostgreSQL paths
    export PG_CONFIG
    export PG_INCLUDEDIR
    export PG_LIBDIR
    export PG_PKGLIBDIR
    export PG_SHAREDIR
    
    # Build with PostgreSQL standards
    make -j$PARALLEL_JOBS \
        PG_CONFIG="$PG_CONFIG" \
        PG_INCLUDEDIR="$PG_INCLUDEDIR" \
        PG_LIBDIR="$PG_LIBDIR" \
        PG_PKGLIBDIR="$PG_PKGLIBDIR" \
        PG_SHAREDIR="$PG_SHAREDIR" \
        CFLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers" \
        CPPFLAGS="-I$PG_INCLUDEDIR" \
        LDFLAGS="-L$PG_LIBDIR"
    
    cd ..
    log_success "PGRaft extension built successfully"
}

# Build RAMD daemon
build_ramd() {
    log_info "Building RAMD daemon..."
    
    cd ramd
    
    # Set PostgreSQL paths
    export PG_CONFIG
    export PG_INCLUDEDIR
    export PG_LIBDIR
    
    # Build with PostgreSQL standards
    make -j$PARALLEL_JOBS \
        PG_CONFIG="$PG_CONFIG" \
        PG_INCLUDEDIR="$PG_INCLUDEDIR" \
        PG_LIBDIR="$PG_LIBDIR" \
        CFLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers" \
        CPPFLAGS="-I$PG_INCLUDEDIR" \
        LDFLAGS="-L$PG_LIBDIR -lpq -lcurl -ljansson -lssl -lcrypto"
    
    cd ..
    log_success "RAMD daemon built successfully"
}

# Build RAMCTRL CLI
build_ramctrl() {
    log_info "Building RAMCTRL CLI..."
    
    cd ramctrl
    
    # Set PostgreSQL paths
    export PG_CONFIG
    export PG_INCLUDEDIR
    export PG_LIBDIR
    
    # Build with PostgreSQL standards
    make -j$PARALLEL_JOBS \
        PG_CONFIG="$PG_CONFIG" \
        PG_INCLUDEDIR="$PG_INCLUDEDIR" \
        PG_LIBDIR="$PG_LIBDIR" \
        CFLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers" \
        CPPFLAGS="-I$PG_INCLUDEDIR" \
        LDFLAGS="-L$PG_LIBDIR -lpq -lcurl -lssl -lcrypto"
    
    cd ..
    log_success "RAMCTRL CLI built successfully"
}

# Run tests
run_tests() {
    log_info "Running tests..."
    
    # Test PGRaft compilation
    if [ -f "pgraft/pgraft.so" ]; then
        log_success "PGRaft extension compiled successfully"
    else
        log_error "PGRaft extension compilation failed"
        exit 1
    fi
    
    # Test RAMD compilation
    if [ -f "ramd/ramd" ]; then
        log_success "RAMD daemon compiled successfully"
    else
        log_error "RAMD daemon compilation failed"
        exit 1
    fi
    
    # Test RAMCTRL compilation
    if [ -f "ramctrl/ramctrl" ]; then
        log_success "RAMCTRL CLI compiled successfully"
    else
        log_error "RAMCTRL CLI compilation failed"
        exit 1
    fi
    
    log_success "All components compiled successfully"
}

# Install components
install_components() {
    log_info "Installing components..."
    
    # Install PGRaft
    cd pgraft
    sudo make install \
        PG_CONFIG="$PG_CONFIG" \
        PG_INCLUDEDIR="$PG_INCLUDEDIR" \
        PG_LIBDIR="$PG_LIBDIR" \
        PG_PKGLIBDIR="$PG_PKGLIBDIR" \
        PG_SHAREDIR="$PG_SHAREDIR"
    cd ..
    
    # Install RAMD
    cd ramd
    sudo make install \
        PREFIX="$PREFIX" \
        PG_CONFIG="$PG_CONFIG" \
        PG_INCLUDEDIR="$PG_INCLUDEDIR" \
        PG_LIBDIR="$PG_LIBDIR"
    cd ..
    
    # Install RAMCTRL
    cd ramctrl
    sudo make install \
        PREFIX="$PREFIX" \
        PG_CONFIG="$PG_CONFIG" \
        PG_INCLUDEDIR="$PG_INCLUDEDIR" \
        PG_LIBDIR="$PG_LIBDIR"
    cd ..
    
    log_success "All components installed successfully"
}

# Main build function
main() {
    echo "=========================================="
    echo "RAM PostgreSQL High Availability System"
    echo "Build Script"
    echo "=========================================="
    echo
    
    log_info "Build configuration:"
    log_info "  Type: $BUILD_TYPE"
    log_info "  Parallel jobs: $PARALLEL_JOBS"
    log_info "  Prefix: $PREFIX"
    log_info "  PG_CONFIG: $PG_CONFIG"
    echo
    
    validate_postgresql
    clean_build
    build_pgraft
    build_ramd
    build_ramctrl
    run_tests
    
    if [ "$1" = "install" ]; then
        install_components
    fi
    
    echo
    echo "=========================================="
    log_success "Build completed successfully!"
    echo "=========================================="
    echo
    echo "Next steps:"
    echo "1. Configure PostgreSQL: sudo -u postgres psql -c \"ALTER SYSTEM SET shared_preload_libraries = 'pgraft';\""
    echo "2. Restart PostgreSQL: sudo systemctl restart postgresql"
    echo "3. Create extension: sudo -u postgres psql -c \"CREATE EXTENSION pgraft;\""
    echo "4. Start RAMD: sudo systemctl start ramd"
    echo "5. Test cluster: ramctrl cluster status"
    echo
}

# Run main function
main "$@"