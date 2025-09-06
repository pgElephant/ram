/*-------------------------------------------------------------------------
 *
 * pgram_logical_slots.h
 *		Logical replication slot management for pg_ram
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef PGRAM_LOGICAL_SLOTS_H
#define PGRAM_LOGICAL_SLOTS_H

#include "postgres.h"
#include "fmgr.h"
#include "replication/slot.h"

/* Logical slot information structure */
typedef struct pgram_logical_slot_info
{
	char slot_name[NAMEDATALEN];
	char plugin[NAMEDATALEN];
	char database[NAMEDATALEN];
	bool temporary;
	bool active;
	XLogRecPtr restart_lsn;
	XLogRecPtr confirmed_flush_lsn;
	int32 wal_status;
	bool safe_wal_size;
	int64 two_phase;
} pgram_logical_slot_info_t;

/* Function prototypes */
extern bool pgram_logical_slot_create(const char* slot_name, const char* plugin,
                                      const char* database, bool temporary,
                                      bool two_phase);
extern bool pgram_logical_slot_drop(const char* slot_name, bool force);
extern bool pgram_logical_slot_advance(const char* slot_name,
                                       XLogRecPtr target_lsn);
extern bool pgram_logical_slot_failover_prepare(void);
extern bool pgram_logical_slot_failover_complete(void);
extern int32 pgram_logical_slot_count(void);
extern bool pgram_logical_slot_get_info(const char* slot_name,
                                        pgram_logical_slot_info_t* info);
extern bool pgram_logical_slot_list_all(pgram_logical_slot_info_t** slots,
                                        int32* count);

/* SQL function prototypes */
extern Datum pgram_logical_slot_create_sql(PG_FUNCTION_ARGS);
extern Datum pgram_logical_slot_drop_sql(PG_FUNCTION_ARGS);
extern Datum pgram_logical_slot_advance_sql(PG_FUNCTION_ARGS);
extern Datum pgram_logical_slot_info_sql(PG_FUNCTION_ARGS);
extern Datum pgram_logical_slots_list_sql(PG_FUNCTION_ARGS);
extern Datum pgram_logical_slot_failover_prepare_sql(PG_FUNCTION_ARGS);

#endif /* PGRAM_LOGICAL_SLOTS_H */
