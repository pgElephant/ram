/*-------------------------------------------------------------------------
 *
 * pgram_logical_slots.c
 *		Logical replication slot management for pg_ram
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "access/xlog.h"
#include "access/xlogdefs.h"
#include "replication/slot.h"
#include "replication/logical.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "catalog/pg_database.h"
#include "storage/procarray.h"
#include "pgram_logical_slots.h"

PG_FUNCTION_INFO_V1(pgram_logical_slot_create_sql);
PG_FUNCTION_INFO_V1(pgram_logical_slot_drop_sql);
PG_FUNCTION_INFO_V1(pgram_logical_slot_advance_sql);
PG_FUNCTION_INFO_V1(pgram_logical_slot_info_sql);
PG_FUNCTION_INFO_V1(pgram_logical_slots_list_sql);
PG_FUNCTION_INFO_V1(pgram_logical_slot_failover_prepare_sql);

bool pgram_logical_slot_create(const char* slot_name, const char* plugin,
                               const char* database, bool temporary,
                               bool two_phase)
{
	Oid dbid;

	if (!slot_name || !plugin)
	{
		elog(WARNING, "pg_ram: slot name and plugin are required");
		return false;
	}

	/* Get database OID if specified */
	if (database && strlen(database) > 0)
	{
		dbid = get_database_oid(database, false);
		if (!OidIsValid(dbid))
		{
			elog(WARNING, "pg_ram: database '%s' not found", database);
			return false;
		}
	}
	else
	{
		dbid = MyDatabaseId;
	}

	/* Check if we're connected to the right database */
	if (dbid != MyDatabaseId)
	{
		elog(WARNING,
		     "pg_ram: must be connected to database '%s' to create slot",
		     database);
		return false;
	}

	PG_TRY();
	{
		/* Create the logical slot */
		ReplicationSlotCreate(slot_name, true, RS_PERSISTENT, two_phase);

		/* Initialize logical decoding */
		LogicalDecodingContext* ctx;
		ctx = CreateInitDecodingContext(plugin, NIL, false, InvalidXLogRecPtr,
		                                XL_ROUTINE, NULL, NULL, NULL, NULL);
		FreeDecodingContext(ctx);

		ReplicationSlotRelease();

		elog(LOG, "pg_ram: created logical slot '%s' with plugin '%s'",
		     slot_name, plugin);
		return true;
	}
	PG_CATCH();
	{
		ErrorData* edata = CopyErrorData();
		FlushErrorState();

		elog(WARNING, "pg_ram: failed to create logical slot '%s': %s",
		     slot_name, edata->message);
		FreeErrorData(edata);
		return false;
	}
	PG_END_TRY();
}

bool pgram_logical_slot_drop(const char* slot_name, bool force)
{
	ReplicationSlot* slot;

	if (!slot_name)
	{
		elog(WARNING, "pg_ram: slot name is required");
		return false;
	}

	PG_TRY();
	{
		/* Find and acquire the slot */
		slot = SearchNamedReplicationSlot(slot_name, true);
		if (!slot)
		{
			elog(WARNING, "pg_ram: logical slot '%s' does not exist",
			     slot_name);
			return false;
		}

		/* Check if slot is active */
		SpinLockAcquire(&slot->mutex);
		bool active = slot->active_pid != 0;
		SpinLockRelease(&slot->mutex);

		if (active && !force)
		{
			elog(WARNING,
			     "pg_ram: logical slot '%s' is active, use force=true to drop",
			     slot_name);
			return false;
		}

		/* Drop the slot */
		ReplicationSlotAcquire(slot_name, true);
		ReplicationSlotDropAcquired();

		elog(LOG, "pg_ram: dropped logical slot '%s'", slot_name);
		return true;
	}
	PG_CATCH();
	{
		ErrorData* edata = CopyErrorData();
		FlushErrorState();

		elog(WARNING, "pg_ram: failed to drop logical slot '%s': %s", slot_name,
		     edata->message);
		FreeErrorData(edata);
		return false;
	}
	PG_END_TRY();
}

bool pgram_logical_slot_advance(const char* slot_name, XLogRecPtr target_lsn)
{
	if (!slot_name)
	{
		elog(WARNING, "pg_ram: slot name is required");
		return false;
	}

	PG_TRY();
	{
		/* Acquire the slot */
		ReplicationSlotAcquire(slot_name, true);

		/* Advance the slot */
		LogicalSlotAdvanceAndCheckSnapState(target_lsn, NULL);

		ReplicationSlotMarkDirty();
		ReplicationSlotSave();
		ReplicationSlotRelease();

		elog(LOG, "pg_ram: advanced logical slot '%s' to %X/%X", slot_name,
		     LSN_FORMAT_ARGS(target_lsn));
		return true;
	}
	PG_CATCH();
	{
		ErrorData* edata = CopyErrorData();
		FlushErrorState();

		elog(WARNING, "pg_ram: failed to advance logical slot '%s': %s",
		     slot_name, edata->message);
		FreeErrorData(edata);
		return false;
	}
	PG_END_TRY();
}

