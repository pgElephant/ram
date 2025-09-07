-- pg_ram extension - minimal version
-- RALE consensus integration for PostgreSQL

CREATE SCHEMA IF NOT EXISTS pgram;

-- Health status function  
CREATE OR REPLACE FUNCTION pgram.health_status()
RETURNS text LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_health_status_sql$$;

-- Librale cluster status helpers
CREATE OR REPLACE FUNCTION pgram.librale_status()
RETURNS SETOF text LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_status_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_nodes()
RETURNS SETOF record LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_nodes_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_is_leader()
RETURNS boolean LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_is_leader_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_get_leader_id()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_leader_id_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_get_node_count()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_node_count_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_has_quorum()
RETURNS boolean LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_has_quorum_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_get_current_role()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_current_role_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_add_node(
    node_id integer,
    node_name text,
    node_ip text,
    rale_port integer,
    dstore_port integer
)
RETURNS boolean LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$pgram_librale_add_node_sql$$;

CREATE OR REPLACE FUNCTION pgram.librale_add_node(
    node_id integer,
    node_name text,
    node_ip text,
    rale_port integer,
    dstore_port integer
)
RETURNS boolean LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$pgram_librale_add_node_sql$$;

-- Minimal cluster status view for ramd integration
CREATE OR REPLACE VIEW pgram.cluster_status AS
SELECT 
    pgram.librale_get_node_count() AS node_count,
    pgram.librale_is_leader() AS is_leader,
    pgram.librale_get_leader_id() AS leader_id,
    pgram.librale_has_quorum() AS has_quorum,
    pgram.librale_get_current_role() AS current_role,
    pgram.health_status() AS health_status;
