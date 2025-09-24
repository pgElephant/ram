#!/bin/bash
# RAM PostgreSQL High Availability System - Installation Script
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

# Check if running as root
check_root() {
    if [[ $EUID -eq 0 ]]; then
        log_warning "Running as root. This is not recommended for security reasons."
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    fi
}

# Check system requirements
check_requirements() {
    log_info "Checking system requirements..."
    
    # Check OS
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        OS="linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        OS="macos"
    else
        log_error "Unsupported operating system: $OSTYPE"
        exit 1
    fi
    
    log_success "Operating system: $OS"
    
    # Check required commands
    local required_commands=("gcc" "make" "git" "curl" "wget")
    for cmd in "${required_commands[@]}"; do
        if ! command -v "$cmd" &> /dev/null; then
            log_error "Required command not found: $cmd"
            exit 1
        fi
    done
    
    log_success "All required commands found"
    
    # Check PostgreSQL
    if ! command -v "psql" &> /dev/null; then
        log_error "PostgreSQL client not found. Please install PostgreSQL first."
        exit 1
    fi
    
    # Check PostgreSQL version
    local pg_version=$(psql --version | grep -oE '[0-9]+\.[0-9]+' | head -1)
    local pg_major=$(echo $pg_version | cut -d. -f1)
    
    if [ "$pg_major" -lt 15 ]; then
        log_error "PostgreSQL version $pg_version is not supported. Please install PostgreSQL 15 or later."
        exit 1
    fi
    
    log_success "PostgreSQL version: $pg_version"
    
    # Check Go
    if ! command -v "go" &> /dev/null; then
        log_error "Go not found. Please install Go 1.21 or later."
        exit 1
    fi
    
    local go_version=$(go version | grep -oE 'go[0-9]+\.[0-9]+' | head -1)
    log_success "Go version: $go_version"
}

# Install dependencies
install_dependencies() {
    log_info "Installing dependencies..."
    
    if [[ "$OS" == "linux" ]]; then
        # Detect package manager
        if command -v "apt-get" &> /dev/null; then
            PKG_MGR="apt"
        elif command -v "yum" &> /dev/null; then
            PKG_MGR="yum"
        elif command -v "dnf" &> /dev/null; then
            PKG_MGR="dnf"
        elif command -v "zypper" &> /dev/null; then
            PKG_MGR="zypper"
        else
            log_error "No supported package manager found"
            exit 1
        fi
        
        case $PKG_MGR in
            "apt")
                sudo apt-get update
                sudo apt-get install -y build-essential libpq-dev libcurl4-openssl-dev libjansson-dev
                ;;
            "yum"|"dnf")
                sudo $PKG_MGR install -y gcc make postgresql-devel libcurl-devel jansson-devel
                ;;
            "zypper")
                sudo zypper install -y gcc make postgresql-devel libcurl-devel jansson-devel
                ;;
        esac
    elif [[ "$OS" == "macos" ]]; then
        if ! command -v "brew" &> /dev/null; then
            log_error "Homebrew not found. Please install Homebrew first."
            exit 1
        fi
        
        brew install postgresql@17 libpq curl jansson
    fi
    
    log_success "Dependencies installed"
}

# Build the project
build_project() {
    log_info "Building RAM system..."
    
    # Clean previous builds
    make clean 2>/dev/null || true
    
    # Configure
    log_info "Configuring build..."
    if [ -f "configure" ]; then
        ./configure --prefix=/usr/local
    elif [ -f "autogen.sh" ]; then
        ./autogen.sh
        ./configure --prefix=/usr/local
    fi
    
    # Build all components
    log_info "Building PGRaft extension..."
    cd pgraft
    make clean 2>/dev/null || true
    make
    cd ..
    
    log_info "Building RAMD daemon..."
    cd ramd
    make clean 2>/dev/null || true
    make
    cd ..
    
    log_info "Building RAMCTRL CLI..."
    cd ramctrl
    make clean 2>/dev/null || true
    make
    cd ..
    
    log_success "Build completed successfully"
}

