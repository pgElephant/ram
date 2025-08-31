-- pg_ram--1.0.sql (minimal)
-- Copyright (c) 2024-2025, pgElephant, Inc.
-- Use CREATE EXTENSION pg_ram; to load this file.

CREATE SCHEMA IF NOT EXISTS pg_ram;

-- Health status helper
CREATE FUNCTION pg_ram.health_status()
RETURNS text LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_health_status_sql$$;

-- Librale cluster status helpers
CREATE FUNCTION pg_ram.librale_status()
RETURNS SETOF text LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_status_sql$$;

CREATE FUNCTION pg_ram.librale_nodes()
RETURNS SETOF record LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_nodes_sql$$;

CREATE FUNCTION pg_ram.librale_is_leader()
RETURNS boolean LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_is_leader_sql$$;

CREATE FUNCTION pg_ram.librale_get_leader_id()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_leader_id_sql$$;

CREATE FUNCTION pg_ram.librale_get_node_count()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_node_count_sql$$;

CREATE FUNCTION pg_ram.librale_has_quorum()
RETURNS boolean LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_has_quorum_sql$$;

CREATE FUNCTION pg_ram.librale_get_current_role()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_current_role_sql$$;

-- Convenience view for quick cluster status glance
CREATE OR REPLACE VIEW pg_ram.cluster_status AS
SELECT
  pg_ram.librale_is_leader()  AS is_leader,
  pg_ram.librale_get_leader_id() AS leader_id,
  pg_ram.librale_get_node_count() AS node_count,
  pg_ram.librale_has_quorum() AS has_quorum,
  pg_ram.librale_get_current_role() AS current_role,
  pg_ram.health_status() AS health_summary;

-- Nodes view with typed columns for convenience
CREATE OR REPLACE VIEW pg_ram.nodes AS
SELECT *
FROM pg_ram.librale_nodes() AS n(
   node_id int,
   name text,
   ip text,
   rale_port int,
   dstore_port int,
   state int,
   status int,
   is_connected boolean,
   is_leader boolean
);

-- Note: control-plane (write) functions are intentionally not exposed here.
