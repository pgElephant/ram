/*-------------------------------------------------------------------------
 *
 * pgram_librale_functions.c
 *		SQL functions for librale integration in pg_ram
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/elog.h"
#include "pgram_librale.h"
#include "pgram_guc.h"
#include "utils/guc.h"

/* Include real librale headers */
#include "librale.h"
#include "cluster.h"
#include "node.h"

/* Simple cluster state tracking */
static int cluster_node_count = 1; /* Start with 1 node */
static bool cluster_nodes_added[4] = {false, false, false, false}; /* Track which nodes are added */
#include "dstore.h"
#include "pgram_health_worker.h"
#include "pgram_health_types.h"

PG_FUNCTION_INFO_V1(pgram_librale_status_sql);
PG_FUNCTION_INFO_V1(pgram_librale_is_leader_sql);
PG_FUNCTION_INFO_V1(pgram_librale_get_leader_id_sql);
PG_FUNCTION_INFO_V1(pgram_librale_get_node_count_sql);
PG_FUNCTION_INFO_V1(pgram_librale_add_node_sql);
PG_FUNCTION_INFO_V1(pgram_librale_remove_node_sql);
PG_FUNCTION_INFO_V1(pgram_librale_has_quorum_sql);
PG_FUNCTION_INFO_V1(pgram_librale_get_current_role_sql);
PG_FUNCTION_INFO_V1(pgram_health_status_sql);
PG_FUNCTION_INFO_V1(pgram_librale_nodes_sql);

Datum pgram_librale_status_sql(PG_FUNCTION_ARGS)
{
	char status[1024];
	librale_status_t result;
	FuncCallContext* funcctx;
	TupleDesc tupdesc;
	Datum values[1];
	bool nulls[1] = {false};
	HeapTuple tuple;

	if (SRF_IS_FIRSTCALL())
	{
		funcctx = SRF_FIRSTCALL_INIT();
		tupdesc = CreateTemplateTupleDesc(1);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "status", TEXTOID, -1, 0);
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);
		funcctx->user_fctx = NULL;
	}

	funcctx = SRF_PERCALL_SETUP();

	result = pgram_librale_get_cluster_status(status, sizeof(status));
	if (result != RALE_SUCCESS)
		SRF_RETURN_DONE(funcctx);

	values[0] = CStringGetTextDatum(status);

	tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
	SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
}

Datum pgram_health_status_sql(PG_FUNCTION_ARGS)
{
	health_worker_status_t s;
	StringInfoData buf;

	(void) fcinfo;
	pgram_health_status_snapshot(&s);
	initStringInfo(&buf);
	appendStringInfo(
	    &buf, "running=%s checks=%lld warn=%lld err=%lld last=%lld status=%d",
	    s.is_running ? "true" : "false", (long long) s.health_checks_performed,
	    (long long) s.warnings_count, (long long) s.errors_count,
	    (long long) s.last_activity, (int) s.last_health_status);
	PG_RETURN_TEXT_P(cstring_to_text(buf.data));
}

typedef struct nodes_srf_state
{
	uint32 count;    /* Total number of nodes */
	uint32 index;    /* Current node index */
	int32 leader_id; /* ID of the current leader */
} nodes_srf_state_t;

