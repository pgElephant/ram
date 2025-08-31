-- pg_ram--1.0.sql (minimal install)
-- Copyright (c) 2024-2025, pgElephant, Inc.
-- This file intentionally exposes a tiny, read-only SQL surface.
-- No cluster/node state machine or replication management is defined here.

CREATE SCHEMA IF NOT EXISTS pg_ram;

-- Health status helper
CREATE OR REPLACE FUNCTION pg_ram.health_status()
RETURNS text LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_health_status_sql$$;

-- Librale cluster status helpers
CREATE OR REPLACE FUNCTION pg_ram.librale_status()
RETURNS SETOF text LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_status_sql$$;

CREATE OR REPLACE FUNCTION pg_ram.librale_nodes()
RETURNS SETOF record LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_nodes_sql$$;

CREATE OR REPLACE FUNCTION pg_ram.librale_is_leader()
RETURNS boolean LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_is_leader_sql$$;

CREATE OR REPLACE FUNCTION pg_ram.librale_get_leader_id()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_leader_id_sql$$;

CREATE OR REPLACE FUNCTION pg_ram.librale_get_node_count()
RETURNS int LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_get_node_count_sql$$;

CREATE OR REPLACE FUNCTION pg_ram.librale_has_quorum()
RETURNS boolean LANGUAGE C STABLE
AS 'MODULE_PATHNAME', $$pgram_librale_has_quorum_sql$$;

CREATE OR REPLACE FUNCTION pg_ram.librale_get_current_role()
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

-- Note: write/control-plane functions intentionally omitted for minimal install.

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

