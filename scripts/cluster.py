#!/usr/bin/env python3
"""
RAM Cluster Management Tool
Uses .conf files for configuration (different from ramd.conf)
"""

import os
import sys
import subprocess
import time
import argparse
import configparser
from pathlib import Path
from typing import List, Dict, Optional, Any

class ClusterConfigManager:
    def __init__(self, scripts_dir: Path):
        self.scripts_dir = scripts_dir
        self.conf_dir = scripts_dir / "conf"
        
    def load_config(self, config_name: str) -> configparser.ConfigParser:
        config_path = self.conf_dir / f"{config_name}.conf"
        config = configparser.ConfigParser()
        config.read(config_path)
        return config
    
    def get_available_configs(self) -> List[str]:
        configs = []
        for config_file in self.conf_dir.glob("*.conf"):
            configs.append(config_file.stem)
        return sorted(configs)

class PostgreSQLManager:
    def __init__(self, config: configparser.ConfigParser):
        self.config = config
        self.pg_bin_dir = Path(config.get('postgresql', 'bin_dir'))
        self.data_dir = Path(config.get('postgresql', 'data_dir'))
        self.log_dir = Path(config.get('postgresql', 'log_dir'))
        
    def init_database(self) -> bool:
        if (self.data_dir / "PG_VERSION").exists():
            print(f"Database already initialized at {self.data_dir}")
            return True
            
        self.data_dir.mkdir(parents=True, exist_ok=True)
        self.log_dir.mkdir(parents=True, exist_ok=True)
        
        cmd = [str(self.pg_bin_dir / "initdb"), "-D", str(self.data_dir), "--auth-local=trust"]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"Failed to initialize database: {result.stderr}")
            return False
            
        self._write_config()
        print(f"Successfully initialized database at {self.data_dir}")
        return True
    
    def _write_config(self):
        config_lines = [
            f"port = {self.config.get('network', 'postgresql_port')}",
            f"listen_addresses = '{self.config.get('network', 'postgresql_host')}'",
            "max_connections = 100",
        ]
        
        if self.config.get('node', 'role') == 'primary':
            config_lines.extend([
                "wal_level = replica",
                "max_wal_senders = 10",
                "hot_standby = on",
            ])
        
        with open(self.data_dir / "postgresql.conf", "w") as f:
            for line in config_lines:
                f.write(f"{line}\n")
        
        with open(self.data_dir / "pg_hba.conf", "w") as f:
            f.write("local   all             all                                     trust\n")
            f.write("host    all             all             127.0.0.1/32            trust\n")
            f.write("host    replication     all             127.0.0.1/32            trust\n")
    
    def start(self) -> bool:
        cmd = [str(self.pg_bin_dir / "pg_ctl"), "-D", str(self.data_dir), 
               "-l", str(self.log_dir / "postgres.log"), "-w", "start"]
        result = subprocess.run(cmd, capture_output=True, text=True)
        
        if result.returncode == 0:
            print(f"PostgreSQL started on port {self.config.get('network', 'postgresql_port')}")
            return True
        else:
            print(f"Failed to start PostgreSQL: {result.stderr}")
            return False
    
    def stop(self) -> bool:
        cmd = [str(self.pg_bin_dir / "pg_ctl"), "-D", str(self.data_dir), "-m", "fast", "-w", "stop"]
        subprocess.run(cmd, capture_output=True, text=True)
        print(f"PostgreSQL stopped for {self.config.get('node', 'name')}")
        return True
    
    def is_running(self) -> bool:
        cmd = [str(self.pg_bin_dir / "pg_ctl"), "-D", str(self.data_dir), "status"]
        result = subprocess.run(cmd, capture_output=True, text=True)
        return result.returncode == 0

class ClusterNode:
    def __init__(self, config: configparser.ConfigParser, root_dir: Path):
        self.config = config
        self.name = config.get('node', 'name')
        self.node_id = config.getint('node', 'id')
        self.role = config.get('node', 'role')
        self.pg_manager = PostgreSQLManager(config)
    
    def init(self) -> bool:
        print(f"Initializing node {self.name} (ID: {self.node_id}) as {self.role}")
        return self.pg_manager.init_database()
    
    def start(self) -> bool:
        print(f"Starting node {self.name}")
        return self.pg_manager.start()
    
    def stop(self) -> bool:
        print(f"Stopping node {self.name}")
        return self.pg_manager.stop()
    
    def status(self) -> Dict[str, Any]:
        return {
            'name': self.name,
            'id': self.node_id,
            'role': self.role,
            'postgresql_running': self.pg_manager.is_running(),
            'port': self.config.getint('network', 'postgresql_port'),
        }