bool pgram_logical_slot_failover_prepare(void)
{
	int i;
	int active_slots = 0;

	elog(LOG, "pg_ram: preparing logical slots for failover");

	/* Check all logical slots and prepare for failover */
	LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationSlot* slot = &ReplicationSlotCtl->replication_slots[i];

		/* Skip if slot is not in use */
		if (!slot->in_use)
			continue;

		/* Skip physical slots */
		if (slot->data.database == InvalidOid)
			continue;

		SpinLockAcquire(&slot->mutex);
		bool active = slot->active_pid != 0;
		SpinLockRelease(&slot->mutex);

		if (active)
		{
			active_slots++;
			elog(LOG,
			     "pg_ram: logical slot '%s' is active, may need attention "
			     "during failover",
			     NameStr(slot->data.name));
		}
	}

	LWLockRelease(ReplicationSlotControlLock);

	elog(LOG, "pg_ram: found %d active logical slots for failover preparation",
	     active_slots);
	return true;
}

bool pgram_logical_slot_failover_complete(void)
{
	elog(LOG, "pg_ram: completing logical slot failover");

	/* After failover, logical slots should be ready on the new primary */
	/* Any additional cleanup or verification can be added here */

	return true;
}

int32 pgram_logical_slot_count(void)
{
	int i;
	int count = 0;

	LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

	for (i = 0; i < max_replication_slots; i++)
	{
		ReplicationSlot* slot = &ReplicationSlotCtl->replication_slots[i];

		if (slot->in_use && slot->data.database != InvalidOid)
			count++;
	}

	LWLockRelease(ReplicationSlotControlLock);

	return count;
}

bool pgram_logical_slot_get_info(const char* slot_name,
                                 pgram_logical_slot_info_t* info)
{
	ReplicationSlot* slot;

	if (!slot_name || !info)
		return false;

	slot = SearchNamedReplicationSlot(slot_name, false);
	if (!slot)
		return false;

	/* Fill in slot information */
	strlcpy(info->slot_name, NameStr(slot->data.name), sizeof(info->slot_name));
	strlcpy(info->plugin, NameStr(slot->data.plugin), sizeof(info->plugin));

	/* Get database name */
	if (slot->data.database != InvalidOid)
	{
		char* dbname = get_database_name(slot->data.database);
		if (dbname)
		{
			strlcpy(info->database, dbname, sizeof(info->database));
			pfree(dbname);
		}
		else
		{
			strlcpy(info->database, "unknown", sizeof(info->database));
		}
	}
	else
	{
		strlcpy(info->database, "", sizeof(info->database));
	}

	SpinLockAcquire(&slot->mutex);
	info->active = slot->active_pid != 0;
	info->restart_lsn = slot->data.restart_lsn;
	info->confirmed_flush_lsn = slot->data.confirmed_flush;
	info->two_phase = slot->data.two_phase;
	SpinLockRelease(&slot->mutex);

	info->temporary = false;    /* We only create persistent slots */
	info->wal_status = 0;       /* Would need additional logic to determine */
	info->safe_wal_size = true; /* Simplified for now */

	return true;
}

/* SQL Interface Functions */

Datum pgram_logical_slot_create_sql(PG_FUNCTION_ARGS)
{
	text* slot_name_text = PG_GETARG_TEXT_P(0);
	text* plugin_text = PG_GETARG_TEXT_P(1);
	text* database_text = PG_ARGISNULL(2) ? NULL : PG_GETARG_TEXT_P(2);
	bool temporary = PG_GETARG_BOOL(3);
	bool two_phase = PG_GETARG_BOOL(4);

	char* slot_name = text_to_cstring(slot_name_text);
	char* plugin = text_to_cstring(plugin_text);
	char* database = database_text ? text_to_cstring(database_text) : NULL;

	bool result = pgram_logical_slot_create(slot_name, plugin, database,
	                                        temporary, two_phase);

	pfree(slot_name);
	pfree(plugin);
	if (database)
		pfree(database);

	PG_RETURN_BOOL(result);
}

Datum pgram_logical_slot_drop_sql(PG_FUNCTION_ARGS)
{
	text* slot_name_text = PG_GETARG_TEXT_P(0);
	bool force = PG_GETARG_BOOL(1);

	char* slot_name = text_to_cstring(slot_name_text);
	bool result = pgram_logical_slot_drop(slot_name, force);

	pfree(slot_name);
	PG_RETURN_BOOL(result);
}

Datum pgram_logical_slot_advance_sql(PG_FUNCTION_ARGS)
{
	text* slot_name_text = PG_GETARG_TEXT_P(0);
	XLogRecPtr target_lsn = PG_GETARG_LSN(1);

	char* slot_name = text_to_cstring(slot_name_text);
	bool result = pgram_logical_slot_advance(slot_name, target_lsn);

	pfree(slot_name);
	PG_RETURN_BOOL(result);
}

