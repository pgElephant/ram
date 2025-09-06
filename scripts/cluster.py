#!/usr/bin/python3

import os
import sys
import signal
import subprocess
import time
import requests
import yaml
from pathlib import Path
from typing import List, Optional

class ClusterConfig:
    def __init__(self):
        self.root_dir = Path(__file__).parent.parent.resolve()
        self.cluster_dir = Path("/tmp/ram_cluster")
        self.conf_dir = self.root_dir / "conf"
        self.pg_bin = Path("/usr/local/pgsql/bin")
        
        # Create default config files if they don't exist
        self.conf_dir.mkdir(parents=True, exist_ok=True)
        
        # Default primary node config
        primary_conf = {
            "node_id": 1,
            "node_name": "n1",
            "cluster_name": "ram_cluster",
            "cluster_id": 1,
            "postgresql_port": 5432,
            "http_api_port": 8432,
            "postgresql_host": "127.0.0.1",
            "postgresql_user": "postgres",
            "postgresql_database": "postgres",
            "streaming_replication_enabled": True,
            "synchronous_replication_enabled": False,
            "auto_failover_enabled": True,
            "rale_enabled": True,
            "log_level": "info"
        }
        
        # Default replica 1 config
        replica1_conf = {
            "node_id": 2,
            "node_name": "n2",
            "cluster_name": "ram_cluster", 
            "cluster_id": 1,
            "postgresql_port": 5433,
            "http_api_port": 8433,
            "postgresql_host": "127.0.0.1",
            "postgresql_user": "postgres",
            "postgresql_database": "postgres",
            "streaming_replication_enabled": True,
            "synchronous_replication_enabled": False,
            "auto_failover_enabled": True,
            "rale_enabled": True,
            "log_level": "info"
        }
        
        # Default replica 2 config
        replica2_conf = {
            "node_id": 3,
            "node_name": "n3",
            "cluster_name": "ram_cluster",
            "cluster_id": 1,
            "postgresql_port": 5434,
            "http_api_port": 8434,
            "postgresql_host": "127.0.0.1",
            "postgresql_user": "postgres",
            "postgresql_database": "postgres",
            "streaming_replication_enabled": True,
            "synchronous_replication_enabled": False,
            "auto_failover_enabled": True,
            "rale_enabled": True,
            "log_level": "info"
        }
        
        # Write default configs if they don't exist
        if not (self.conf_dir / "primary.yaml").exists():
            with open(self.conf_dir / "primary.yaml", "w") as f:
                yaml.dump(primary_conf, f)
                
        if not (self.conf_dir / "replica1.yaml").exists():
            with open(self.conf_dir / "replica1.yaml", "w") as f:
                yaml.dump(replica1_conf, f)
                
        if not (self.conf_dir / "replica2.yaml").exists():
            with open(self.conf_dir / "replica2.yaml", "w") as f:
                yaml.dump(replica2_conf, f)
                
        # Validate required PostgreSQL binaries exist
        self.pg_bin_req = ["pg_ctl", "initdb", "postgres", "createdb"]
        for binary in self.pg_bin_req:
            if not (self.pg_bin / binary).exists():
                print(f"Error: PostgreSQL binary '{binary}' not found in PG_BIN={self.pg_bin}", file=sys.stderr)
                sys.exit(1)
                
        self.cluster_dir.mkdir(parents=True, exist_ok=True)

        # Load node configurations
        self.node_configs = {}
        if self.conf_dir.exists():
            for conf_file in self.conf_dir.glob("*.yaml"):
                with open(conf_file) as f:
                    node_conf = yaml.safe_load(f)
                    if "node_id" in node_conf:
                        self.node_configs[node_conf["node_id"]] = node_conf

