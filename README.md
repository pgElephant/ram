# RAM/RALE: PostgreSQL Auto‑Failover (ramd + ramctrl + pg_ram)

RAM/RALE is a complete, production‑grade PostgreSQL auto‑failover stack:

- ramd: local node daemon that monitors PostgreSQL, manages replication, and executes failover and recovery.
- ramctrl: CLI and controller client to operate clusters and the ramd daemon.
- pg_ram: PostgreSQL extension providing in‑database health, LSN, and metrics workers; integrates with RALE consensus.

See IMPLEMENTATION_SUMMARY.md for a detailed feature matrix and internals.

## Build

Prerequisites: a C toolchain, libpq, pthreads. The pg_ram extension builds when pg_config is available.

```
./configure [--with-pg-config=/path/to/pg_config]
make -j
```

Artifacts:
- ramd/ramd – daemon
- ramctrl/ramctrl – CLI
- pg_ram – extension sources, SQL and control files (builds .so if pg_config is found)

## Quickstart (3‑node cluster)

1) Prepare PostgreSQL on each node
- Ensure PostgreSQL is installed and running once to create a data dir, then stop it.
- Configure passwordless replication between nodes or provide credentials in ramd.conf.
- Optional: add pg_ram to shared_preload_libraries if building/using the extension.

2) Configure ramd on each node
- Copy ramd/conf/ramd.conf per node and adjust:
  - cluster_name, cluster_id
  - node_id, node_name, node_role (one primary, others standby)
  - postgresql_* settings (host, port, credentials, data dir)
  - replication and failover thresholds per your SLOs

3) Start the local daemon
```
ramd/ramd -c /path/to/ramd.conf -d
```

4) Operate with ramctrl
- Status: `ramctrl status`
- Show nodes: `ramctrl show nodes`
- Promote: `ramctrl promote <NODE_ID>`
- Trigger failover: `ramctrl failover`
- Start/Stop daemon: `ramctrl start|stop`

5) Use the HTTP API (default port 8008)
- GET /api/v1/cluster/status
- GET /api/v1/nodes
- GET /api/v1/nodes/{id}
- POST /api/v1/promote/{id}
- POST /api/v1/demote/{id}
- POST /api/v1/failover
- GET|POST /api/v1/replication/sync
- POST /api/v1/config/reload

6) pg_ram extension (optional)
- Build with PostgreSQL headers present: `./configure --with-pg-config=... && make`
- Install the extension artifacts to your PostgreSQL sharedir/libdir.
- postgresql.conf:
  - `shared_preload_libraries = 'pg_ram'`
  - configure pg_ram GUCs as needed (see pg_ram/README.md)
- Restart PostgreSQL and `CREATE EXTENSION pg_ram;` if SQL is needed.

## How Failover Works

- Health and replication monitored by ramd (and pg_ram if present)
- Quorum/consensus via RALE; split‑brain protections
- Candidate selection by most advanced LSN and health
- Promotion of standby, reconfigure sync replication, rebuild replicas

## Packaging and Service

- Run ramd under systemd or a supervisor; it writes a PID to ramd/ramd.pid.
- Expose the HTTP API behind TLS or a firewall (enable auth in ramd.conf).

## Notes

- If pg_config is absent, pg_ram is skipped at build time; ramd and ramctrl still provide full autofailover.
- See ramd/conf/ramd.conf for a comprehensive, self‑documented config.