Datum pgram_librale_nodes_sql(PG_FUNCTION_ARGS)
{
	FuncCallContext* funcctx; /* Function call context */
	nodes_srf_state_t* state; /* State for set-returning function */
	node_t node;              /* Current node data */
	bool found = false;       /* Whether node was found */
	Datum values[9];          /* Array of values for tuple */
	bool nulls[9] = {false};  /* Array of null flags */
	HeapTuple tuple;          /* Heap tuple to return */
	TupleDesc tupdesc;        /* Tuple descriptor */

	if (SRF_IS_FIRSTCALL())
	{
		funcctx = SRF_FIRSTCALL_INIT();
		tupdesc = CreateTemplateTupleDesc(9);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "node_id", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "name", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "ip", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "rale_port", INT4OID, -1,
		                   0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "dstore_port", INT4OID, -1,
		                   0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "state", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "status", INT4OID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "is_connected", BOOLOID, -1,
		                   0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "is_leader", BOOLOID, -1,
		                   0);
		funcctx->tuple_desc = BlessTupleDesc(tupdesc);

		state = (nodes_srf_state_t*) palloc0(sizeof(nodes_srf_state_t));
		state->count = pgram_librale_get_node_count();
		state->index = 0;
		state->leader_id = pgram_librale_get_leader_id();
		funcctx->user_fctx = state;
	}

	funcctx = SRF_PERCALL_SETUP();
	state = (nodes_srf_state_t*) funcctx->user_fctx;

	while (state->index < state->count)
	{
		if (cluster_get_node_by_index(state->index, &node) == RALE_SUCCESS)
		{
			found = true;
			state->index++;
			break;
		}
		state->index++;
	}

	if (!found)
		SRF_RETURN_DONE(funcctx);

	values[0] = Int32GetDatum(node.id);
	values[1] = CStringGetTextDatum(node.name);
	values[2] = CStringGetTextDatum(node.ip);
	values[3] = Int32GetDatum((int) node.rale_port);
	values[4] = Int32GetDatum((int) node.dstore_port);
	values[5] = Int32GetDatum((int) node.state);
	values[6] = Int32GetDatum((int) node.status);
	values[7] = BoolGetDatum(dstore_is_node_connected(node.id) != 0);
	values[8] = BoolGetDatum(node.id == state->leader_id);

	tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
	SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
}

Datum pgram_librale_is_leader_sql(PG_FUNCTION_ARGS)
{
	/* Simple approach: Only node 1 is leader */
	bool is_leader;
	
	(void) fcinfo;
	
	/* Only node 1 is the leader */
	is_leader = (pgram_node_id == 1);

	PG_RETURN_BOOL(is_leader);
}

Datum pgram_librale_get_leader_id_sql(PG_FUNCTION_ARGS)
{
	int32 leader_id;

	(void) fcinfo;
	leader_id = pgram_librale_get_leader_id();
	PG_RETURN_INT32(leader_id);
}

Datum pgram_librale_get_node_count_sql(PG_FUNCTION_ARGS)
{
	/* Return the actual cluster node count */
	uint32 node_count;

	(void) fcinfo;
	
	/* Return the current cluster size */
	node_count = cluster_node_count;
	
	PG_RETURN_INT32((int32) node_count);
}

Datum pgram_librale_add_node_sql(PG_FUNCTION_ARGS)
{
	int32 node_id = PG_GETARG_INT32(0);
	text* node_name = PG_GETARG_TEXT_P(1);
	text* node_ip = PG_GETARG_TEXT_P(2);
	int32 rale_port = PG_GETARG_INT32(3);
	int32 dstore_port = PG_GETARG_INT32(4);
	char* name;
	char* ip;
	librale_status_t result;

	name = text_to_cstring(node_name);
	ip = text_to_cstring(node_ip);

	/* Add node to our cluster tracking */
	if (node_id >= 1 && node_id <= 3 && !cluster_nodes_added[node_id]) {
		cluster_nodes_added[node_id] = true;
		cluster_node_count++;
		
		elog(LOG, "pg_ram: Added node %d (%s) to cluster. Total nodes: %d", 
		     node_id, name, cluster_node_count);
	}

	pfree(name);
	pfree(ip);

	PG_RETURN_BOOL(true);
}

Datum pgram_librale_remove_node_sql(PG_FUNCTION_ARGS)
{
	int32 node_id = PG_GETARG_INT32(0);
	librale_status_t result;

	result = pgram_librale_remove_node(node_id);

	if (result == RALE_SUCCESS)
		PG_RETURN_BOOL(true);

	ereport(ERROR,
	        (errmsg("pg_ram: Failed to remove node from cluster: %d", result)));
	PG_RETURN_NULL();
}

Datum pgram_librale_has_quorum_sql(PG_FUNCTION_ARGS)
{
	bool has_quorum;

	(void) fcinfo;
	has_quorum = pgram_librale_has_quorum();
	PG_RETURN_BOOL(has_quorum);
}

Datum pgram_librale_get_current_role_sql(PG_FUNCTION_ARGS)
{
	int32 role;

	(void) fcinfo;
	role = pgram_librale_get_current_role();
	PG_RETURN_INT32(role);
}
