#!/usr/bin/env bash
set -euo pipefail

# Local 3-node PostgreSQL + ramd demo cluster
# - Initializes 3 PG instances on ports 54331,54332,54333
# - Starts 3 ramd daemons bound to those instances with rale disabled
# - Shows cluster status via ramctrl and HTTP

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEMO_DIR="${DEMO_DIR:-/tmp/ram_demo}"
PG_BIN="${PG_BIN:-$(command -v pg_ctl >/dev/null 2>&1 && dirname "$(command -v pg_ctl)" || echo /usr/local/pgsql/bin)}"

pg_bin_req=(pg_ctl initdb postgres createdb)
for b in "${pg_bin_req[@]}"; do
  if ! command -v "$PG_BIN/$b" >/dev/null 2>&1; then
    echo "Error: PostgreSQL binary '$b' not found in PG_BIN=$PG_BIN" >&2
    exit 1
  fi
done

mkdir -p "$DEMO_DIR"

node() {
  local id=$1; shift
  echo "n${id}"
}

pgdata() {
  local id=$1; shift
  echo "$DEMO_DIR/$(node "$id")/pgdata"
}

logdir() {
  local id=$1; shift
  echo "$DEMO_DIR/$(node "$id")/logs"
}

port() { echo $((54330 + $1)); }
http_port() { echo $((8007 + $1)); }

write_ramd_conf() {
  local id=$1
  local role=$2
  local pg_port; pg_port=$(port "$id")
  local http; http=$(http_port "$id")
  local data; data=$(pgdata "$id")
  local conf_dir="$DEMO_DIR/$(node "$id")"
  mkdir -p "$conf_dir"
  cat >"$conf_dir/ramd.conf" <<EOF
cluster_name = "ram_demo_cluster"
cluster_id = 1
node_id = $id
node_name = "$(node "$id")"
node_role = "$role"
bind_address = "127.0.0.1"
ramd_port = $http
http_api_port = $http
postgresql_host = "127.0.0.1"
postgresql_port = $pg_port
postgresql_user = "postgres"
postgresql_password = ""
postgresql_database = "postgres"
postgresql_data_dir = "$data"
pid_file = "$conf_dir/ramd.pid"
streaming_replication_enabled = true
synchronous_replication_enabled = true
num_sync_standbys = 1
auto_failover_enabled = true
failover_timeout = 15
primary_failure_timeout = 5
rale_enabled = false
log_level = "INFO"
log_to_console = true
EOF
}

start_pg() {
  local id=$1
  local data; data=$(pgdata "$id")
  local logs; logs=$(logdir "$id")
  local pg_port; pg_port=$(port "$id")
  mkdir -p "$data" "$logs"
  if [ ! -s "$data/PG_VERSION" ]; then
    "$PG_BIN/pg_ctl" initdb -D "$data" >/dev/null
  fi
  # Basic settings for demo
  { echo "port = $pg_port"; echo "listen_addresses = '127.0.0.1'"; } >>"$data/postgresql.conf"
  "$PG_BIN/pg_ctl" -D "$data" -l "$logs/postgres.log" -w start >/dev/null
}

stop_pg() {
  local id=$1
  local data; data=$(pgdata "$id")
  if [ -s "$data/PG_VERSION" ]; then
    "$PG_BIN/pg_ctl" -D "$data" -m fast -w stop >/dev/null || true
  fi
}

start_ramd() {
  local id=$1
  local conf="$DEMO_DIR/$(node "$id")/ramd.conf"
  "$ROOT_DIR/ramd/ramd" -c "$conf" -d
}

stop_ramd() {
  # Uses PID file location embedded in ramd; best-effort via ramctrl if available
  "$ROOT_DIR/ramctrl/ramctrl" stop >/dev/null 2>&1 || true
}

cleanup() {
  echo "Stopping ramd and Postgres..."
  stop_ramd || true
  for i in 1 2 3; do stop_pg "$i" || true; done
}

trap cleanup EXIT

echo "Starting 3 local Postgres nodes..."
for i in 1 2 3; do start_pg "$i"; done

echo "Writing ramd configs..."
write_ramd_conf 1 primary
write_ramd_conf 2 standby
write_ramd_conf 3 standby

echo "Starting ramd daemons..."
for i in 1 2 3; do start_ramd "$i"; done

sleep 2

echo "Cluster status via HTTP:"
curl -s http://127.0.0.1/ --max-time 1 >/dev/null 2>&1 || true
for i in 1 2 3; do
  curl -s "http://127.0.0.1:$(http_port "$i")/api/v1/cluster/status" || true
  echo
done

echo "ramctrl status:"
"$ROOT_DIR/ramctrl/ramctrl" status || true

echo "Demo is running. Press Ctrl-C to stop. Logs in $DEMO_DIR/*/logs"
sleep infinity