class RAMCluster:
    def __init__(self, scripts_dir: Optional[Path] = None):
        if scripts_dir is None:
            scripts_dir = Path(__file__).parent.resolve()
        self.scripts_dir = scripts_dir
        self.config_manager = ClusterConfigManager(scripts_dir)
        self.nodes: Dict[str, ClusterNode] = {}
        
    def load_node(self, config_name: str) -> bool:
        try:
            config = self.config_manager.load_config(config_name)
            node = ClusterNode(config, self.scripts_dir.parent)
            self.nodes[config_name] = node
            print(f"Loaded node configuration: {config_name}")
            return True
        except Exception as e:
            print(f"Failed to load node {config_name}: {e}")
            return False
    
    def init_cluster(self, node_configs: Optional[List[str]] = None) -> bool:
        if node_configs is None:
            node_configs = self.config_manager.get_available_configs()
        
        print(f"Initializing cluster with nodes: {', '.join(node_configs)}")
        success = True
        for config_name in node_configs:
            if not self.load_node(config_name):
                success = False
                continue
            if not self.nodes[config_name].init():
                success = False
        return success
    
    def start_cluster(self, node_configs: Optional[List[str]] = None) -> bool:
        if node_configs is None:
            node_configs = list(self.nodes.keys())
        
        print(f"Starting cluster nodes: {', '.join(node_configs)}")
        primary_nodes = [name for name in node_configs if self.nodes.get(name) and self.nodes[name].role == 'primary']
        replica_nodes = [name for name in node_configs if self.nodes.get(name) and self.nodes[name].role == 'standby']
        
        success = True
        for node_name in primary_nodes:
            if not self.nodes[node_name].start():
                success = False
        
        if primary_nodes:
            time.sleep(2)
        
        for node_name in replica_nodes:
            if not self.nodes[node_name].start():
                success = False
        return success
    
    def stop_cluster(self) -> bool:
        print("Stopping cluster")
        success = True
        for node in self.nodes.values():
            if not node.stop():
                success = False
        return success
    
    def print_status(self):
        print("\n=== RAM Cluster Status ===")
        for name, node in self.nodes.items():
            status = node.status()
            pg_status = "Running" if status['postgresql_running'] else "Stopped"
            print(f"Node: {status['name']} (ID: {status['id']})")
            print(f"  Role: {status['role']}")
            print(f"  PostgreSQL: {pg_status} (Port: {status['port']})")
            print()

def main():
    parser = argparse.ArgumentParser(description="RAM Cluster Management Tool")
    parser.add_argument("command", choices=['init', 'start', 'stop', 'status', 'restart'])
    parser.add_argument("--nodes", nargs='+', help="Specific nodes to operate on")
    
    args = parser.parse_args()
    
    try:
        cluster = RAMCluster()
        
        if args.command == 'init':
            success = cluster.init_cluster(args.nodes)
            sys.exit(0 if success else 1)
            
        elif args.command == 'start':
            if args.nodes:
                for node_name in args.nodes:
                    cluster.load_node(node_name)
            else:
                for config_name in cluster.config_manager.get_available_configs():
                    cluster.load_node(config_name)
            success = cluster.start_cluster(args.nodes)
            sys.exit(0 if success else 1)
            
        elif args.command == 'stop':
            for config_name in cluster.config_manager.get_available_configs():
                cluster.load_node(config_name)
            success = cluster.stop_cluster()
            sys.exit(0 if success else 1)
            
        elif args.command == 'status':
            for config_name in cluster.config_manager.get_available_configs():
                cluster.load_node(config_name)
            cluster.print_status()
            
        elif args.command == 'restart':
            for config_name in cluster.config_manager.get_available_configs():
                cluster.load_node(config_name)
            print("Restarting cluster...")
            cluster.stop_cluster()
            time.sleep(2)
            success = cluster.start_cluster(args.nodes)
            sys.exit(0 if success else 1)
            
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
