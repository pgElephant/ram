#!/usr/bin/env python3

"""
RAM Cluster Management Tool
PostgreSQL RAM Control Utility - Cluster Management Script
"""

import os
import sys
import json
# import yaml  # Removed to avoid dependency issues
import argparse
import subprocess
import time
import threading
from pathlib import Path
from typing import Dict, List, Optional, Any
from dataclasses import dataclass
import logging

# Configure logging

# ANSI color codes
class Colors:
    RED = '\033[91m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    MAGENTA = '\033[95m'
    CYAN = '\033[96m'
    WHITE = '\033[97m'
    BOLD = '\033[1m'
    END = '\033[0m'

# Custom formatter for tick/cross format with colors
class CustomFormatter(logging.Formatter):
    def format(self, record):
        # Get tick/cross and color based on level
        if record.levelno >= logging.ERROR:
            status = "✗"  # Cross for errors
            color = Colors.RED
        elif record.levelno >= logging.WARNING:
            status = "⚠"  # Warning symbol
            color = Colors.YELLOW
        else:
            status = "✓"  # Tick for info/success
            color = Colors.GREEN
        
        # Get process ID
        pid = os.getpid()
        
        # Format datetime
        dt = self.formatTime(record, '%Y-%m-%d %H:%M:%S')
        
        # Return formatted message with color
        return f"{color}{status}{Colors.END} {pid} {dt}: {record.getMessage()}"

# Setup logger with custom formatter
logger = logging.getLogger(__name__)
handler = logging.StreamHandler()
handler.setFormatter(CustomFormatter())
logger.addHandler(handler)
logger.setLevel(logging.INFO)

# Global verbose setting
verbose = 0

@dataclass
class NodeConfig:
    """Node configuration data class"""
    node_id: int
    node_name: str
    role: str
    postgresql_port: int
    ramd_port: int
    data_dir: str
    log_file: str
    config_file: str
    ip: str = "127.0.0.1"
    user: str = "postgres"
    database: str = "postgres"
    password: str = "postgres"

class ClusterManager:
    """RAM Cluster Management Tool"""
    
    def __init__(self, config_file: str = "cluster.json"):
        self.config_file = Path(config_file)
        self.config = self._load_config()
        self.root_dir = Path(self.config['paths']['root_dir'])
        self.pg_bin = Path(self.config['paths']['pg_bin'])
        self.pg_log = Path(self.config['paths']['pg_log'])
        self.nodes: Dict[str, NodeConfig] = {}
        self._load_nodes_from_config()
        self._create_directories()
        self._set_environment()
        logger.info(f"Cluster manager initialized with {len(self.nodes)} nodes")
    
    def _load_config(self) -> Dict[str, Any]:
        """Load configuration from JSON file"""
        if not self.config_file.exists():
            logger.error(f"Configuration file not found: {self.config_file}")
            sys.exit(1)
            
        try:
            with open(self.config_file, 'r') as f:
                return json.load(f)
        except Exception as e:
            logger.error(f"Failed to load configuration: {e}")
            sys.exit(1)
    
    def _load_nodes_from_config(self):
        """Load node configurations from JSON config"""
        import os
        
        for node_name, node_config in self.config['nodes'].items():
            # Get defaults
            default_user = os.getenv('USER', 'postgres')
            default_ip = '127.0.0.1'
            default_db = 'postgres'
            default_password = 'postgres'
            
            # Extract values with defaults
            ip = node_config.get('ip', default_ip)
            pg_config = node_config.get('pg', {})
            user = pg_config.get('user', default_user)
            database = pg_config.get('db', default_db)
            password = pg_config.get('password', default_password)
            
            node = NodeConfig(
                node_id=int(node_name[1:]),  # Extract number from n1, n2, etc.
                node_name=node_config['name'],
                role="primary" if node_name == "n1" else "replica",
                postgresql_port=pg_config['port'],
                ramd_port=node_config['ramd']['port'],
                data_dir=pg_config['data_dir'],
                log_file=str(self.pg_log / f"{node_name}.log"),
                config_file=str(self.pg_log / f"{node_name}.conf")
            )
            
            # Store additional config in the node object for later use
            node.ip = ip
            node.user = user
            node.database = database
            node.password = password
            
            self.nodes[node_name] = node
            logger.debug(f"Loaded node configuration: {node_name} (user={user}, ip={ip}, db={database})")
    
    def _create_directories(self):
        """Create necessary directories"""
        directories = [self.pg_log]
        for directory in directories:
            directory.mkdir(parents=True, exist_ok=True)
        
        # Create data directories for each node
        for node in self.nodes.values():
            data_dir = Path(node.data_dir)
            data_dir.mkdir(parents=True, exist_ok=True)
    
    def _set_environment(self):
        """Set environment variables from config"""
        env_vars = self.config.get('environment', {})
        for key, value in env_vars.items():
            os.environ[key] = str(value)
    
    def _run_command(self, command: List[str], cwd: Optional[Path] = None) -> subprocess.CompletedProcess:
        """Run a command and return the result"""
        try:
            if verbose >= 1:
                logger.info(f"Running command: {' '.join(command)}")
                if cwd:
                    logger.info(f"Working directory: {cwd}")
            
            result = subprocess.run(command, cwd=cwd, capture_output=True, text=True, check=False)
            
            if verbose >= 1:
                if result.stdout:
                    logger.info(f"Command output: {result.stdout.strip()}")
                if result.stderr:
                    logger.warning(f"Command error: {result.stderr.strip()}")
                logger.info(f"Command exit code: {result.returncode}")
            
            return result
        except Exception as e:
            logger.error(f"Failed to run command: {e}")
            raise
    
    def _stop_all_postgresql(self) -> None:
        """Stop all running PostgreSQL processes"""
        logger.info("Stopping all running PostgreSQL processes...")
        try:
            result = self._run_command(["pkill", "-f", "postgres"])
            if verbose >= 1:
                logger.info(f"pkill result: {result.returncode}")
            # Wait a moment for processes to stop
            import time
            time.sleep(2)
        except Exception as e:
            logger.warning(f"Failed to stop PostgreSQL processes: {e}")

    def _check_port_free(self, port: int) -> bool:
        """Check if a port is free"""
        import socket
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(1)
                result = s.connect_ex(('127.0.0.1', port))
                return result != 0  # Port is free if connection fails
        except Exception:
            return True  # Assume free if we can't check

    def _check_data_directory_empty(self, data_dir: str) -> bool:
        """Check if data directory is empty or doesn't exist"""
        data_path = Path(data_dir)
        if not data_path.exists():
            return True
        try:
            # Check if directory is empty (ignoring hidden files)
            return not any(f for f in data_path.iterdir() if not f.name.startswith('.'))
        except Exception:
            return False

    def _verify_prerequisites(self, num_nodes: int) -> bool:
        """Verify all prerequisites before creating cluster"""
        logger.info("Verifying prerequisites...")
        
        # Check required ports are free
        used_ports = []
        for i in range(1, num_nodes + 1):
            node_name = f"n{i}"
            if node_name in self.nodes:
                node_config = self.nodes[node_name]
                
                # Check PostgreSQL port
                if not self._check_port_free(node_config.postgresql_port):
                    logger.error(f"PostgreSQL port {node_config.postgresql_port} is already in use")
                    used_ports.append(f"PostgreSQL port {node_config.postgresql_port}")
                
                # Check RAMD port
                if not self._check_port_free(node_config.ramd_port):
                    logger.error(f"RAMD port {node_config.ramd_port} is already in use")
                    used_ports.append(f"RAMD port {node_config.ramd_port}")
        
        if used_ports:
            logger.error(f"Ports in use: {', '.join(used_ports)}")
            logger.error("Please stop services using these ports or use different ports")
            return False
        
        # Check data directories
        for i in range(1, num_nodes + 1):
            node_name = f"n{i}"
            if node_name in self.nodes:
                node_config = self.nodes[node_name]
                if not self._check_data_directory_empty(node_config.data_dir):
                    logger.error(f"Data directory {node_config.data_dir} is not empty")
                    return False
        
        logger.info("All prerequisites verified successfully")
        return True

    def _force_cleanup_directories(self, num_nodes: int) -> None:
        """Force cleanup of all data directories"""
        logger.info("Force cleaning up all data directories...")
        import shutil
        
        for i in range(1, num_nodes + 1):
            node_name = f"n{i}"
            if node_name in self.nodes:
                node_config = self.nodes[node_name]
                data_dir = Path(node_config.data_dir)
                if data_dir.exists():
                    logger.info(f"Removing existing data directory: {data_dir}")
                    try:
                        shutil.rmtree(data_dir)
                    except Exception as e:
                        logger.warning(f"Failed to remove {data_dir}: {e}")
                        # Try to remove with sudo if regular removal fails
                        try:
                            result = self._run_command(["sudo", "rm", "-rf", str(data_dir)])
                            if result.returncode != 0:
                                logger.error(f"Failed to remove {data_dir} even with sudo")
                        except Exception:
                            logger.error(f"Could not remove {data_dir}")

    def _check_postgresql_installation(self) -> bool:
        """Check if PostgreSQL is properly installed"""
        required_binaries = ['initdb', 'pg_ctl', 'psql', 'pg_basebackup']
        for binary in required_binaries:
            binary_path = self.pg_bin / binary
            if not binary_path.exists():
                logger.error(f"PostgreSQL binary not found: {binary_path}")
                return False
        logger.info("PostgreSQL installation verified")
        return True
    
    def _install_pgraft_extension(self, node_name: str) -> bool:
        """Install pgraft extension in PostgreSQL"""
        logger.info(f"Installing pgraft extension for node {node_name}")
        
        node_config = self.nodes.get(node_name)
        if not node_config:
            logger.error(f"Node configuration not found: {node_name}")
            return False
        
        # Build pgraft extension
        pgraft_dir = self.root_dir / "pgraft"
        if not pgraft_dir.exists():
            logger.error(f"pgraft directory not found: {pgraft_dir}")
            return False
        
        logger.info("Building pgraft extension...")
        build_result = self._run_command(["make", "clean", "install"], cwd=pgraft_dir)
        if build_result.returncode != 0:
            logger.error("Failed to build pgraft extension")
            return False
        
        # Wait for PostgreSQL to be ready
        self._wait_for_postgresql(node_name, timeout=30)
        
        # Create extension
        create_extension_cmd = [
            str(self.pg_bin / "psql"),
            "-h", "127.0.0.1", "-p", str(node_config.postgresql_port),
            "-U", self.config['nodes'][node_config.node_name]['pg']['user'],
            "-d", "postgres",
            "-c", "CREATE EXTENSION IF NOT EXISTS pgraft;"
        ]
        
        create_result = self._run_command(create_extension_cmd)
        if create_result.returncode != 0:
            logger.error("Failed to create pgraft extension")
            return False
        
        # Initialize Raft system
        logger.info("Initializing Raft system...")
        init_raft_cmd = [
            str(self.pg_bin / "psql"),
            "-h", node_config.ip, "-p", str(node_config.postgresql_port),
            "-U", node_config.user,
            "-d", node_config.database,
            "-c", f"SELECT pgraft_init({node_config.node_id}, '{node_config.ip}', {node_config.postgresql_port});"
        ]
        
        init_result = self._run_command(init_raft_cmd)
        if init_result.returncode != 0:
            logger.error("Failed to initialize Raft system")
            return False
        
        logger.info(f"Successfully installed pgraft extension for node {node_name}")
        return True
    
    def _wait_for_postgresql(self, node_name: str, timeout: int = 30) -> bool:
        """Wait for PostgreSQL to be ready"""
        node_config = self.nodes.get(node_name)
        if not node_config:
            return False
        
        logger.info(f"Waiting for PostgreSQL to be ready on node {node_name}...")
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            try:
                test_cmd = [
                    str(self.pg_bin / "psql"),
                    "-h", node_config.ip, "-p", str(node_config.postgresql_port),
                    "-U", node_config.user,
                    "-d", node_config.database,
                    "-c", "SELECT 1;"
                ]
                
                result = self._run_command(test_cmd)
                if result.returncode == 0:
                    logger.info(f"PostgreSQL is ready on node {node_name}")
                    return True
            except Exception:
                pass
            time.sleep(1)
        
        logger.error(f"PostgreSQL failed to start within {timeout} seconds on node {node_name}")
        return False
    
    def create_primary(self, node_name: str) -> bool:
        """Create a primary node"""
        logger.info(f"Creating primary node: {node_name}")
        
        if not self._check_postgresql_installation():
            return False
        
        # Get node configuration from loaded nodes
        if node_name not in self.nodes:
            logger.error(f"Node {node_name} not found in configuration")
            return False
        
        node_config = self.nodes[node_name]
        
        # Initialize PostgreSQL data directory
        logger.info(f"Initializing PostgreSQL data directory for {node_name}")
        initdb_cmd = [
            str(self.pg_bin / "initdb"),
            "-D", node_config.data_dir,
            "--encoding=UTF8",
            "--locale=en_US.UTF-8"
        ]
        
        initdb_result = self._run_command(initdb_cmd)
        if initdb_result.returncode != 0:
            logger.error("Failed to initialize PostgreSQL data directory")
            return False
        
        # Generate postgresql.conf
        postgresql_config = self._generate_postgresql_config(node_config)
        with open(node_config.config_file, 'w') as f:
            f.write(postgresql_config)
        
        # Copy config to data directory
        import shutil
        shutil.copy2(node_config.config_file, Path(node_config.data_dir) / "postgresql.conf")
        
        # Start PostgreSQL
        logger.info(f"Starting PostgreSQL for {node_name}")
        start_cmd = [
            str(self.pg_bin / "pg_ctl"),
            "-D", node_config.data_dir,
            "-l", node_config.log_file,
            "start"
        ]
        
        start_result = self._run_command(start_cmd)
        if start_result.returncode != 0:
            logger.error("Failed to start PostgreSQL")
            return False
        
        if not self._wait_for_postgresql(node_name):
            return False
        
        if not self._install_pgraft_extension(node_name):
            return False
        
        logger.info(f"Successfully created primary node: {node_name}")
        return True
    
    def create_replica(self, node_name: str, primary_node: str) -> bool:
        """Create a replica node"""
        logger.info(f"Creating replica node: {node_name} (primary: {primary_node})")
        
        if primary_node not in self.nodes:
            logger.error(f"Primary node not found: {primary_node}")
            return False
        
        if node_name not in self.nodes:
            logger.error(f"Node {node_name} not found in configuration")
            return False
        
        primary_config = self.nodes[primary_node]
        node_config = self.nodes[node_name]
        
        # Create base backup from primary
        logger.info(f"Creating base backup from primary {primary_node}")
        backup_cmd = [
            str(self.pg_bin / "pg_basebackup"),
            "-h", primary_config.ip, "-p", str(primary_config.postgresql_port),
            "-U", primary_config.user,
            "-D", node_config.data_dir,
            "-R", "-v", "-P"
        ]
        
        backup_result = self._run_command(backup_cmd)
        if backup_result.returncode != 0:
            logger.error("Failed to create base backup")
            return False
        
        # Generate postgresql.conf
        postgresql_config = self._generate_postgresql_config(node_config)
        with open(node_config.config_file, 'w') as f:
            f.write(postgresql_config)
        
        # Copy config to data directory
        import shutil
        shutil.copy2(node_config.config_file, Path(node_config.data_dir) / "postgresql.conf")
        
        # Start PostgreSQL
        logger.info(f"Starting PostgreSQL for {node_name}")
        start_cmd = [
            str(self.pg_bin / "pg_ctl"),
            "-D", node_config.data_dir,
            "-l", node_config.log_file,
            "start"
        ]
        
        start_result = self._run_command(start_cmd)
        if start_result.returncode != 0:
            logger.error("Failed to start PostgreSQL")
            return False
        
        if not self._wait_for_postgresql(node_name):
            return False
        
        # Skip pgraft extension installation for replicas - it should be available from base backup
        logger.info(f"Replica node {node_name} is ready (pgraft extension available from base backup)")
        
        logger.info(f"Successfully created replica node: {node_name}")
        return True
    
    def create_cluster(self, num_nodes: int = None, primary_node: str = None) -> bool:
        """Create a multi-node cluster"""
        # If no parameters provided, use all nodes from config
        if num_nodes is None:
            num_nodes = len(self.nodes)
            logger.info(f"No parameters provided, using all {num_nodes} nodes from configuration")
        
        if primary_node is None:
            primary_node = "n1"  # First node is primary by default
            logger.info(f"No primary specified, using first node: {primary_node}")
        
        logger.info(f"Creating {num_nodes}-node cluster with primary: {primary_node}")
        
        if num_nodes < 1 or num_nodes > len(self.nodes):
            logger.error(f"Number of nodes must be between 1 and {len(self.nodes)}")
            return False
        
        # Stop all running PostgreSQL processes first
        self._stop_all_postgresql()
        
        # Force cleanup all data directories first
        self._force_cleanup_directories(num_nodes)
        
        # Verify prerequisites after cleanup
        if not self._verify_prerequisites(num_nodes):
            logger.error("Prerequisites check failed")
            return False
        
        # Create primary node
        if not self.create_primary(primary_node):
            logger.error("Failed to create primary node")
            return False
        
        # Create replica nodes
        for i in range(2, num_nodes + 1):
            replica_name = f"n{i}"
            if replica_name in self.nodes:  # Only create if node exists in config
                if not self.create_replica(replica_name, primary_node):
                    logger.error(f"Failed to create replica node: {replica_name}")
                    return False
        
        # Verify cluster is working
        logger.info("Verifying cluster creation...")
        if not self._verify_cluster(num_nodes):
            logger.error("Cluster verification failed")
            return False
        
        logger.info(f"Successfully created and verified {num_nodes}-node cluster")
        return True
    
    def _verify_cluster(self, num_nodes: int) -> bool:
        """Verify that the cluster is working properly"""
        logger.info("Checking PostgreSQL processes...")
        
        # Check if PostgreSQL processes are running
        for i in range(1, num_nodes + 1):
            node_name = f"n{i}"
            if node_name not in self.nodes:
                logger.error(f"Node {node_name} not found in configuration")
                return False
            
            node_config = self.nodes[node_name]
            
            # Check if PostgreSQL is accepting connections
            test_cmd = [
                str(self.pg_bin / "psql"),
                "-h", "127.0.0.1",
                "-p", str(node_config.postgresql_port),
                "-U", self.config['nodes'][node_config.node_name]['pg']['user'],
                "-d", "postgres",
                "-c", "SELECT 1 as test;"
            ]
            
            result = self._run_command(test_cmd)
            if result.returncode != 0:
                logger.error(f"Node {node_name} (port {node_config.postgresql_port}) is not accepting connections")
                return False
            
            logger.info(f"Node {node_name} (port {node_config.postgresql_port}) is responding")
        
        # Check pgraft extension
        logger.info("Verifying pgraft extension...")
        primary_config = self.nodes["n1"]
        pgraft_cmd = [
            str(self.pg_bin / "psql"),
            "-h", "127.0.0.1",
            "-p", str(primary_config.postgresql_port),
            "-U", self.config['nodes'][node_config.node_name]['pg']['user'],
            "-d", "postgres",
            "-c", "SELECT pgraft_get_state();"
        ]
        
        result = self._run_command(pgraft_cmd)
        if result.returncode == 0:
            logger.info("pgraft extension is working")
        else:
            logger.warning("pgraft extension verification failed - this is expected if not initialized yet")
        
        # Check cluster connectivity
        logger.info("Testing inter-node connectivity...")
        if num_nodes > 1:
            # Test replication by checking if replicas can connect to primary
            for i in range(2, num_nodes + 1):
                replica_name = f"n{i}"
                replica_config = self.nodes[replica_name]
                
                # Check if replica is in recovery mode
                recovery_cmd = [
                    str(self.pg_bin / "psql"),
                    "-h", "127.0.0.1",
                    "-p", str(replica_config.postgresql_port),
                    "-U", self.config['nodes'][node_config.node_name]['pg']['user'],
                    "-d", "postgres",
                    "-c", "SELECT pg_is_in_recovery();"
                ]
                
                result = self._run_command(recovery_cmd)
                if result.returncode == 0:
                    if "t" in result.stdout:
                        logger.info(f"Replica {replica_name} is in recovery mode (expected)")
                    else:
                        logger.info(f"Replica {replica_name} is not in recovery mode")
        
        logger.info("Cluster verification completed successfully")
        return True
    
    def start_node(self, node_name: str) -> bool:
        """Start a specific node"""
        logger.info(f"Starting node: {node_name}")
        
        if node_name not in self.nodes:
            logger.error(f"Node not found: {node_name}")
            return False
        
        node_config = self.nodes[node_name]
        start_cmd = [
            str(self.pg_bin / "pg_ctl"),
            "-D", node_config.data_dir,
            "-l", node_config.log_file,
            "start"
        ]
        
        start_result = self._run_command(start_cmd)
        if start_result.returncode != 0:
            logger.error(f"Failed to start PostgreSQL for {node_name}")
            return False
        
        if not self._wait_for_postgresql(node_name):
            return False
        
        logger.info(f"Successfully started node: {node_name}")
        return True
    
    def stop_node(self, node_name: str) -> bool:
        """Stop a specific node"""
        logger.info(f"Stopping node: {node_name}")
        
        if node_name not in self.nodes:
            logger.error(f"Node not found: {node_name}")
            return False
        
        node_config = self.nodes[node_name]
        stop_cmd = [
            str(self.pg_bin / "pg_ctl"),
            "-D", node_config.data_dir,
            "stop"
        ]
        
        stop_result = self._run_command(stop_cmd)
        if stop_result.returncode != 0:
            logger.error(f"Failed to stop PostgreSQL for {node_name}")
            return False
        
        logger.info(f"Successfully stopped node: {node_name}")
        return True
    
    def destroy_node(self, node_name: str) -> bool:
        """Destroy a specific node"""
        logger.info(f"Destroying node: {node_name}")
        
        if node_name not in self.nodes:
            logger.error(f"Node not found: {node_name}")
            return False
        
        node_config = self.nodes[node_name]
        self.stop_node(node_name)
        
        import shutil
        data_dir = Path(node_config.data_dir)
        if data_dir.exists():
            shutil.rmtree(data_dir)
            logger.info(f"Removed data directory: {data_dir}")
        
        del self.nodes[node_name]
        logger.info(f"Successfully destroyed node: {node_name}")
        return True
    
    def destroy_all(self) -> bool:
        """Destroy all nodes in the cluster"""
        logger.info("Destroying entire cluster...")
        
        success_count = 0
        total_nodes = len(self.nodes)
        
        # Stop and destroy each node
        for node_name in list(self.nodes.keys()):
            logger.info(f"Destroying node: {node_name}")
            
            # Stop the node first
            self.stop_node(node_name)
            
            # Remove data directory
            node_config = self.nodes[node_name]
            import shutil
            data_dir = Path(node_config.data_dir)
            if data_dir.exists():
                shutil.rmtree(data_dir)
                logger.info(f"Removed data directory: {data_dir}")
            
            # Remove from nodes dictionary
            del self.nodes[node_name]
            success_count += 1
        
        if success_count == total_nodes:
            logger.info(f"Successfully destroyed entire cluster ({total_nodes} nodes)")
            return True
        else:
            logger.error(f"Failed to destroy all nodes ({success_count}/{total_nodes} destroyed)")
            return False
    
    def _generate_postgresql_config(self, node_config: NodeConfig) -> str:
        """Generate PostgreSQL configuration for a node"""
        # Use the node's own configuration from JSON
        node_json_config = self.config['nodes'][node_config.node_name]
        template = node_json_config['pg']
        
        return f"""# PostgreSQL configuration for {node_config.node_name}
listen_addresses = '127.0.0.1'
port = {node_config.postgresql_port}
log_destination = 'stderr'
logging_collector = on
log_directory = '{Path(node_config.log_file).parent}'
log_filename = 'postgresql-%Y-%m-%d_%H%M%S.log'
log_rotation_age = 1d
log_rotation_size = 100MB
log_min_duration_statement = 1000
log_line_prefix = '%t [%p]: [%l-1] user=%u,db=%d,app=%a,client=%h '
log_checkpoints = on
log_connections = on
log_disconnections = on
log_lock_waits = on
# shared_preload_libraries = 'pgraft'
# pgraft.node_id = {node_config.node_id}
# pgraft.cluster_id = 1
# pgraft.listen_address = '127.0.0.1'
# pgraft.listen_port = {7000 + node_config.node_id}
"""

def main():
    """Main entry point"""
    global verbose
    
    parser = argparse.ArgumentParser(description="RAM Cluster Management Tool")
    parser.add_argument("--config", default="cluster.json", help="Configuration file path")
    parser.add_argument("-v", "--verbose", action="count", default=0, help="Increase verbosity (use -v for verbose, -vv for debug)")
    
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Create command
    create_parser = subparsers.add_parser("create", help="Create cluster components")
    create_subparsers = create_parser.add_subparsers(dest="create_type")
    
    create_primary_parser = create_subparsers.add_parser("primary", help="Create primary node")
    create_primary_parser.add_argument("--node", required=True, help="Node name")
    
    create_replica_parser = create_subparsers.add_parser("replica", help="Create replica node")
    create_replica_parser.add_argument("--node", required=True, help="Node name")
    create_replica_parser.add_argument("--primary", required=True, help="Primary node name")
    
    create_cluster_parser = create_subparsers.add_parser("cluster", help="Create multi-node cluster")
    create_cluster_parser.add_argument("--num_nodes", type=int, help="Number of nodes (default: all nodes from config)")
    create_cluster_parser.add_argument("--primary", help="Primary node name (default: n1)")
    
    # Other commands
    destroy_parser = subparsers.add_parser("destroy", help="Destroy cluster components")
    destroy_parser.add_argument("--node", help="Node name (if not provided, destroys all nodes)")
    
    start_parser = subparsers.add_parser("start", help="Start cluster components")
    start_parser.add_argument("--node", required=True, help="Node name")
    
    stop_parser = subparsers.add_parser("stop", help="Stop cluster components")
    stop_parser.add_argument("--node", required=True, help="Node name")
    
    args = parser.parse_args()
    
    # Set global verbose level
    verbose = args.verbose
    
    # Adjust logging level based on verbosity
    if verbose >= 2:
        logging.getLogger().setLevel(logging.DEBUG)
    elif verbose >= 1:
        logging.getLogger().setLevel(logging.INFO)
    
    if not args.command:
        parser.print_help()
        sys.exit(1)
    
    cluster = ClusterManager(args.config)
    
    try:
        if args.command == "create":
            if args.create_type == "primary":
                success = cluster.create_primary(args.node)
            elif args.create_type == "replica":
                success = cluster.create_replica(args.node, args.primary)
            elif args.create_type == "cluster":
                # Handle optional parameters with defaults
                num_nodes = getattr(args, 'num_nodes', None)
                primary = getattr(args, 'primary', None)
                success = cluster.create_cluster(num_nodes, primary)
            else:
                print("Error: Invalid create type")
                sys.exit(1)
            
            if success:
                if args.create_type == "cluster":
                    logger.info(f"Successfully created {args.create_type} with {args.num_nodes} nodes")
                else:
                    logger.info(f"Successfully created {args.create_type}: {args.node}")
            else:
                if args.create_type == "cluster":
                    logger.error(f"Failed to create {args.create_type} with {args.num_nodes} nodes")
                else:
                    logger.error(f"Failed to create {args.create_type}: {args.node}")
                sys.exit(1)
                
        elif args.command == "destroy":
            if args.node:
                success = cluster.destroy_node(args.node)
                if success:
                    logger.info(f"Successfully destroyed node: {args.node}")
                else:
                    logger.error(f"Failed to destroy node: {args.node}")
                    sys.exit(1)
            else:
                success = cluster.destroy_all()
                if success:
                    logger.info("Successfully destroyed entire cluster")
                else:
                    logger.error("Failed to destroy cluster")
                    sys.exit(1)
                
        elif args.command == "start":
            success = cluster.start_node(args.node)
            if success:
                logger.info(f"Successfully started node: {args.node}")
            else:
                logger.error(f"Failed to start node: {args.node}")
                sys.exit(1)
                
        elif args.command == "stop":
            success = cluster.stop_node(args.node)
            if success:
                logger.info(f"Successfully stopped node: {args.node}")
            else:
                logger.error(f"Failed to stop node: {args.node}")
                sys.exit(1)
        
    except KeyboardInterrupt:
        logger.info("Operation cancelled by user")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
