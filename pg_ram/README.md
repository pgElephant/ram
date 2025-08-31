# pg_ram extension

pg_ram is a PostgreSQL extension that integrates the RALE (librale) consensus
engine. It runs as two background workers:

- librale worker: owns consensus and DStore lifecycle
- health worker: collects node health; no consensus or elections

The SQL surface is minimal and read-only for cluster status and node info. No
client-side control plane, clusters, or FSM are implemented here.