# Install the project
install_project() {
    log_info "Installing RAM system..."
    
    # Install PGRaft extension
    log_info "Installing PGRaft extension..."
    cd pgraft
    sudo make install
    cd ..
    
    # Install RAMD daemon
    log_info "Installing RAMD daemon..."
    cd ramd
    sudo make install
    cd ..
    
    # Install RAMCTRL CLI
    log_info "Installing RAMCTRL CLI..."
    cd ramctrl
    sudo make install
    cd ..
    
    # Install configuration files
    log_info "Installing configuration files..."
    sudo mkdir -p /etc/ramd
    sudo cp conf/ramd.conf /etc/ramd/
    sudo cp conf/cluster.json /etc/ramd/
    
    sudo mkdir -p /etc/ramctrl
    sudo cp conf/ramctrl.conf /etc/ramctrl/
    
    sudo mkdir -p /etc/pgraft
    sudo cp conf/pgraft.conf /etc/pgraft/
    
    # Set proper permissions
    sudo chmod 644 /etc/ramd/*.conf
    sudo chmod 644 /etc/ramd/*.json
    sudo chmod 644 /etc/ramctrl/*.conf
    sudo chmod 644 /etc/pgraft/*.conf
    
    log_success "Installation completed successfully"
}

# Configure PostgreSQL
configure_postgresql() {
    log_info "Configuring PostgreSQL..."
    
    # Find PostgreSQL configuration directory
    local pg_config_dir=""
    if command -v "pg_config" &> /dev/null; then
        pg_config_dir=$(pg_config --sysconfdir)
    else
        # Common locations
        for dir in "/etc/postgresql" "/usr/local/pgsql" "/opt/homebrew/var/postgresql@17"; do
            if [ -d "$dir" ]; then
                pg_config_dir="$dir"
                break
            fi
        done
    fi
    
    if [ -z "$pg_config_dir" ]; then
        log_error "PostgreSQL configuration directory not found"
        exit 1
    fi
    
    log_info "PostgreSQL configuration directory: $pg_config_dir"
    
    # Find postgresql.conf
    local postgresql_conf=""
    for conf in "$pg_config_dir/postgresql.conf" "$pg_config_dir/postgresql@17/postgresql.conf"; do
        if [ -f "$conf" ]; then
            postgresql_conf="$conf"
            break
        fi
    done
    
    if [ -z "$postgresql_conf" ]; then
        log_error "postgresql.conf not found"
        exit 1
    fi
    
    log_info "PostgreSQL configuration file: $postgresql_conf"
    
    # Backup original configuration
    sudo cp "$postgresql_conf" "$postgresql_conf.backup.$(date +%Y%m%d_%H%M%S)"
    
    # Add PGRaft to shared_preload_libraries
    if ! grep -q "pgraft" "$postgresql_conf"; then
        echo "shared_preload_libraries = 'pgraft'" | sudo tee -a "$postgresql_conf"
        log_success "Added PGRaft to shared_preload_libraries"
    else
        log_info "PGRaft already in shared_preload_libraries"
    fi
    
    # Restart PostgreSQL
    log_info "Restarting PostgreSQL..."
    if command -v "systemctl" &> /dev/null; then
        sudo systemctl restart postgresql
    elif command -v "brew" &> /dev/null; then
        brew services restart postgresql@17
    else
        log_warning "Please restart PostgreSQL manually"
    fi
    
    log_success "PostgreSQL configured successfully"
}

# Create systemd service files
create_systemd_services() {
    if [[ "$OS" != "linux" ]]; then
        return
    fi
    
    log_info "Creating systemd service files..."
    
    # RAMD service
    sudo tee /etc/systemd/system/ramd.service > /dev/null <<EOF
[Unit]
Description=RAM PostgreSQL High Availability Daemon
After=postgresql.service
Requires=postgresql.service

[Service]
Type=simple
User=postgres
Group=postgres
ExecStart=/usr/local/bin/ramd --config=/etc/ramd/ramd.conf
ExecReload=/bin/kill -HUP \$MAINPID
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
    
    # Enable and start service
    sudo systemctl daemon-reload
    sudo systemctl enable ramd
    
    log_success "Systemd service files created"
}

# Run tests
run_tests() {
    log_info "Running tests..."
    
    cd tests
    if [ -f "test_suite.py" ]; then
        python3 test_suite.py --category=unit
        log_success "Unit tests passed"
    else
        log_warning "Test suite not found, skipping tests"
    fi
    cd ..
}

# Main installation function
main() {
    echo "=========================================="
    echo "RAM PostgreSQL High Availability System"
    echo "Installation Script"
    echo "=========================================="
    echo
    
    check_root
    check_requirements
    install_dependencies
    build_project
    install_project
    configure_postgresql
    create_systemd_services
    run_tests
    
    echo
    echo "=========================================="
    log_success "Installation completed successfully!"
    echo "=========================================="
    echo
    echo "Next steps:"
    echo "1. Start RAMD daemon: sudo systemctl start ramd"
    echo "2. Create your first cluster: ramctrl cluster create --num-nodes=3"
    echo "3. Check cluster status: ramctrl cluster status"
    echo "4. View documentation: cat doc/README.md"
    echo
    echo "Configuration files:"
    echo "- RAMD: /etc/ramd/ramd.conf"
    echo "- RAMCTRL: /etc/ramctrl/ramctrl.conf"
    echo "- PGRaft: /etc/pgraft/pgraft.conf"
    echo "- Cluster: /etc/ramd/cluster.json"
    echo
}

# Run main function
main "$@"