/*

CREATE TYPE pg_ram.replication_state
    AS ENUM
 (
    'unknown',
    'init',
    'single',
    'wait_primary',
    'primary',
    'draining',
    'demote_timeout',
    'demoted',
    'catchingup',
    'secondary',
    'prepare_promotion',
    'stop_replication',
    'wait_standby',
    'maintenance',
    'join_primary',
    'apply_settings',
    'prepare_maintenance',
    'wait_maintenance',
    'report_lsn',
    'fast_forward',
    'join_secondary',
    'dropped'
 );

CREATE TABLE pg_ram.cluster
 (
    clusterid          text NOT NULL DEFAULT 'default',
    kind                 text NOT NULL DEFAULT 'pgsql',
    dbname               name NOT NULL DEFAULT 'postgres',
    opt_secondary        bool NOT NULL DEFAULT true,
    number_sync_standbys int  NOT NULL DEFAULT 0,

    PRIMARY KEY   (clusterid),
    CHECK (kind IN ('pgsql'))
 );
insert into pg_ram.cluster (clusterid) values ('default');

CREATE FUNCTION pg_ram.create_cluster
 (
    IN cluster_id         text,
    IN kind                 text,
    IN dbname               name,
    IN opt_secondary        bool,
    IN number_sync_standbys int,
   OUT cluster_id         text,
   OUT kind                 text,
   OUT dbname               name,
   OUT opt_secondary        bool,
   OUT number_sync_standbys int
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$create_cluster$$;

grant execute on function
      pg_ram.create_cluster(text,text,name,bool,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.drop_cluster
 (
    IN cluster_id  text
 )
RETURNS void LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$drop_cluster$$;

grant execute on function pg_ram.drop_cluster(text) to autoctl_node;

CREATE FUNCTION pg_ram.set_cluster_number_sync_standbys
 (
    IN cluster_id         text,
    IN number_sync_standbys int
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_cluster_number_sync_standbys$$;

grant execute on function
      pg_ram.set_cluster_number_sync_standbys(text, int)
   to autoctl_node;

CREATE TABLE pg_ram.node
 (
    clusterid          text not null default 'default',
    nodeid               bigserial,
    groupid              int not null,
    nodename             text not null,
    nodehost             text not null,
    nodeport             int not null,
    sysidentifier        bigint,
    goalstate            pg_ram.replication_state not null default 'init',
    reportedstate        pg_ram.replication_state not null,
    reportedpgisrunning  bool default true,
    reportedrepstate     text default 'async',
    reporttime           timestamptz not null default now(),
    reportedtli          int not null default 1 check (reportedtli > 0),
    reportedlsn          pg_lsn not null default '0/0',
    walreporttime        timestamptz not null default now(),
    health               integer not null default -1,
    healthchecktime      timestamptz not null default now(),
    statechangetime      timestamptz not null default now(),
    candidatepriority	 int not null default 100,
    replicationquorum	 bool not null default true,
    nodecluster          text not null default 'default',

   -- node names must be unique in a given cluster
    UNIQUE (clusterid, nodename),
    -- any nodehost:port can only be a unique node in the system
    UNIQUE (nodehost, nodeport),
    --
    -- The EXCLUDE constraint only allows the same sysidentifier for all the
    -- nodes in the same group. The system_identifier is a property that is
    -- kept when implementing streaming replication and should be unique per
    -- Postgres instance in all other cases.
    --
    -- We allow the sysidentifier column to be NULL when registering a new
    -- primary server from scratch, because we have not done pg_ctl initdb
    -- at the time we call the register_node() function.
    --
    CONSTRAINT system_identifier_is_null_at_init_only
         CHECK (
                  (
                       sysidentifier IS NULL
                   AND reportedstate
                       IN (
                           'init',
                           'wait_standby',
                           'catchingup',
                           'dropped'
                          )
                   )
                OR sysidentifier IS NOT NULL
               ),

    CONSTRAINT same_system_identifier_within_group
       EXCLUDE USING gist(clusterid with =,
                          groupid with =,
                          sysidentifier with <>)
    DEFERRABLE INITIALLY DEFERRED,

    PRIMARY KEY (nodeid),
    FOREIGN KEY (clusterid) REFERENCES pg_ram.cluster(clusterid)
 )
 -- we expect few rows and lots of UPDATE, let's benefit from HOT
 WITH (fillfactor = 25);

CREATE TABLE pg_ram.event
 (
    eventid           bigserial not null,
    eventtime         timestamptz not null default now(),
    clusterid       text not null,
    nodeid            bigint not null,
    groupid           int not null,
    nodename          text not null,
    nodehost          text not null,
    nodeport          integer not null,
    reportedstate     pg_ram.replication_state not null,
    goalstate         pg_ram.replication_state not null,
    reportedrepstate  text,
    reportedtli       int not null default 1 check (reportedtli > 0),
    reportedlsn       pg_lsn not null default '0/0',
    candidatepriority int,
    replicationquorum bool,
    description       text,

    PRIMARY KEY (eventid)
 );

GRANT SELECT ON ALL TABLES IN SCHEMA pg_ram TO autoctl_node;

CREATE FUNCTION pg_ram.set_node_system_identifier
 (
    IN node_id             bigint,
    IN node_sysidentifier  bigint,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int
 )
RETURNS record LANGUAGE SQL STRICT SECURITY DEFINER
AS $$
      update pg_ram.node
         set sysidentifier = node_sysidentifier
       where nodeid = set_node_system_identifier.node_id
   returning nodeid, nodename, nodehost, nodeport;
$$;

grant execute on function pg_ram.set_node_system_identifier(bigint,bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.set_group_system_identifier
 (
    IN group_id            bigint,
    IN node_sysidentifier  bigint,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int
 )
RETURNS setof record LANGUAGE SQL STRICT SECURITY DEFINER
AS $$
      update pg_ram.node
         set sysidentifier = node_sysidentifier
       where groupid = set_group_system_identifier.group_id
         and sysidentifier = 0
   returning nodeid, nodename, nodehost, nodeport;
$$;

grant execute on function pg_ram.set_group_system_identifier(bigint,bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.update_node_metadata
  (
     IN node_id   bigint,
     IN node_name text,
     IN node_host text,
     IN node_port int
  )
 RETURNS boolean LANGUAGE C SECURITY DEFINER
 AS 'MODULE_PATHNAME', $$update_node_metadata$$;

grant execute on function pg_ram.update_node_metadata(bigint,text,text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.register_node
 (
    IN cluster_id         text,
    IN node_host            text,
    IN node_port            int,
    IN dbname               name,
    IN node_name            text default '',
    IN sysidentifier        bigint default 0,
    IN desired_node_id      bigint default -1,
    IN desired_group_id     int default -1,
    IN initial_group_role   pg_ram.replication_state default 'init',
    IN node_kind            text default 'standalone',
    IN candidate_priority 	int default 100,
    IN replication_quorum	bool default true,
    IN node_cluster         text default 'default',
   OUT assigned_node_id     bigint,
   OUT assigned_group_id    int,
   OUT assigned_group_state pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool,
   OUT assigned_node_name   text
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$register_node$$;

grant execute on function
      pg_ram.register_node(text,text,int,name,text,bigint,bigint,int,
                                   pg_ram.replication_state,text,
                                   int,bool,text)
   to autoctl_node;


CREATE FUNCTION pg_ram.node_active
 (
    IN cluster_id           		text,
    IN node_id        		        bigint,
    IN group_id       		        int,
    IN current_group_role     		pg_ram.replication_state default 'init',
    IN current_pg_is_running  		bool default true,
    IN current_tli			  		integer default 1,
    IN current_lsn			  		pg_lsn default '0/0',
    IN current_rep_state      		text default '',
   OUT assigned_node_id       		bigint,
   OUT assigned_group_id      		int,
   OUT assigned_group_state   		pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$node_active$$;

grant execute on function
      pg_ram.node_active(text,bigint,int,
                          pg_ram.replication_state,bool,int,pg_lsn,text)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_nodes
 (
    IN cluster_id     text default 'default',
    IN group_id         int default NULL,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C
AS 'MODULE_PATHNAME', $$get_nodes$$;

comment on function pg_ram.get_nodes(text,int)
        is 'get all the nodes in a group';

grant execute on function pg_ram.get_nodes(text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_primary
 (
    IN cluster_id      text default 'default',
    IN group_id          int default 0,
   OUT primary_node_id   bigint,
   OUT primary_name      text,
   OUT primary_host      text,
   OUT primary_port      int
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$get_primary$$;

comment on function pg_ram.get_primary(text,int)
        is 'get the writable node for a group';

grant execute on function pg_ram.get_primary(text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN nodeid           bigint,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes(bigint)
        is 'get the other nodes in a group';

grant execute on function pg_ram.get_other_nodes(bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN nodeid           bigint,
    IN current_state    pg_ram.replication_state,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes
                    (bigint,pg_ram.replication_state)
        is 'get the other nodes in a group, filtering on current_state';

grant execute on function pg_ram.get_other_nodes
                          (bigint,pg_ram.replication_state)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_coordinator
 (
    IN cluster_id  text default 'default',
   OUT node_host     text,
   OUT node_port     int
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
  select nodehost, nodeport
    from pg_ram.node
         join pg_ram.cluster using(clusterid)
   where clusterid = cluster_id
     and groupid = 0
     and goalstate in ('single', 'wait_primary', 'primary')
     and reportedstate in ('single', 'wait_primary', 'primary');
$$;

grant execute on function pg_ram.get_coordinator(text)
   to autoctl_node;


CREATE FUNCTION pg_ram.get_most_advanced_standby
 (
   IN clusterid       text default 'default',
   IN groupid           int default 0,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select nodeid, nodename, nodehost, nodeport, reportedlsn, false
     from pg_ram.node
    where clusterid = $1
      and groupid = $2
      and reportedstate = 'report_lsn'
 order by reportedlsn desc, health desc
    limit 1;
$$;

grant execute on function pg_ram.get_most_advanced_standby(text,int)
   to autoctl_node;


CREATE FUNCTION pg_ram.remove_node
 (
   node_id bigint,
   force   bool default 'false'
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$remove_node_by_nodeid$$;

comment on function pg_ram.remove_node(bigint,bool)
        is 'remove a node from the monitor';

grant execute on function pg_ram.remove_node(bigint,bool)
   to autoctl_node;

CREATE FUNCTION pg_ram.remove_node
 (
   node_host text,
   node_port int default 5432,
   force     bool default 'false'
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$remove_node_by_host$$;

comment on function pg_ram.remove_node(text,int,bool)
        is 'remove a node from the monitor';

grant execute on function pg_ram.remove_node(text,int,bool)
   to autoctl_node;

CREATE FUNCTION pg_ram.perform_failover
 (
  cluster_id text default 'default',
  group_id     int  default 0
 )
RETURNS void LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$perform_failover$$;

comment on function pg_ram.perform_failover(text,int)
        is 'manually failover from the primary to the secondary';

grant execute on function pg_ram.perform_failover(text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.perform_promotion
 (
  cluster_id text,
  node_name    text
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$perform_promotion$$;

comment on function pg_ram.perform_promotion(text,text)
        is 'manually failover from the primary to the given node';

grant execute on function pg_ram.perform_promotion(text,text)
   to autoctl_node;

CREATE FUNCTION pg_ram.start_maintenance(node_id bigint)
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$start_maintenance$$;

comment on function pg_ram.start_maintenance(bigint)
        is 'set a node in maintenance state';

grant execute on function pg_ram.start_maintenance(bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.stop_maintenance(node_id bigint)
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$stop_maintenance$$;

comment on function pg_ram.stop_maintenance(bigint)
        is 'set a node out of maintenance state';

grant execute on function pg_ram.stop_maintenance(bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.last_events
 (
  count int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
  select eventid, eventtime, clusterid,
         nodeid, groupid, nodename, nodehost, nodeport,
         reportedstate, goalstate,
         reportedrepstate, reportedtli, reportedlsn,
         candidatepriority, replicationquorum, description
    from pg_ram.event
order by eventid desc
   limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(int)
        is 'retrieve last COUNT events';

grant execute on function pg_ram.last_events(int)
   to autoctl_node;

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text default 'default',
  count        int  default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodehost, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedtli, reportedlsn,
           candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int)
        is 'retrieve last COUNT events for given cluster';

grant execute on function pg_ram.last_events(text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text,
  group_id     int,
  count        int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodehost, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedtli, reportedlsn,
           candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
       and groupid = group_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int,int)
        is 'retrieve last COUNT events for given cluster and group';

grant execute on function pg_ram.last_events(text,int,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text default 'default',
   OUT cluster_kind       text,
   OUT nodename             text,
   OUT nodehost             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool,
   OUT reported_tli         int,
   OUT reported_lsn         pg_lsn,
   OUT health               integer,
   OUT nodecluster          text
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select kind, nodename, nodehost, nodeport, groupid, nodeid,
          reportedstate, goalstate,
   		  candidatepriority, replicationquorum,
          reportedtli, reportedlsn, health, nodecluster
     from pg_ram.node
     join pg_ram.cluster using(clusterid)
    where clusterid = cluster_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text)
        is 'get the current state of both nodes of a cluster';

grant execute on function pg_ram.current_state(text)
   to autoctl_node;

CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text,
    IN group_id             int,
   OUT cluster_kind       text,
   OUT nodename             text,
   OUT nodehost             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool,
   OUT reported_tli         int,
   OUT reported_lsn         pg_lsn,
   OUT health               integer,
   OUT nodecluster          text
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select kind, nodename, nodehost, nodeport, groupid, nodeid,
          reportedstate, goalstate,
   		  candidatepriority, replicationquorum,
          reportedtli, reportedlsn, health, nodecluster
     from pg_ram.node
     join pg_ram.cluster using(clusterid)
    where clusterid = cluster_id
      and groupid = group_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text, int)
        is 'get the current state of both nodes of a group in a cluster';

grant execute on function pg_ram.current_state(text, int)
   to autoctl_node;


CREATE FUNCTION pg_ram.cluster_uri
 (
    IN cluster_id         text DEFAULT 'default',
    IN cluster_name         text DEFAULT 'default',
    IN sslmode              text DEFAULT 'prefer',
    IN sslrootcert          text DEFAULT '',
    IN sslcrl               text DEFAULT ''
 )
RETURNS text LANGUAGE SQL STRICT
AS $$
    select case
           when string_agg(format('%s:%s', nodehost, nodeport),',') is not null
           then format(
               'postgres://%s/%s?%ssslmode=%s%s%s',
               string_agg(format('%s:%s', nodehost, nodeport),','),
               -- as we join cluster on node we get the same dbname for all
               -- entries, pick one.
               min(dbname),
               case when cluster_name = 'default'
                    then 'target_session_attrs=read-write&'
                    else ''
               end,
               min(sslmode),
               CASE WHEN min(sslrootcert) = ''
                   THEN ''
                   ELSE '&sslrootcert=' || sslrootcert
               END,
               CASE WHEN min(sslcrl) = ''
                   THEN ''
                   ELSE '&sslcrl=' || sslcrl
               END
           )
           end as uri
      from pg_ram.node as node
           join pg_ram.cluster using(clusterid)
     where clusterid = cluster_id
       and groupid = 0
       and nodecluster = cluster_name;
$$;

CREATE FUNCTION pg_ram.enable_secondary
 (
   cluster_id text
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$enable_secondary$$;

comment on function pg_ram.enable_secondary(text)
        is 'changes the state of a cluster to assign secondaries for nodes when added';

CREATE FUNCTION pg_ram.disable_secondary
 (
   cluster_id text
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$disable_secondary$$;

comment on function pg_ram.disable_secondary(text)
        is 'changes the state of a cluster to disable the assignment of secondaries for nodes when added';


CREATE OR REPLACE FUNCTION pg_ram.update_secondary_check()
  RETURNS trigger
  LANGUAGE 'plpgsql'
AS $$
declare
  nodeid        bigint := null;
  reportedstate pg_ram.replication_state := null;
begin
	-- when secondary changes from true to false, check all nodes remaining are primary
	if     new.opt_secondary is false
	   and new.opt_secondary is distinct from old.opt_secondary
	then
		select node.nodeid, node.reportedstate
		  into nodeid, reportedstate
		  from pg_ram.node
		 where node.clusterid = new.clusterid
		   and node.reportedstate <> 'single'
           and node.goalstate <> 'dropped';

		if nodeid is not null
		then
		    raise exception object_not_in_prerequisite_state
		      using
		        message = 'cluster has nodes that are not in SINGLE state',
		         detail = 'nodeid ' || nodeid || ' is in state ' || reportedstate,
		           hint = 'drop secondary nodes before disabling secondaries on cluster';
		end if;
	end if;

    return new;
end
$$;

comment on function pg_ram.update_secondary_check()
        is 'performs a check when changes to hassecondary on pg_ram.cluster are made, verifying cluster state allows the change';

CREATE TRIGGER disable_secondary_check
	BEFORE UPDATE
	ON pg_ram.cluster
	FOR EACH ROW
	EXECUTE PROCEDURE pg_ram.update_secondary_check();


CREATE FUNCTION pg_ram.set_node_candidate_priority
 (
    IN cluster_id         text,
    IN node_name            text,
    IN candidate_priority	int
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_node_candidate_priority$$;

comment on function pg_ram.set_node_candidate_priority(text, text, int)
        is 'sets the candidate priority value for a node. Expects a priority value between 0 and 100. 0 if the node is not a candidate to be promoted to be primary.';

grant execute on function
      pg_ram.set_node_candidate_priority(text, text, int)
   to autoctl_node;

CREATE FUNCTION pg_ram.set_node_replication_quorum
 (
    IN cluster_id       text,
    IN node_name          text,
    IN replication_quorum bool
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_node_replication_quorum$$;

comment on function pg_ram.set_node_replication_quorum(text, text, bool)
        is 'sets the replication quorum value for a node. true if the node participates in write quorum';

grant execute on function
      pg_ram.set_node_replication_quorum(text, text, bool)
   to autoctl_node;


create function pg_ram.synchronous_standby_names
 (
    IN cluster_id text default 'default',
    IN group_id     int default 0
 )
returns text language C strict
AS 'MODULE_PATHNAME', $$synchronous_standby_names$$;

comment on function pg_ram.synchronous_standby_names(text, int)
        is 'get the synchronous_standby_names setting for a given group';

grant execute on function
      pg_ram.synchronous_standby_names(text, int)
   to autoctl_node;


CREATE FUNCTION pg_ram.cluster_settings
 (
    IN cluster_id         text default 'default',
   OUT context              text,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT nodename             text,
   OUT setting              text,
   OUT value                text
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
  with groups(clusterid, groupid) as
  (
     select clusterid, groupid
       from pg_ram.node
      where clusterid = cluster_id
   group by clusterid, groupid
  )

  -- context: cluster, number_sync_standbys
  select 'cluster' as context,
         NULL as group_id, NULL as node_id, clusterid as nodename,
         'number_sync_standbys' as setting,
         cast(number_sync_standbys as text) as value
    from pg_ram.cluster
   where clusterid = cluster_id

union all

  -- context: primary, one entry per group in the cluster
  select 'primary', groups.groupid, nodes.node_id, nodes.node_name,
         'synchronous_standby_names',
         format('''%s''',
         pg_ram.synchronous_standby_names(clusterid, groupid))
    from groups, pg_ram.get_nodes(clusterid, groupid) as nodes
   where node_is_primary

union all

(
  -- context: node, one entry per node in the cluster
  select 'node', node.groupid, node.nodeid, node.nodename,
         'replication quorum', cast(node.replicationquorum as text)
    from pg_ram.node as node
   where node.clusterid = cluster_id
order by nodeid
)

union all

(
  select 'node', node.groupid, node.nodeid, node.nodename,
         'candidate priority', cast(node.candidatepriority as text)
    from pg_ram.node as node
   where node.clusterid = cluster_id
order by nodeid
)
$$;

comment on function pg_ram.cluster_settings(text)
        is 'get the current replication settings a cluster';
--
-- extension update file from 1.0 to 1.1
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION pg_ram UPDATE TO 1.1" to load this file. \quit

ALTER TABLE pg_ram.node
	RENAME TO node_upgrade_old;

CREATE TABLE pg_ram.node
 (
    clusterid          text not null default 'default',
    nodeid               bigint not null DEFAULT nextval('pg_ram.node_nodeid_seq'::regclass),
    groupid              int not null,
    nodename             text not null,
    nodeport             integer not null,
    goalstate            pg_ram.replication_state not null default 'init',
    reportedstate        pg_ram.replication_state not null,
    reportedpgisrunning  bool default true,
    reportedrepstate     text default 'async',
    reporttime           timestamptz not null default now(),
    reportedlsn          pg_lsn not null default '0/0',
    walreporttime        timestamptz not null default now(),
    health               integer not null default -1,
    healthchecktime      timestamptz not null default now(),
    statechangetime      timestamptz not null default now(),

    UNIQUE (nodename, nodeport),
    PRIMARY KEY (nodeid),
    FOREIGN KEY (clusterid) REFERENCES pg_ram.cluster(clusterid)
 )
 -- we expect few rows and lots of UPDATE, let's benefit from HOT
 WITH (fillfactor = 25);

ALTER SEQUENCE pg_ram.node_nodeid_seq OWNED BY pg_ram.node.nodeid;

INSERT INTO pg_ram.node (clusterid, nodeid, groupid, nodename, nodeport, goalstate, reportedstate,
		reportedpgisrunning, reportedrepstate, reporttime, walreporttime, health, healthchecktime, statechangetime)
	SELECT clusterid, nodeid, groupid, nodename, nodeport, goalstate, reportedstate,
		reportedpgisrunning, reportedrepstate, reporttime, walreporttime, health, healthchecktime, statechangetime
	FROM pg_ram.node_upgrade_old;

ALTER TABLE pg_ram.event
	RENAME TO event_upgrade_old;

ALTER TABLE pg_ram.event_upgrade_old	
	ALTER COLUMN nodeid DROP NOT NULL,
    ALTER COLUMN nodeid SET DEFAULT NULL;

DROP SEQUENCE pg_ram.event_nodeid_seq;

CREATE TABLE pg_ram.event
 (
    eventid          bigint not null DEFAULT nextval('pg_ram.event_eventid_seq'::regclass),
    eventtime        timestamptz not null default now(),
    clusterid      text not null,
    nodeid           bigint not null,
    groupid          int not null,
    nodename         text not null,
    nodeport         integer not null,
    reportedstate    pg_ram.replication_state not null,
    goalstate        pg_ram.replication_state not null,
    reportedrepstate text,
    reportedlsn      pg_lsn not null default '0/0',
    description      text,

    PRIMARY KEY (eventid)
 );

ALTER SEQUENCE pg_ram.event_eventid_seq OWNED BY pg_ram.event.eventid;

INSERT INTO pg_ram.event
		(eventid, eventtime, clusterid, nodeid, groupid, nodename, nodeport, reportedstate, goalstate, reportedrepstate, description)
	SELECT eventid, eventtime, clusterid, nodeid, groupid, nodename, nodeport, reportedstate, goalstate, reportedrepstate, description
	FROM pg_ram.event_upgrade_old;


GRANT SELECT ON ALL TABLES IN SCHEMA pg_ram TO autoctl_node;

DROP FUNCTION pg_ram.node_active(text,text,int,int,int, pg_ram.replication_state,bool,bigint,text);

CREATE FUNCTION pg_ram.node_active
 (
    IN cluster_id                 text,
    IN node_name                    text,
    IN node_port                    int,
    IN current_node_id              int default -1,
    IN current_group_id             int default -1,
    IN current_group_role           pg_ram.replication_state default 'init',
    IN current_pg_is_running        bool default true,
    IN current_lsn                  pg_lsn default '0/0',
    IN current_rep_state            text default '',
   OUT assigned_node_id             int,
   OUT assigned_group_id            int,
   OUT assigned_group_state         pg_ram.replication_state
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$node_active$$;

grant execute on function
      pg_ram.node_active(text,text,int,int,int,
                          pg_ram.replication_state,bool,pg_lsn,text)
   to autoctl_node;


grant execute on function pg_ram.remove_node(text,int)
   to autoctl_node;


ALTER FUNCTION pg_ram.perform_failover(text,int)
      SECURITY DEFINER;
 
grant execute on function pg_ram.perform_failover(text,int)
   to autoctl_node;

grant execute on function pg_ram.start_maintenance(text,int)
   to autoctl_node;

grant execute on function pg_ram.stop_maintenance(text,int)
   to autoctl_node;


DROP FUNCTION pg_ram.last_events(integer);

CREATE OR REPLACE FUNCTION pg_ram.last_events
 (
  count int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
  select eventid, eventtime, clusterid,
         nodeid, groupid, nodename, nodeport,
         reportedstate, goalstate,
         reportedrepstate, reportedlsn, description
    from pg_ram.event
order by eventid desc
   limit count
)
select * from last_events order by eventtime, eventid;
$$;

DROP FUNCTION pg_ram.last_events(text, integer);

CREATE OR REPLACE FUNCTION pg_ram.last_events
 (
  cluster_id text default 'default',
  count        int  default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedlsn, description
      from pg_ram.event
     where clusterid = cluster_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;


DROP FUNCTION pg_ram.last_events(text, integer, integer);

CREATE OR REPLACE FUNCTION pg_ram.last_events
 (
  cluster_id text,
  group_id     int,
  count        int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedlsn, description
      from pg_ram.event
     where clusterid = cluster_id
       and groupid = group_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

DROP TABLE pg_ram.node_upgrade_old;
DROP TABLE pg_ram.event_upgrade_old;
--
-- extension update file from 1.1 to 1.2
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "ALTER EXTENSION pg_ram UPDATE TO 1.2" to load this file. \quit


DROP FUNCTION IF EXISTS pg_ram.cluster_uri(text);

CREATE FUNCTION pg_ram.cluster_uri
 (
    IN cluster_id         text DEFAULT 'default',
    IN sslmode              text DEFAULT 'prefer'
 )
RETURNS text LANGUAGE SQL STRICT
AS $$
    select case
           when string_agg(format('%s:%s', nodename, nodeport),',') is not null
           then format('postgres://%s/%s?target_session_attrs=read-write&sslmode=%s',
                       string_agg(format('%s:%s', nodename, nodeport),','),
                       -- as we join cluster on node we get the same dbname for all
                       -- entries, pick one.
                       min(dbname),
                       min(sslmode)
                      )
           end as uri
      from pg_ram.node as node
           join pg_ram.cluster using(clusterid)
     where clusterid = cluster_id
       and groupid = 0;
$$;
--
-- extension update file from 1.2 to 1.3
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
-- \echo Use "ALTER EXTENSION pg_ram UPDATE TO 1.3" to load this file. \quit

--- The following only works in Postgres 12 onward
-- ALTER TYPE pg_ram.replication_state ADD VALUE 'join_primary';
-- ALTER TYPE pg_ram.replication_state ADD VALUE 'apply_settings';


DROP FUNCTION IF EXISTS pg_ram.register_node(text,text,integer,name,integer,pg_ram.replication_state,text);

DROP FUNCTION IF EXISTS pg_ram.node_active(text,text,int,int,int,
                          pg_ram.replication_state,bool,pg_lsn,text);

DROP FUNCTION IF EXISTS pg_ram.current_state(text);

DROP FUNCTION IF EXISTS pg_ram.current_state(text, int);

ALTER TYPE pg_ram.replication_state RENAME TO old_replication_state;

CREATE TYPE pg_ram.replication_state
    AS ENUM
 (
    'unknown',
    'init',
    'single',
    'wait_primary',
    'primary',
    'draining',
    'demote_timeout',
    'demoted',
    'catchingup',
    'secondary',
    'prepare_promotion',
    'stop_replication',
    'wait_standby',
    'maintenance',
    'join_primary',
    'apply_settings'
 );

-- Note the double cast here, first to text and only then to the new enums
ALTER TABLE pg_ram.node
      ALTER COLUMN goalstate DROP NOT NULL,
      ALTER COLUMN goalstate DROP DEFAULT,

      ALTER COLUMN goalstate
              TYPE pg_ram.replication_state
             USING goalstate::text::pg_ram.replication_state,

      ALTER COLUMN goalstate SET DEFAULT 'init',
      ALTER COLUMN goalstate SET NOT NULL,

      ALTER COLUMN reportedstate
              TYPE pg_ram.replication_state
             USING reportedstate::text::pg_ram.replication_state;

ALTER TABLE pg_ram.event
      ALTER COLUMN goalstate
              TYPE pg_ram.replication_state
             USING goalstate::text::pg_ram.replication_state,

      ALTER COLUMN reportedstate
              TYPE pg_ram.replication_state
             USING reportedstate::text::pg_ram.replication_state;

DROP TYPE pg_ram.old_replication_state;

ALTER TABLE pg_ram.cluster
  ADD COLUMN number_sync_standbys int  NOT NULL DEFAULT 1;

DROP FUNCTION IF EXISTS pg_ram.create_cluster(text, text);

DROP FUNCTION IF EXISTS pg_ram.create_cluster(text,text,name,boolean);
DROP FUNCTION IF EXISTS pg_ram.get_other_node(text,integer);

CREATE FUNCTION pg_ram.set_cluster_number_sync_standbys
 (
    IN cluster_id  		text,
    IN number_sync_standbys int
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_cluster_number_sync_standbys$$;

grant execute on function
      pg_ram.set_cluster_number_sync_standbys(text, int)
   to autoctl_node;

CREATE FUNCTION pg_ram.create_cluster
 (
    IN cluster_id         text,
    IN kind                 text,
    IN dbname               name,
    IN opt_secondary        bool,
    IN number_sync_standbys int,
   OUT cluster_id         text,
   OUT kind                 text,
   OUT dbname               name,
   OUT opt_secondary        bool,
   OUT number_sync_standbys int
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$create_cluster$$;

grant execute on function
      pg_ram.create_cluster(text,text,name,bool,int)
   to autoctl_node;

ALTER TABLE pg_ram.node
	RENAME TO node_upgrade_old;

CREATE TABLE pg_ram.node
 (
    clusterid          text not null default 'default',
    nodeid               bigint not null DEFAULT nextval('pg_ram.node_nodeid_seq'::regclass),
    groupid              int not null,
    nodename             text not null,
    nodeport             int not null,
    goalstate            pg_ram.replication_state not null default 'init',
    reportedstate        pg_ram.replication_state not null,
    reportedpgisrunning  bool default true,
    reportedrepstate     text default 'async',
    reporttime           timestamptz not null default now(),
    reportedlsn          pg_lsn not null default '0/0',
    walreporttime        timestamptz not null default now(),
    health               integer not null default -1,
    healthchecktime      timestamptz not null default now(),
    statechangetime      timestamptz not null default now(),
    candidatepriority	 int not null default 100,
    replicationquorum	 bool not null default true,

    UNIQUE (nodename, nodeport),
    PRIMARY KEY (nodeid),
    FOREIGN KEY (clusterid) REFERENCES pg_ram.cluster(clusterid)
 )
 -- we expect few rows and lots of UPDATE, let's benefit from HOT
 WITH (fillfactor = 25);

ALTER SEQUENCE pg_ram.node_nodeid_seq OWNED BY pg_ram.node.nodeid;

INSERT INTO pg_ram.node
 (
  clusterid, nodeid, groupid, nodename, nodeport,
  goalstate, reportedstate, reportedpgisrunning, reportedrepstate,
  reporttime, reportedlsn, walreporttime,
  health, healthchecktime, statechangetime
 )
 SELECT clusterid, nodeid, groupid, nodename, nodeport,
        goalstate, reportedstate, reportedpgisrunning, reportedrepstate,
        reporttime, reportedlsn, walreporttime,
        health, healthchecktime, statechangetime
   FROM pg_ram.node_upgrade_old;


ALTER TABLE pg_ram.event
	RENAME TO event_upgrade_old;

CREATE TABLE pg_ram.event
 (
    eventid           bigint not null DEFAULT nextval('pg_ram.event_eventid_seq'::regclass),
    eventtime         timestamptz not null default now(),
    clusterid       text not null,
    nodeid            bigint not null,
    groupid           int not null,
    nodename          text not null,
    nodeport          integer not null,
    reportedstate     pg_ram.replication_state not null,
    goalstate         pg_ram.replication_state not null,
    reportedrepstate  text,
    reportedlsn       pg_lsn not null default '0/0',
    candidatepriority int,
    replicationquorum bool,
    description       text,

    PRIMARY KEY (eventid)
 );

ALTER SEQUENCE pg_ram.event_eventid_seq
      OWNED BY pg_ram.event.eventid;

INSERT INTO pg_ram.event
 (
  eventid, eventtime, clusterid, nodeid, groupid,
  nodename, nodeport,
  reportedstate, goalstate, reportedrepstate, description
 )
 SELECT eventid, eventtime, clusterid, nodeid, groupid,
        nodename, nodeport,
        reportedstate, goalstate, reportedrepstate, description
   FROM pg_ram.event_upgrade_old;

GRANT SELECT ON ALL TABLES IN SCHEMA pg_ram TO autoctl_node;

CREATE FUNCTION pg_ram.set_node_nodename
 (
    IN node_id   bigint,
    IN node_name text,
   OUT node_id   bigint,
   OUT name      text,
   OUT port      int
 )
RETURNS record LANGUAGE SQL STRICT SECURITY DEFINER
AS $$
      update pg_ram.node
         set nodename = node_name
       where nodeid = node_id
   returning nodeid, nodename, nodeport;
$$;

grant execute on function pg_ram.set_node_nodename(bigint,text)
   to autoctl_node;


DROP FUNCTION IF EXISTS pg_ram.register_node(text, text);

CREATE FUNCTION pg_ram.register_node
 (
    IN cluster_id         text,
    IN node_name            text,
    IN node_port            int,
    IN dbname               name,
    IN desired_group_id     int default -1,
    IN initial_group_role   pg_ram.replication_state default 'init',
    IN node_kind            text default 'standalone',
    IN candidate_priority 	int default 100,
    IN replication_quorum	bool default true,
   OUT assigned_node_id     int,
   OUT assigned_group_id    int,
   OUT assigned_group_state pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$register_node$$;

grant execute on function
      pg_ram.register_node(text,text,int,name,int,pg_ram.replication_state,text, int, bool)
   to autoctl_node;


CREATE FUNCTION pg_ram.node_active
 (
    In cluster_id           		text,
    IN node_name              		text,
    IN node_port              		int,
    IN current_node_id        		int default -1,
    IN current_group_id       		int default -1,
    IN current_group_role     		pg_ram.replication_state default 'init',
    IN current_pg_is_running  		bool default true,
    IN current_lsn			  		pg_lsn default '0/0',
    IN current_rep_state      		text default '',
   OUT assigned_node_id       		int,
   OUT assigned_group_id      		int,
   OUT assigned_group_state   		pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$node_active$$;

grant execute on function
      pg_ram.node_active(text,text,int,int,int,
                          pg_ram.replication_state,bool,pg_lsn,text)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.get_nodes(text, text);

CREATE FUNCTION pg_ram.get_nodes
 (
    IN cluster_id     text default 'default',
    IN group_id         int default NULL,
   OUT node_id          int,
   OUT node_name        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C
AS 'MODULE_PATHNAME', $$get_nodes$$;

comment on function pg_ram.get_nodes(text,int)
        is 'get all the nodes in a group';

grant execute on function pg_ram.get_nodes(text,int)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.get_primary(text,int);

CREATE FUNCTION pg_ram.get_primary
 (
    IN cluster_id      text default 'default',
    IN group_id          int default 0,
   OUT primary_node_id   int,
   OUT primary_name      text,
   OUT primary_port      int
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$get_primary$$;

comment on function pg_ram.get_primary(text,int)
        is 'get the writable node for a group';

grant execute on function pg_ram.get_primary(text,int)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.get_other_nodes(text. int);

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN node_name        text,
    IN node_port        int,
   OUT node_id          int,
   OUT node_name        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes(text,int)
        is 'get the other nodes in a group';

grant execute on function pg_ram.get_other_nodes(text,int)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.get_other_nodes
                        (text. int, pg_ram.replication_state);

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN node_name        text,
    IN node_port        int,
    IN current_state    pg_ram.replication_state,
   OUT node_id          int,
   OUT node_name        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes
                    (text,int,pg_ram.replication_state)
        is 'get the other nodes in a group, filtering on current_state';

grant execute on function pg_ram.get_other_nodes
                          (text,int,pg_ram.replication_state)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.last_events(int);

CREATE FUNCTION pg_ram.last_events
 (
  count int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
  select eventid, eventtime, clusterid,
         nodeid, groupid, nodename, nodeport,
         reportedstate, goalstate,
         reportedrepstate, reportedlsn, candidatepriority, replicationquorum, description
    from pg_ram.event
order by eventid desc
   limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(int)
        is 'retrieve last COUNT events';

DROP FUNCTION IF EXISTS pg_ram.last_events(text,int);

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text default 'default',
  count        int  default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedlsn, candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int)
        is 'retrieve last COUNT events for given cluster';

DROP FUNCTION IF EXISTS pg_ram.last_events(text,int,int);

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text,
  group_id     int,
  count        int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedlsn, candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
       and groupid = group_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int,int)
        is 'retrieve last COUNT events for given cluster and group';


CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text default 'default',
   OUT nodename             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select nodename, nodeport, groupid, nodeid, reportedstate, goalstate,
   		candidatepriority, replicationquorum
   from pg_ram.node
   where clusterid = cluster_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text)
        is 'get the current state of both nodes of a cluster';

CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text,
    IN group_id             int,
   OUT nodename             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select nodename, nodeport, groupid, nodeid, reportedstate, goalstate,
   		  candidatepriority, replicationquorum
   from pg_ram.node
   where clusterid = cluster_id
      and groupid = group_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text, int)
        is 'get the current state of both nodes of a group in a cluster';

DROP FUNCTION IF EXISTS pg_ram.cluster_uri(text, text);

CREATE FUNCTION pg_ram.cluster_uri
 (
    IN cluster_id         text DEFAULT 'default',
    IN sslmode              text DEFAULT 'prefer',
    IN sslrootcert          text DEFAULT '',
    IN sslcrl               text DEFAULT ''
 )
RETURNS text LANGUAGE SQL STRICT
AS $$
    select case
           when string_agg(format('%s:%s', nodename, nodeport),',') is not null
           then format(
               'postgres://%s/%s?target_session_attrs=read-write&sslmode=%s%s%s',
               string_agg(format('%s:%s', nodename, nodeport),','),
               -- as we join cluster on node we get the same dbname for all
               -- entries, pick one.
               min(dbname),
               min(sslmode),
               CASE WHEN min(sslrootcert) = ''
                   THEN ''
                   ELSE '&sslrootcert=' || sslrootcert
               END,
               CASE WHEN min(sslcrl) = ''
                   THEN ''
                   ELSE '&sslcrl=' || sslcrl
               END
           )
           end as uri
      from pg_ram.node as node
           join pg_ram.cluster using(clusterid)
     where clusterid = cluster_id
       and groupid = 0;
$$;

CREATE FUNCTION pg_ram.set_node_candidate_priority
 (
    IN nodeid				int,
	IN nodename             text,
	IN nodeport             int,
    IN candidate_priority	int
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_node_candidate_priority$$;

comment on function pg_ram.set_node_candidate_priority(int, text, int, int)
        is 'sets the candidate priority value for a node. Expects a priority value between 0 and 100. 0 if the node is not a candidate to be promoted to be primary.';

grant execute on function
      pg_ram.set_node_candidate_priority(int, text, int, int)
   to autoctl_node;

CREATE FUNCTION pg_ram.set_node_replication_quorum
 (
    IN nodeid				int,
	IN nodename             text,
	IN nodeport             int,
    IN replication_quorum	bool
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_node_replication_quorum$$;

comment on function pg_ram.set_node_replication_quorum(int, text, int, bool)
        is 'sets the replication quorum value for a node. true if the node participates in write quorum';

grant execute on function
      pg_ram.set_node_replication_quorum(int, text, int, bool)
   to autoctl_node;


create function pg_ram.synchronous_standby_names
 (
    IN cluster_id text default 'default',
    IN group_id     int default 0
 )
returns text language C strict
AS 'MODULE_PATHNAME', $$synchronous_standby_names$$;

comment on function pg_ram.synchronous_standby_names(text, int)
        is 'get the synchronous_standby_names setting for a given group';

grant execute on function
      pg_ram.synchronous_standby_names(text, int)
   to autoctl_node;


CREATE OR REPLACE FUNCTION pg_ram.adjust_number_sync_standbys()
  RETURNS trigger
  LANGUAGE 'plpgsql'
AS $$
declare
  standby_count integer := null;
  number_sync_standbys integer := null;
begin
   select count(*) - 1
     into standby_count
     from pg_ram.node
    where clusterid = old.clusterid;

   select cluster.number_sync_standbys
     into number_sync_standbys
     from pg_ram.cluster
    where cluster.clusterid = old.clusterid;

  if number_sync_standbys > 1
  then
    -- we must have number_sync_standbys + 1 <= standby_count
    if (number_sync_standbys + 1) > standby_count
    then
      update pg_ram.cluster
         set number_sync_standbys = greatest(standby_count - 1, 1)
       where cluster.clusterid = old.clusterid;
    end if;
  end if;

  return old;
end
$$;

comment on function pg_ram.adjust_number_sync_standbys()
        is 'adjust cluster number_sync_standbys when removing a node, if needed';

CREATE TRIGGER adjust_number_sync_standbys
         AFTER DELETE
            ON pg_ram.node
           FOR EACH ROW
       EXECUTE PROCEDURE pg_ram.adjust_number_sync_standbys();

DROP TABLE pg_ram.node_upgrade_old;
DROP TABLE pg_ram.event_upgrade_old;
--
-- extension update file from 1.3 to 1.4
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ram" to load this file. \quit

DROP FUNCTION IF EXISTS pg_ram.register_node(text,text,int,name,int,
                          pg_ram.replication_state,text, int, bool);

DROP FUNCTION IF EXISTS pg_ram.node_active(text,text,int,int,int,
                          pg_ram.replication_state,bool,pg_lsn,text);

DROP FUNCTION IF EXISTS pg_ram.get_other_nodes
                          (text,integer,pg_ram.replication_state);

DROP FUNCTION IF EXISTS pg_ram.current_state(text);

DROP FUNCTION IF EXISTS pg_ram.current_state(text, int);

ALTER TYPE pg_ram.replication_state RENAME TO old_replication_state;

CREATE TYPE pg_ram.replication_state
    AS ENUM
(
    'unknown',
    'init',
    'single',
    'wait_primary',
    'primary',
    'draining',
    'demote_timeout',
    'demoted',
    'catchingup',
    'secondary',
    'prepare_promotion',
    'stop_replication',
    'wait_standby',
    'maintenance',
    'join_primary',
    'apply_settings',
    'prepare_maintenance',
    'wait_maintenance',
    'report_lsn',
    'fast_forward',
    'join_secondary'
 );

-- Note the double cast here, first to text and only then to the new enums
ALTER TABLE pg_ram.node
      ALTER COLUMN goalstate DROP NOT NULL,
      ALTER COLUMN goalstate DROP DEFAULT,

      ALTER COLUMN goalstate
              TYPE pg_ram.replication_state
             USING goalstate::text::pg_ram.replication_state,

      ALTER COLUMN goalstate SET DEFAULT 'init',
      ALTER COLUMN goalstate SET NOT NULL,

      ALTER COLUMN reportedstate
              TYPE pg_ram.replication_state
             USING reportedstate::text::pg_ram.replication_state;

ALTER TABLE pg_ram.event
      ALTER COLUMN goalstate
              TYPE pg_ram.replication_state
             USING goalstate::text::pg_ram.replication_state,

      ALTER COLUMN reportedstate
              TYPE pg_ram.replication_state
             USING reportedstate::text::pg_ram.replication_state;

DROP TYPE pg_ram.old_replication_state;

ALTER TABLE pg_ram.cluster
      ALTER COLUMN number_sync_standbys
       SET DEFAULT 0;

--
-- The default used to be 1, now it's zero. Change it for people who left
-- the default (everybody, most certainly, because this used to have no
-- impact).
--
UPDATE pg_ram.cluster
   SET number_sync_standbys = 0
 WHERE number_sync_standbys = 1;

ALTER TABLE pg_ram.cluster
        ADD CHECK (kind IN ('pgsql'));

ALTER TABLE pg_ram.node
	RENAME TO node_upgrade_old;

CREATE TABLE pg_ram.node
 (
    clusterid          text not null default 'default',
    nodeid               bigint not null DEFAULT nextval('pg_ram.node_nodeid_seq'::regclass),
    groupid              int not null,
    nodename             text not null,
    nodehost             text not null,
    nodeport             int not null,
    sysidentifier        bigint,
    goalstate            pg_ram.replication_state not null default 'init',
    reportedstate        pg_ram.replication_state not null,
    reportedpgisrunning  bool default true,
    reportedrepstate     text default 'async',
    reporttime           timestamptz not null default now(),
    reportedlsn          pg_lsn not null default '0/0',
    walreporttime        timestamptz not null default now(),
    health               integer not null default -1,
    healthchecktime      timestamptz not null default now(),
    statechangetime      timestamptz not null default now(),
    candidatepriority	 int not null default 100,
    replicationquorum	 bool not null default true,

    -- node names must be unique in a given cluster
    UNIQUE (clusterid, nodename),
    -- any nodehost:port can only be a unique node in the system
    UNIQUE (nodehost, nodeport),
    --
    -- The EXCLUDE constraint only allows the same sysidentifier for all the
    -- nodes in the same group. The system_identifier is a property that is
    -- kept when implementing streaming replication and should be unique per
    -- Postgres instance in all other cases.
    --
    -- We allow the sysidentifier column to be NULL when registering a new
    -- primary server from scratch, because we have not done pg_ctl initdb
    -- at the time we call the register_node() function.
    --
    CONSTRAINT system_identifier_is_null_at_init_only
         CHECK (  (    sysidentifier IS NULL
                   AND reportedstate in ('init', 'wait_standby', 'catchingup') )
                OR sysidentifier IS NOT NULL),

    CONSTRAINT same_system_identifier_within_group
       EXCLUDE USING gist(clusterid with =,
                          groupid with =,
                          sysidentifier with <>)
    DEFERRABLE INITIALLY DEFERRED,

    PRIMARY KEY (nodeid),
    FOREIGN KEY (clusterid) REFERENCES pg_ram.cluster(clusterid)
 )
 -- we expect few rows and lots of UPDATE, let's benefit from HOT
 WITH (fillfactor = 25);

ALTER SEQUENCE pg_ram.node_nodeid_seq OWNED BY pg_ram.node.nodeid;

INSERT INTO pg_ram.node
 (
  clusterid, nodeid, groupid, nodename, nodehost, nodeport, sysidentifier,
  goalstate, reportedstate, reportedpgisrunning, reportedrepstate,
  reporttime, reportedlsn, walreporttime,
  health, healthchecktime, statechangetime,
  candidatepriority, replicationquorum
 )
 SELECT clusterid, nodeid, groupid,
        format('node_%s', nodeid) as nodename,
        nodename as nodehost, nodeport, 0 as sysidentifier,
        goalstate, reportedstate, reportedpgisrunning, reportedrepstate,
        reporttime, reportedlsn, walreporttime,
        health, healthchecktime, statechangetime,
        candidatepriority, replicationquorum
   FROM pg_ram.node_upgrade_old;


ALTER TABLE pg_ram.event
	RENAME TO event_upgrade_old;

CREATE TABLE pg_ram.event
 (
    eventid           bigint not null DEFAULT nextval('pg_ram.event_eventid_seq'::regclass),
    eventtime         timestamptz not null default now(),
    clusterid       text not null,
    nodeid            bigint not null,
    groupid           int not null,
    nodename          text not null,
    nodehost          text not null,
    nodeport          integer not null,
    reportedstate     pg_ram.replication_state not null,
    goalstate         pg_ram.replication_state not null,
    reportedrepstate  text,
    reportedlsn       pg_lsn not null default '0/0',
    candidatepriority int,
    replicationquorum bool,
    description       text,

    PRIMARY KEY (eventid)
 );

ALTER SEQUENCE pg_ram.event_eventid_seq
      OWNED BY pg_ram.event.eventid;

INSERT INTO pg_ram.event
 (
  eventid, eventtime, clusterid, nodeid, groupid,
  nodename, nodehost, nodeport,
  reportedstate, goalstate, reportedrepstate, description
 )
 SELECT eventid, eventtime, event.clusterid, event.nodeid, event.groupid,
        node.nodename, node.nodehost, event.nodeport,
        event.reportedstate, event.goalstate, event.reportedrepstate,
        event.description
   FROM pg_ram.event_upgrade_old as event
   JOIN pg_ram.node USING(nodeid);

GRANT SELECT ON ALL TABLES IN SCHEMA pg_ram TO autoctl_node;

CREATE FUNCTION pg_ram.set_node_system_identifier
 (
    IN node_id             bigint,
    IN node_sysidentifier  bigint,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int
 )
RETURNS record LANGUAGE SQL STRICT SECURITY DEFINER
AS $$
      update pg_ram.node
         set sysidentifier = node_sysidentifier
       where nodeid = set_node_system_identifier.node_id
   returning nodeid, nodename, nodehost, nodeport;
$$;

grant execute on function pg_ram.set_node_system_identifier(bigint,bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.set_group_system_identifier
 (
    IN group_id            bigint,
    IN node_sysidentifier  bigint,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int
 )
RETURNS setof record LANGUAGE SQL STRICT SECURITY DEFINER
AS $$
      update pg_ram.node
         set sysidentifier = node_sysidentifier
       where groupid = set_group_system_identifier.group_id
         and sysidentifier = 0
   returning nodeid, nodename, nodehost, nodeport;
$$;

grant execute on function pg_ram.set_group_system_identifier(bigint,bigint)
   to autoctl_node;


DROP FUNCTION pg_ram.set_node_nodename(bigint,text);

CREATE FUNCTION pg_ram.update_node_metadata
  (
     IN node_id   bigint,
     IN node_name text,
     IN node_host text,
     IN node_port int
  )
 RETURNS boolean LANGUAGE C SECURITY DEFINER
 AS 'MODULE_PATHNAME', $$update_node_metadata$$;

grant execute on function pg_ram.update_node_metadata(bigint,text,text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.register_node
 (
    IN cluster_id         text,
    IN node_host            text,
    IN node_port            int,
    IN dbname               name,
    IN node_name            text default '',
    IN sysidentifier        bigint default 0,
    IN desired_group_id     int default -1,
    IN initial_group_role   pg_ram.replication_state default 'init',
    IN node_kind            text default 'standalone',
    IN candidate_priority 	int default 100,
    IN replication_quorum	bool default true,
   OUT assigned_node_id     int,
   OUT assigned_group_id    int,
   OUT assigned_group_state pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool,
   OUT assigned_node_name   text
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$register_node$$;

grant execute on function
      pg_ram.register_node(text,text,int,name,text,bigint,int,pg_ram.replication_state,text, int, bool)
   to autoctl_node;

CREATE FUNCTION pg_ram.node_active
 (
    IN cluster_id           		text,
    IN node_id        		        int,
    IN group_id       		        int,
    IN current_group_role     		pg_ram.replication_state default 'init',
    IN current_pg_is_running  		bool default true,
    IN current_lsn			  		pg_lsn default '0/0',
    IN current_rep_state      		text default '',
   OUT assigned_node_id       		int,
   OUT assigned_group_id      		int,
   OUT assigned_group_state   		pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$node_active$$;

grant execute on function
      pg_ram.node_active(text,int,int,
                          pg_ram.replication_state,bool,pg_lsn,text)
   to autoctl_node;


DROP FUNCTION pg_ram.get_nodes(text, int);

CREATE FUNCTION pg_ram.get_nodes
 (
    IN cluster_id     text default 'default',
    IN group_id         int default NULL,
   OUT node_id          int,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C
AS 'MODULE_PATHNAME', $$get_nodes$$;

comment on function pg_ram.get_nodes(text,int)
        is 'get all the nodes in a group';

grant execute on function pg_ram.get_nodes(text,int)
   to autoctl_node;

DROP FUNCTION pg_ram.get_primary(text, int);

CREATE FUNCTION pg_ram.get_primary
 (
    IN cluster_id      text default 'default',
    IN group_id          int default 0,
   OUT primary_node_id   int,
   OUT primary_name      text,
   OUT primary_host      text,
   OUT primary_port      int
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$get_primary$$;

comment on function pg_ram.get_primary(text,int)
        is 'get the writable node for a group';

grant execute on function pg_ram.get_primary(text,int)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.get_other_nodes (text,integer);

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN nodeid           int,
   OUT node_id          int,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes(int)
        is 'get the other nodes in a group';

grant execute on function pg_ram.get_other_nodes(int)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN nodeid           int,
    IN current_state    pg_ram.replication_state,
   OUT node_id          int,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes
                    (int,pg_ram.replication_state)
        is 'get the other nodes in a group, filtering on current_state';

grant execute on function pg_ram.get_other_nodes
                          (int,pg_ram.replication_state)
   to autoctl_node;


DROP FUNCTION pg_ram.get_coordinator(text);

CREATE FUNCTION pg_ram.get_coordinator
 (
    IN cluster_id  text default 'default',
   OUT node_host     text,
   OUT node_port     int
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
  select nodehost, nodeport
    from pg_ram.node
         join pg_ram.cluster using(clusterid)
   where clusterid = cluster_id
     and groupid = 0
     and goalstate in ('single', 'wait_primary', 'primary')
     and reportedstate in ('single', 'wait_primary', 'primary');
$$;

grant execute on function pg_ram.get_coordinator(text)
   to autoctl_node;


CREATE FUNCTION pg_ram.get_most_advanced_standby
 (
   IN clusterid       text default 'default',
   IN groupid           int default 0,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select nodeid, nodename, nodehost, nodeport, reportedlsn, false
     from pg_ram.node
    where clusterid = $1
      and groupid = $2
      and reportedstate = 'report_lsn'
 order by reportedlsn desc, health desc
    limit 1;
$$;

grant execute on function pg_ram.get_most_advanced_standby(text,int)
   to autoctl_node;

DROP FUNCTION IF EXISTS pg_ram.remove_node(text, int);

CREATE FUNCTION pg_ram.remove_node
 (
   node_id int
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$remove_node_by_nodeid$$;

comment on function pg_ram.remove_node(int)
        is 'remove a node from the monitor';

grant execute on function pg_ram.remove_node(int)
   to autoctl_node;

CREATE FUNCTION pg_ram.remove_node
 (
   node_host text,
   node_port int default 5432
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$remove_node_by_host$$;

comment on function pg_ram.remove_node(text,int)
        is 'remove a node from the monitor';

grant execute on function pg_ram.remove_node(text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.perform_promotion
 (
  cluster_id text,
  node_name    text
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$perform_promotion$$;

comment on function pg_ram.perform_promotion(text,text)
        is 'manually failover from the primary to the given node';

grant execute on function pg_ram.perform_promotion(text,text)
   to autoctl_node;

DROP FUNCTION pg_ram.start_maintenance(text, int);
DROP FUNCTION pg_ram.stop_maintenance(text, int);

CREATE FUNCTION pg_ram.start_maintenance(node_id int)
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$start_maintenance$$;

comment on function pg_ram.start_maintenance(int)
        is 'set a node in maintenance state';

grant execute on function pg_ram.start_maintenance(int)
   to autoctl_node;

CREATE FUNCTION pg_ram.stop_maintenance(node_id int)
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$stop_maintenance$$;

comment on function pg_ram.stop_maintenance(int)
        is 'set a node out of maintenance state';

grant execute on function pg_ram.stop_maintenance(int)
   to autoctl_node;

DROP FUNCTION pg_ram.last_events(int);
DROP FUNCTION pg_ram.last_events(text,int);
DROP FUNCTION pg_ram.last_events(text,int,int);

CREATE FUNCTION pg_ram.last_events
 (
  count int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
  select eventid, eventtime, clusterid,
         nodeid, groupid, nodename, nodehost, nodeport,
         reportedstate, goalstate,
         reportedrepstate, reportedlsn,
         candidatepriority, replicationquorum, description
    from pg_ram.event
order by eventid desc
   limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(int)
        is 'retrieve last COUNT events';

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text default 'default',
  count        int  default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodehost, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedlsn,
           candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int)
        is 'retrieve last COUNT events for given cluster';

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text,
  group_id     int,
  count        int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodehost, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedlsn,
           candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
       and groupid = group_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int,int)
        is 'retrieve last COUNT events for given cluster and group';


CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text default 'default',
   OUT cluster_kind       text,
   OUT nodename             text,
   OUT nodehost             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool,
   OUT reported_lsn         pg_lsn,
   OUT health               integer
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select kind, nodename, nodehost, nodeport, groupid, nodeid,
          reportedstate, goalstate,
   		  candidatepriority, replicationquorum,
          reportedlsn, health
     from pg_ram.node
     join pg_ram.cluster using(clusterid)
    where clusterid = cluster_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text)
        is 'get the current state of both nodes of a cluster';

CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text,
    IN group_id             int,
   OUT cluster_kind       text,
   OUT nodename             text,
   OUT nodehost             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool,
   OUT reported_lsn         pg_lsn,
   OUT health               integer
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select kind, nodename, nodehost, nodeport, groupid, nodeid,
          reportedstate, goalstate,
   		  candidatepriority, replicationquorum,
          reportedlsn, health
     from pg_ram.node
     join pg_ram.cluster using(clusterid)
    where clusterid = cluster_id
      and groupid = group_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text, int)
        is 'get the current state of both nodes of a group in a cluster';


CREATE OR REPLACE FUNCTION pg_ram.cluster_uri
 (
    IN cluster_id         text DEFAULT 'default',
    IN sslmode              text DEFAULT 'prefer',
    IN sslrootcert          text DEFAULT '',
    IN sslcrl               text DEFAULT ''
 )
RETURNS text LANGUAGE SQL STRICT
AS $$
    select case
           when string_agg(format('%s:%s', nodehost, nodeport),',') is not null
           then format(
               'postgres://%s/%s?target_session_attrs=read-write&sslmode=%s%s%s',
               string_agg(format('%s:%s', nodehost, nodeport),','),
               -- as we join cluster on node we get the same dbname for all
               -- entries, pick one.
               min(dbname),
               min(sslmode),
               CASE WHEN min(sslrootcert) = ''
                   THEN ''
                   ELSE '&sslrootcert=' || sslrootcert
               END,
               CASE WHEN min(sslcrl) = ''
                   THEN ''
                   ELSE '&sslcrl=' || sslcrl
               END
           )
           end as uri
      from pg_ram.node as node
           join pg_ram.cluster using(clusterid)
     where clusterid = cluster_id
       and groupid = 0;
$$;

DROP FUNCTION pg_ram.set_node_candidate_priority(int,text,int,int);

CREATE FUNCTION pg_ram.set_node_candidate_priority
 (
    IN cluster_id         text,
    IN node_name            text,
    IN candidate_priority	int
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_node_candidate_priority$$;

comment on function pg_ram.set_node_candidate_priority(text, text, int)
        is 'sets the candidate priority value for a node. Expects a priority value between 0 and 100. 0 if the node is not a candidate to be promoted to be primary.';

grant execute on function
      pg_ram.set_node_candidate_priority(text, text, int)
   to autoctl_node;

DROP FUNCTION pg_ram.set_node_replication_quorum(int,text,int,bool);

CREATE FUNCTION pg_ram.set_node_replication_quorum
 (
    IN cluster_id       text,
    IN node_name          text,
    IN replication_quorum bool
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$set_node_replication_quorum$$;

comment on function pg_ram.set_node_replication_quorum(text, text, bool)
        is 'sets the replication quorum value for a node. true if the node participates in write quorum';

grant execute on function
      pg_ram.set_node_replication_quorum(text, text, bool)
   to autoctl_node;

CREATE FUNCTION pg_ram.cluster_settings
 (
    IN cluster_id         text default 'default',
   OUT context              text,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT nodename             text,
   OUT setting              text,
   OUT value                text
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
  with groups(clusterid, groupid) as
  (
     select clusterid, groupid
       from pg_ram.node
      where clusterid = cluster_id
   group by clusterid, groupid
  )

  -- context: cluster, number_sync_standbys
  select 'cluster' as context,
         NULL as group_id, NULL as node_id, clusterid as nodename,
         'number_sync_standbys' as setting,
         cast(number_sync_standbys as text) as value
    from pg_ram.cluster
   where clusterid = cluster_id

union all

  -- context: primary, one entry per group in the cluster
  select 'primary', groups.groupid, nodes.node_id, nodes.node_name,
         'synchronous_standby_names',
         format('''%s''',
         pg_ram.synchronous_standby_names(clusterid, groupid))
    from groups, pg_ram.get_nodes(clusterid, groupid) as nodes
   where node_is_primary

union all

(
  -- context: node, one entry per node in the cluster
  select 'node', node.groupid, node.nodeid, node.nodename,
         'replication quorum', cast(node.replicationquorum as text)
    from pg_ram.node as node
   where node.clusterid = cluster_id
order by nodeid
)

union all

(
  select 'node', node.groupid, node.nodeid, node.nodename,
         'candidate priority', cast(node.candidatepriority as text)
    from pg_ram.node as node
   where node.clusterid = cluster_id
order by nodeid
)
$$;

comment on function pg_ram.cluster_settings(text)
        is 'get the current replication settings a cluster';

drop function pg_ram.adjust_number_sync_standbys() cascade;

DROP TABLE pg_ram.node_upgrade_old;
DROP TABLE pg_ram.event_upgrade_old;
--
-- extension update file from 1.4.2 to 1.5.1
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ram" to load this file. \quit

ALTER TABLE pg_ram.node
  ADD COLUMN nodecluster text not null default 'default';

DROP FUNCTION IF EXISTS pg_ram.cluster_uri(text, text, text, text);

CREATE FUNCTION pg_ram.cluster_uri
 (
    IN cluster_id         text DEFAULT 'default',
    IN cluster_name         text DEFAULT 'default',
    IN sslmode              text DEFAULT 'prefer',
    IN sslrootcert          text DEFAULT '',
    IN sslcrl               text DEFAULT ''
 )
RETURNS text LANGUAGE SQL STRICT
AS $$
    select case
           when string_agg(format('%s:%s', nodehost, nodeport),',') is not null
           then format(
               'postgres://%s/%s?%ssslmode=%s%s%s',
               string_agg(format('%s:%s', nodehost, nodeport),','),
               -- as we join cluster on node we get the same dbname for all
               -- entries, pick one.
               min(dbname),
               case when cluster_name = 'default'
                    then 'target_session_attrs=read-write&'
                    else ''
               end,
               min(sslmode),
               CASE WHEN min(sslrootcert) = ''
                   THEN ''
                   ELSE '&sslrootcert=' || sslrootcert
               END,
               CASE WHEN min(sslcrl) = ''
                   THEN ''
                   ELSE '&sslcrl=' || sslcrl
               END
           )
           end as uri
      from pg_ram.node as node
           join pg_ram.cluster using(clusterid)
     where clusterid = cluster_id
       and groupid = 0
       and nodecluster = cluster_name;
$$;

DROP FUNCTION IF EXISTS
     pg_ram.register_node(text,text,int,name,text,bigint,int,
                                  pg_ram.replication_state,text,
                                  int,bool,text);

CREATE FUNCTION pg_ram.register_node
 (
    IN cluster_id         text,
    IN node_host            text,
    IN node_port            int,
    IN dbname               name,
    IN node_name            text default '',
    IN sysidentifier        bigint default 0,
    IN desired_node_id      int default -1,
    IN desired_group_id     int default -1,
    IN initial_group_role   pg_ram.replication_state default 'init',
    IN node_kind            text default 'standalone',
    IN candidate_priority 	int default 100,
    IN replication_quorum	bool default true,
    IN node_cluster         text default 'default',
   OUT assigned_node_id     int,
   OUT assigned_group_id    int,
   OUT assigned_group_state pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool,
   OUT assigned_node_name   text
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$register_node$$;

grant execute on function
      pg_ram.register_node(text,text,int,name,text,bigint,int,int,
                                   pg_ram.replication_state,text,
                                   int,bool,text)
   to autoctl_node;
--
-- extension update file from 1.5 to 1.6
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ram" to load this file. \quit

-- remove a possible leftover from older versions that was not correctly
-- removed in a migration to 1.5
DROP FUNCTION IF EXISTS
     pg_ram.register_node(text,text,int,name,text,bigint,int,
                                  pg_ram.replication_state,text,
                                  int,bool);

DROP FUNCTION
     pg_ram.register_node(text,text,int,name,text,bigint,int,int,
                                  pg_ram.replication_state,text,
                                  int,bool,text);

DROP FUNCTION
     pg_ram.node_active(text,int,int,
                                pg_ram.replication_state,bool,pg_lsn,text);

DROP FUNCTION pg_ram.get_other_nodes(int);

DROP FUNCTION pg_ram.get_other_nodes
              (integer,pg_ram.replication_state);

DROP FUNCTION pg_ram.last_events(int);
DROP FUNCTION pg_ram.last_events(text,int);
DROP FUNCTION pg_ram.last_events(text,int,int);

DROP FUNCTION pg_ram.current_state(text);
DROP FUNCTION pg_ram.current_state(text,int);

DROP TRIGGER disable_secondary_check ON pg_ram.cluster;
DROP FUNCTION pg_ram.update_secondary_check();

ALTER TYPE pg_ram.replication_state RENAME TO old_replication_state;

CREATE TYPE pg_ram.replication_state
    AS ENUM
 (
    'unknown',
    'init',
    'single',
    'wait_primary',
    'primary',
    'draining',
    'demote_timeout',
    'demoted',
    'catchingup',
    'secondary',
    'prepare_promotion',
    'stop_replication',
    'wait_standby',
    'maintenance',
    'join_primary',
    'apply_settings',
    'prepare_maintenance',
    'wait_maintenance',
    'report_lsn',
    'fast_forward',
    'join_secondary',
    'dropped'
 );

-- Note the double cast here, first to text and only then to the new enums
ALTER TABLE pg_ram.event
      ALTER COLUMN goalstate
              TYPE pg_ram.replication_state
             USING goalstate::text::pg_ram.replication_state,

      ALTER COLUMN reportedstate
              TYPE pg_ram.replication_state
             USING reportedstate::text::pg_ram.replication_state;

ALTER TABLE pg_ram.node RENAME TO node_upgrade_old;

ALTER TABLE pg_ram.node_upgrade_old
      RENAME CONSTRAINT system_identifier_is_null_at_init_only
                     TO system_identifier_is_null_at_init_only_old;

ALTER TABLE pg_ram.node_upgrade_old
      RENAME CONSTRAINT same_system_identifier_within_group
                     TO same_system_identifier_within_group_old;

CREATE TABLE pg_ram.node
 (
    clusterid          text not null default 'default',
    nodeid               bigint not null DEFAULT nextval('pg_ram.node_nodeid_seq'::regclass),
    groupid              int not null,
    nodename             text not null,
    nodehost             text not null,
    nodeport             int not null,
    sysidentifier        bigint,
    goalstate            pg_ram.replication_state not null default 'init',
    reportedstate        pg_ram.replication_state not null,
    reportedpgisrunning  bool default true,
    reportedrepstate     text default 'async',
    reporttime           timestamptz not null default now(),
    reportedtli          int not null default 1 check (reportedtli > 0),
    reportedlsn          pg_lsn not null default '0/0',
    walreporttime        timestamptz not null default now(),
    health               integer not null default -1,
    healthchecktime      timestamptz not null default now(),
    statechangetime      timestamptz not null default now(),
    candidatepriority	 int not null default 100,
    replicationquorum	 bool not null default true,
    nodecluster          text not null default 'default',

    -- node names must be unique in a given cluster
    UNIQUE (clusterid, nodename),
    -- any nodehost:port can only be a unique node in the system
    UNIQUE (nodehost, nodeport),
    --
    -- The EXCLUDE constraint only allows the same sysidentifier for all the
    -- nodes in the same group. The system_identifier is a property that is
    -- kept when implementing streaming replication and should be unique per
    -- Postgres instance in all other cases.
    --
    -- We allow the sysidentifier column to be NULL when registering a new
    -- primary server from scratch, because we have not done pg_ctl initdb
    -- at the time we call the register_node() function.
    --
    CONSTRAINT system_identifier_is_null_at_init_only
         CHECK (
                  (
                       sysidentifier IS NULL
                   AND reportedstate
                       IN (
                           'init',
                           'wait_standby',
                           'catchingup',
                           'dropped'
                          )
                   )
                OR sysidentifier IS NOT NULL
               ),

    CONSTRAINT same_system_identifier_within_group
       EXCLUDE USING gist(clusterid with =,
                          groupid with =,
                          sysidentifier with <>)
    DEFERRABLE INITIALLY DEFERRED,

    PRIMARY KEY (nodeid),
    FOREIGN KEY (clusterid) REFERENCES pg_ram.cluster(clusterid)
 )
 -- we expect few rows and lots of UPDATE, let's benefit from HOT
 WITH (fillfactor = 25);

ALTER SEQUENCE pg_ram.node_nodeid_seq OWNED BY pg_ram.node.nodeid;

INSERT INTO pg_ram.node
 (
  clusterid, nodeid, groupid, nodename, nodehost, nodeport, sysidentifier,
  goalstate, reportedstate, reportedpgisrunning, reportedrepstate,
  reporttime, reportedtli, reportedlsn, walreporttime,
  health, healthchecktime, statechangetime,
  candidatepriority, replicationquorum, nodecluster
 )
 SELECT clusterid, nodeid, groupid,
        nodename, nodehost, nodeport, sysidentifier,
        goalstate::text::pg_ram.replication_state,
        reportedstate::text::pg_ram.replication_state,
        reportedpgisrunning, reportedrepstate, reporttime,
        1 as reportedtli,
        reportedlsn, walreporttime, health, healthchecktime, statechangetime,
        candidatepriority, replicationquorum, nodecluster
   FROM pg_ram.node_upgrade_old;


ALTER TABLE pg_ram.event
	RENAME TO event_upgrade_old;

CREATE TABLE pg_ram.event
 (
    eventid           bigint not null DEFAULT nextval('pg_ram.event_eventid_seq'::regclass),
    eventtime         timestamptz not null default now(),
    clusterid       text not null,
    nodeid            bigint not null,
    groupid           int not null,
    nodename          text not null,
    nodehost          text not null,
    nodeport          integer not null,
    reportedstate     pg_ram.replication_state not null,
    goalstate         pg_ram.replication_state not null,
    reportedrepstate  text,
    reportedtli       int not null default 1 check (reportedtli > 0),
    reportedlsn       pg_lsn not null default '0/0',
    candidatepriority int,
    replicationquorum bool,
    description       text,

    PRIMARY KEY (eventid)
 );

ALTER SEQUENCE pg_ram.event_eventid_seq
      OWNED BY pg_ram.event.eventid;

INSERT INTO pg_ram.event
 (
  eventid, eventtime, clusterid, nodeid, groupid,
  nodename, nodehost, nodeport,
  reportedstate, goalstate, reportedrepstate,
  reportedtli, reportedlsn, candidatepriority, replicationquorum,
  description
 )
 SELECT eventid, eventtime, clusterid, nodeid, groupid,
        nodename, nodehost, nodeport,
        reportedstate, goalstate, reportedrepstate,
        1 as reportedtli, reportedlsn, candidatepriority, replicationquorum,
        description
   FROM pg_ram.event_upgrade_old as event;

DROP TABLE pg_ram.event_upgrade_old;
DROP TABLE pg_ram.node_upgrade_old;
DROP TYPE pg_ram.old_replication_state;

GRANT SELECT ON ALL TABLES IN SCHEMA pg_ram TO autoctl_node;



CREATE FUNCTION pg_ram.register_node
 (
    IN cluster_id         text,
    IN node_host            text,
    IN node_port            int,
    IN dbname               name,
    IN node_name            text default '',
    IN sysidentifier        bigint default 0,
    IN desired_node_id      bigint default -1,
    IN desired_group_id     int default -1,
    IN initial_group_role   pg_ram.replication_state default 'init',
    IN node_kind            text default 'standalone',
    IN candidate_priority 	int default 100,
    IN replication_quorum	bool default true,
    IN node_cluster         text default 'default',
   OUT assigned_node_id     bigint,
   OUT assigned_group_id    int,
   OUT assigned_group_state pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool,
   OUT assigned_node_name   text
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$register_node$$;

grant execute on function
      pg_ram.register_node(text,text,int,name,text,bigint,bigint,int,
                                   pg_ram.replication_state,text,
                                   int,bool,text)
   to autoctl_node;


CREATE FUNCTION pg_ram.node_active
 (
    IN cluster_id           		text,
    IN node_id        		        bigint,
    IN group_id       		        int,
    IN current_group_role     		pg_ram.replication_state default 'init',
    IN current_pg_is_running  		bool default true,
    IN current_tli			  		integer default 1,
    IN current_lsn			  		pg_lsn default '0/0',
    IN current_rep_state      		text default '',
   OUT assigned_node_id       		bigint,
   OUT assigned_group_id      		int,
   OUT assigned_group_state   		pg_ram.replication_state,
   OUT assigned_candidate_priority 	int,
   OUT assigned_replication_quorum  bool
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$node_active$$;

grant execute on function
      pg_ram.node_active(text,bigint,int,
                          pg_ram.replication_state,bool,int,pg_lsn,text)
   to autoctl_node;


DROP FUNCTION pg_ram.get_nodes(text,int);

CREATE FUNCTION pg_ram.get_nodes
 (
    IN cluster_id     text default 'default',
    IN group_id         int default NULL,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C
AS 'MODULE_PATHNAME', $$get_nodes$$;

comment on function pg_ram.get_nodes(text,int)
        is 'get all the nodes in a group';

grant execute on function pg_ram.get_nodes(text,int)
   to autoctl_node;


DROP FUNCTION pg_ram.get_primary(text,int);

CREATE FUNCTION pg_ram.get_primary
 (
    IN cluster_id      text default 'default',
    IN group_id          int default 0,
   OUT primary_node_id   bigint,
   OUT primary_name      text,
   OUT primary_host      text,
   OUT primary_port      int
 )
RETURNS record LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$get_primary$$;

comment on function pg_ram.get_primary(text,int)
        is 'get the writable node for a group';

grant execute on function pg_ram.get_primary(text,int)
   to autoctl_node;


CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN nodeid           bigint,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes(bigint)
        is 'get the other nodes in a group';

grant execute on function pg_ram.get_other_nodes(bigint)
   to autoctl_node;

CREATE FUNCTION pg_ram.get_other_nodes
 (
    IN nodeid           bigint,
    IN current_state    pg_ram.replication_state,
   OUT node_id          bigint,
   OUT node_name        text,
   OUT node_host        text,
   OUT node_port        int,
   OUT node_lsn         pg_lsn,
   OUT node_is_primary  bool
 )
RETURNS SETOF record LANGUAGE C STRICT
AS 'MODULE_PATHNAME', $$get_other_nodes$$;

comment on function pg_ram.get_other_nodes
                    (bigint,pg_ram.replication_state)
        is 'get the other nodes in a group, filtering on current_state';

grant execute on function pg_ram.get_other_nodes
                          (bigint,pg_ram.replication_state)
   to autoctl_node;


DROP FUNCTION pg_ram.remove_node(int);

CREATE FUNCTION pg_ram.remove_node
 (
   node_id bigint,
   force   bool default 'false'
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$remove_node_by_nodeid$$;

comment on function pg_ram.remove_node(bigint,bool)
        is 'remove a node from the monitor';

grant execute on function pg_ram.remove_node(bigint,bool)
   to autoctl_node;

DROP FUNCTION pg_ram.remove_node(text,int);

CREATE FUNCTION pg_ram.remove_node
 (
   node_host text,
   node_port int default 5432,
   force     bool default 'false'
 )
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$remove_node_by_host$$;

comment on function pg_ram.remove_node(text,int,bool)
        is 'remove a node from the monitor';

grant execute on function pg_ram.remove_node(text,int,bool)
   to autoctl_node;

DROP FUNCTION pg_ram.start_maintenance(node_id int);

CREATE FUNCTION pg_ram.start_maintenance(node_id bigint)
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$start_maintenance$$;

comment on function pg_ram.start_maintenance(bigint)
        is 'set a node in maintenance state';

grant execute on function pg_ram.start_maintenance(bigint)
   to autoctl_node;

DROP FUNCTION pg_ram.stop_maintenance(node_id int);

CREATE FUNCTION pg_ram.stop_maintenance(node_id bigint)
RETURNS bool LANGUAGE C STRICT SECURITY DEFINER
AS 'MODULE_PATHNAME', $$stop_maintenance$$;

comment on function pg_ram.stop_maintenance(bigint)
        is 'set a node out of maintenance state';

grant execute on function pg_ram.stop_maintenance(bigint)
   to autoctl_node;


CREATE OR REPLACE FUNCTION pg_ram.update_secondary_check()
  RETURNS trigger
  LANGUAGE 'plpgsql'
AS $$
declare
  nodeid        bigint := null;
  reportedstate pg_ram.replication_state := null;
begin
	-- when secondary changes from true to false, check all nodes remaining are primary
	if     new.opt_secondary is false
	   and new.opt_secondary is distinct from old.opt_secondary
	then
		select node.nodeid, node.reportedstate
		  into nodeid, reportedstate
		  from pg_ram.node
		 where node.clusterid = new.clusterid
		   and node.reportedstate <> 'single'
           and node.goalstate <> 'dropped';

		if nodeid is not null
		then
		    raise exception object_not_in_prerequisite_state
		      using
		        message = 'cluster has nodes that are not in SINGLE state',
		         detail = 'nodeid ' || nodeid || ' is in state ' || reportedstate,
		           hint = 'drop secondary nodes before disabling secondaries on cluster';
		end if;
	end if;

    return new;
end
$$;

comment on function pg_ram.update_secondary_check()
        is 'performs a check when changes to hassecondary on pg_ram.cluster are made, verifying cluster state allows the change';

CREATE TRIGGER disable_secondary_check
	BEFORE UPDATE
	ON pg_ram.cluster
	FOR EACH ROW
	EXECUTE PROCEDURE pg_ram.update_secondary_check();


CREATE FUNCTION pg_ram.last_events
 (
  count int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
  select eventid, eventtime, clusterid,
         nodeid, groupid, nodename, nodehost, nodeport,
         reportedstate, goalstate,
         reportedrepstate, reportedtli, reportedlsn,
         candidatepriority, replicationquorum, description
    from pg_ram.event
order by eventid desc
   limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(int)
        is 'retrieve last COUNT events';

grant execute on function pg_ram.last_events(int)
   to autoctl_node;

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text default 'default',
  count        int  default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodehost, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedtli, reportedlsn,
           candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int)
        is 'retrieve last COUNT events for given cluster';

grant execute on function pg_ram.last_events(text,int)
   to autoctl_node;

CREATE FUNCTION pg_ram.last_events
 (
  cluster_id text,
  group_id     int,
  count        int default 10
 )
RETURNS SETOF pg_ram.event LANGUAGE SQL STRICT
AS $$
with last_events as
(
    select eventid, eventtime, clusterid,
           nodeid, groupid, nodename, nodehost, nodeport,
           reportedstate, goalstate,
           reportedrepstate, reportedtli, reportedlsn,
           candidatepriority, replicationquorum, description
      from pg_ram.event
     where clusterid = cluster_id
       and groupid = group_id
  order by eventid desc
     limit count
)
select * from last_events order by eventtime, eventid;
$$;

comment on function pg_ram.last_events(text,int,int)
        is 'retrieve last COUNT events for given cluster and group';

grant execute on function pg_ram.last_events(text,int,int)
   to autoctl_node;


CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text default 'default',
   OUT cluster_kind       text,
   OUT nodename             text,
   OUT nodehost             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool,
   OUT reported_tli         int,
   OUT reported_lsn         pg_lsn,
   OUT health               integer,
   OUT nodecluster          text
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select kind, nodename, nodehost, nodeport, groupid, nodeid,
          reportedstate, goalstate,
   		  candidatepriority, replicationquorum,
          reportedtli, reportedlsn, health, nodecluster
     from pg_ram.node
     join pg_ram.cluster using(clusterid)
    where clusterid = cluster_id
 order by groupid, nodeid;
$$;

comment on function pg_ram.current_state(text)
        is 'get the current state of both nodes of a cluster';

grant execute on function pg_ram.current_state(text)
   to autoctl_node;

CREATE FUNCTION pg_ram.current_state
 (
    IN cluster_id         text,
    IN group_id             int,
   OUT cluster_kind       text,
   OUT nodename             text,
   OUT nodehost             text,
   OUT nodeport             int,
   OUT group_id             int,
   OUT node_id              bigint,
   OUT current_group_state  pg_ram.replication_state,
   OUT assigned_group_state pg_ram.replication_state,
   OUT candidate_priority	int,
   OUT replication_quorum	bool,
   OUT reported_tli         int,
   OUT reported_lsn         pg_lsn,
   OUT health               integer,
   OUT nodecluster          text
 )
RETURNS SETOF record LANGUAGE SQL STRICT
AS $$
   select kind, nodename, nodehost, nodeport, groupid, nodeid,
          reportedstate, goalstate,
   		  candidatepriority, replicationquorum,
          reportedtli, reportedlsn, health, nodecluster
     from pg_ram.node
     join pg_ram.cluster using(clusterid)
    where clusterid = cluster_id
      and groupid = group_id
 order by groupid, nodeid;
$$;

grant execute on function pg_ram.current_state(text, int)
   to autoctl_node;

comment on function pg_ram.current_state(text, int)
        is 'get the current state of both nodes of a group in a cluster';
--
-- extension update file from 1.6 to 2.0
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ram" to load this file. \quit

-- no changes, just the version number
--
-- extension update file from 2.0 to 2.1
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ram" to load this file. \quit

-- no changes, just the version number
--
-- extension update file from 2.1 to 2.2
--
-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ram" to load this file. \quit

-- no changes, just the version number