class Node:
    def __init__(self, node_id: int, config: ClusterConfig):
        self.id = node_id
        self.config = config
        
        # Get node configuration from yaml if available
        node_conf = config.node_configs.get(node_id, {})
        self.name = node_conf.get("node_name", f"n{node_id}")
        self.port = node_conf.get("postgresql_port")
        self.http_port = node_conf.get("http_api_port")
        
        self.pgdata = config.cluster_dir / self.name / "pgdata"
        self.logdir = config.cluster_dir / self.name / "logs"
        
    def write_ramd_conf(self, role: str, primary_port: Optional[int] = None):
        conf_dir = self.config.cluster_dir / self.name
        conf_dir.mkdir(parents=True, exist_ok=True)
        
        # Get base config from yaml if available
        node_conf = self.config.node_configs.get(self.id, {})
        
        conf = {
            "cluster_name": node_conf.get("cluster_name"),
            "cluster_id": node_conf.get("cluster_id"),
            "node_id": self.id,
            "node_name": self.name,
            "node_role": role,
            "bind_address": node_conf.get("bind_address"),
            "ramd_port": self.http_port,
            "http_api_port": self.http_port,
            "postgresql_host": node_conf.get("postgresql_host"),
            "postgresql_port": self.port,
            "postgresql_user": node_conf.get("postgresql_user"),
            "postgresql_password": node_conf.get("postgresql_password"),
            "postgresql_database": node_conf.get("postgresql_database"),
            "postgresql_data_dir": str(self.pgdata),
            "pid_file": str(conf_dir / "ramd.pid"),
            "streaming_replication_enabled": node_conf.get("streaming_replication_enabled"),
            "synchronous_replication_enabled": node_conf.get("synchronous_replication_enabled"),
            "num_sync_standbys": node_conf.get("num_sync_standbys"),
            "auto_failover_enabled": node_conf.get("auto_failover_enabled"),
            "failover_timeout": node_conf.get("failover_timeout"),
            "primary_failure_timeout": node_conf.get("primary_failure_timeout"),
            "rale_enabled": node_conf.get("rale_enabled"),
            "log_level": node_conf.get("log_level"),
            "log_to_console": node_conf.get("log_to_console")
        }

        if role == "standby" and primary_port:
            conf["primary_port"] = primary_port

        with open(conf_dir / "ramd.conf", "w") as f:
            for key, value in conf.items():
                if value is not None:
                    f.write(f"{key} = {value if isinstance(value, (int, bool)) else f'"{value}"'}\n")
            
    def create(self, role: str = "primary", primary_port: Optional[int] = None):
        self.pgdata.mkdir(parents=True, exist_ok=True)
        self.logdir.mkdir(parents=True, exist_ok=True)
        
        if not (self.pgdata / "PG_VERSION").exists():
            subprocess.run([self.config.pg_bin / "pg_ctl", "initdb", "-D", str(self.pgdata)], 
                         stdout=subprocess.DEVNULL)
            
        # Basic settings for cluster
        with open(self.pgdata / "postgresql.conf", "a") as f:
            f.write(f"port = {self.port}\n")
            node_conf = self.config.node_configs.get(self.id, {})
            f.write(f"listen_addresses = '{node_conf.get('postgresql_host', '127.0.0.1')}'\n")
            
        self.write_ramd_conf(role, primary_port)
            
    def start(self):
        subprocess.run([self.config.pg_bin / "pg_ctl", "-D", str(self.pgdata), 
                       "-l", str(self.logdir / "postgres.log"), "-w", "start"],
                       stdout=subprocess.DEVNULL)
        self.start_ramd()
                       
    def stop(self):
        self.stop_ramd()
        if (self.pgdata / "PG_VERSION").exists():
            try:
                subprocess.run([self.config.pg_bin / "pg_ctl", "-D", str(self.pgdata),
                              "-m", "fast", "-w", "stop"], 
                              stdout=subprocess.DEVNULL)
            except:
                pass
                
    def start_ramd(self):
        conf = self.config.cluster_dir / self.name / "ramd.conf"
        subprocess.run([self.config.root_dir / "ramd/ramd", "-c", str(conf), "-d"])
        
    def stop_ramd(self):
        try:
            subprocess.run([self.config.root_dir / "ramctrl/ramctrl", "stop"],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except:
            pass
            
    def destroy(self):
        self.stop()
        if self.pgdata.exists():
            subprocess.run(["rm", "-rf", str(self.pgdata)])
        if self.logdir.exists():
            subprocess.run(["rm", "-rf", str(self.logdir)])

class ClusterManager:
    def __init__(self):
        self.config = ClusterConfig()
        self.primary = None
        self.replicas = []
        
    def create_primary(self, node_id: int = 1):
        """Create a new primary node with the given ID"""
        if self.primary:
            print("Error: Primary node already exists")
            return
            
        node = Node(node_id, self.config)
        node.create(role="primary")
        self.primary = node
        print(f"Created primary node {node_id}")
        
    def create_replica(self, node_id: int):
        """Create a new replica node with the given ID"""
        if not self.primary:
            print("Error: No primary node exists. Create primary first")
            return
            
        node = Node(node_id, self.config)
        node.create(role="standby", primary_port=self.primary.port)
        self.replicas.append(node)
        print(f"Created replica node {node_id}")
        
    def start_primary(self):
        """Start the primary node if it exists"""
        if not self.primary:
            print("Error: No primary node exists")
            return
        self.primary.start()
        print("Started primary node")
        
    def stop_replicas(self):
        """Stop all replica nodes"""
        for replica in self.replicas:
            replica.stop()
        print("Stopped all replica nodes")
        
    def destroy_primary(self):
        """Destroy the primary node if it exists"""
        if self.primary:
            self.primary.destroy()
            self.primary = None
            print("Destroyed primary node")
            
    def destroy_replica(self, node_id: int):
        """Destroy a replica node with the given ID"""
        for replica in self.replicas:
            if replica.id == node_id:
                replica.destroy()
                self.replicas.remove(replica)
                print(f"Destroyed replica node {node_id}")
                return
        print(f"Error: No replica with ID {node_id} found")

if __name__ == "__main__":
    cluster = ClusterManager()
    print("RAM Cluster Management Tool")
    print("Available commands:")
    print("  cluster.create_primary(node_id)  - Create primary node")
    print("  cluster.create_replica(node_id)  - Create replica node")
    print("  cluster.start_primary()          - Start primary node")
    print("  cluster.stop_replicas()          - Stop all replicas")
    print("  cluster.destroy_replica(node_id) - Destroy specific replica")
    print("  cluster.destroy_primary()        - Destroy primary node")