Datum pgram_logical_slot_info_sql(PG_FUNCTION_ARGS)
{
	text* slot_name_text = PG_GETARG_TEXT_P(0);
	char* slot_name = text_to_cstring(slot_name_text);

	pgram_logical_slot_info_t info;
	bool found = pgram_logical_slot_get_info(slot_name, &info);

	pfree(slot_name);

	if (!found)
		PG_RETURN_NULL();

	/* Build result tuple */
	TupleDesc tupdesc;
	Datum values[9];
	bool nulls[9] = {false};
	HeapTuple tuple;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		ereport(ERROR, (errmsg("function returning record called in context "
		                       "that cannot accept a record")));

	values[0] = CStringGetTextDatum(info.slot_name);
	values[1] = CStringGetTextDatum(info.plugin);
	values[2] = CStringGetTextDatum(info.database);
	values[3] = BoolGetDatum(info.active);
	values[4] = LSNGetDatum(info.restart_lsn);
	values[5] = LSNGetDatum(info.confirmed_flush_lsn);
	values[6] = BoolGetDatum(info.temporary);
	values[7] = BoolGetDatum(info.safe_wal_size);
	values[8] = BoolGetDatum(info.two_phase);

	tuple = heap_form_tuple(tupdesc, values, nulls);
	PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
}

Datum pgram_logical_slots_list_sql(PG_FUNCTION_ARGS)
{
	FuncCallContext* funcctx;
	int call_cntr;
	int max_calls;
	MemoryContext oldcontext;

	if (SRF_IS_FIRSTCALL())
	{
		funcctx = SRF_FIRSTCALL_INIT();
		oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		/* Count logical slots */
		max_calls = pgram_logical_slot_count();
		funcctx->max_calls = max_calls;

		/* Build tuple descriptor */
		TupleDesc tupdesc = CreateTemplateTupleDesc(9);
		TupleDescInitEntry(tupdesc, (AttrNumber) 1, "slot_name", TEXTOID, -1,
		                   0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 2, "plugin", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 3, "database", TEXTOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 4, "active", BOOLOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 5, "restart_lsn", LSNOID, -1,
		                   0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 6, "confirmed_flush_lsn",
		                   LSNOID, -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 7, "temporary", BOOLOID, -1,
		                   0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 8, "safe_wal_size", BOOLOID,
		                   -1, 0);
		TupleDescInitEntry(tupdesc, (AttrNumber) 9, "two_phase", BOOLOID, -1,
		                   0);

		funcctx->tuple_desc = BlessTupleDesc(tupdesc);
		funcctx->user_fctx = (void*) 0; /* slot index */

		MemoryContextSwitchTo(oldcontext);
	}

	funcctx = SRF_PERCALL_SETUP();
	call_cntr = funcctx->call_cntr;
	max_calls = funcctx->max_calls;

	if (call_cntr < max_calls)
	{
		/* Find next logical slot */
		int slot_index = (int) (intptr_t) funcctx->user_fctx;
		ReplicationSlot* slot = NULL;

		LWLockAcquire(ReplicationSlotControlLock, LW_SHARED);

		for (int i = slot_index; i < max_replication_slots; i++)
		{
			ReplicationSlot* candidate =
			    &ReplicationSlotCtl->replication_slots[i];

			if (candidate->in_use && candidate->data.database != InvalidOid)
			{
				slot = candidate;
				funcctx->user_fctx = (void*) (intptr_t) (i + 1);
				break;
			}
		}

		if (slot)
		{
			Datum values[9];
			bool nulls[9] = {false};
			HeapTuple tuple;

			/* Extract slot information */
			pgram_logical_slot_info_t info;
			pgram_logical_slot_get_info(NameStr(slot->data.name), &info);

			values[0] = CStringGetTextDatum(info.slot_name);
			values[1] = CStringGetTextDatum(info.plugin);
			values[2] = CStringGetTextDatum(info.database);
			values[3] = BoolGetDatum(info.active);
			values[4] = LSNGetDatum(info.restart_lsn);
			values[5] = LSNGetDatum(info.confirmed_flush_lsn);
			values[6] = BoolGetDatum(info.temporary);
			values[7] = BoolGetDatum(info.safe_wal_size);
			values[8] = BoolGetDatum(info.two_phase);

			tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);

			LWLockRelease(ReplicationSlotControlLock);
			SRF_RETURN_NEXT(funcctx, HeapTupleGetDatum(tuple));
		}

		LWLockRelease(ReplicationSlotControlLock);
	}

	SRF_RETURN_DONE(funcctx);
}

Datum pgram_logical_slot_failover_prepare_sql(PG_FUNCTION_ARGS)
{
	bool result = pgram_logical_slot_failover_prepare();
	PG_RETURN_BOOL(result);
}
