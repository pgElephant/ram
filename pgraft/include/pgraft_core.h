#ifndef PGRAFT_CORE_H
#define PGRAFT_CORE_H

#include "postgres.h"
#include "storage/shmem.h"
#include "storage/spin.h"

/* Core consensus types */
typedef struct pgraft_node
{
	int32_t		id;
	char		address[256];
	int32_t		port;
	bool		is_leader;
}			pgraft_node_t;

typedef struct pgraft_cluster
{
	bool		initialized;	/* Whether the core system is initialized */
	int32_t		node_id;
	int32_t		current_term;
	int64_t		leader_id;
	char		state[32];		/* "leader", "follower", "candidate" */
	int32_t		num_nodes;
	pgraft_node_t nodes[16];
	
	/* Performance metrics */
	int64_t		messages_processed;
	int64_t		heartbeats_sent;
	int64_t		elections_triggered;
	
	/* Mutex for thread safety */
	slock_t		mutex;
}			pgraft_cluster_t;

/* Core consensus functions */
int			pgraft_core_init(int32_t node_id, const char *address, int32_t port);
int			pgraft_core_add_node(int32_t node_id, const char *address, int32_t port);
int			pgraft_core_remove_node(int32_t node_id);
int			pgraft_core_get_cluster_state(pgraft_cluster_t *cluster);
int			pgraft_core_update_cluster_state(int64_t leader_id, int64_t current_term, const char *state);
bool		pgraft_core_is_leader(void);
int64_t		pgraft_core_get_leader_id(void);
int32_t		pgraft_core_get_current_term(void);
void		pgraft_core_cleanup(void);

/* Shared memory functions */
void		pgraft_core_init_shared_memory(void);
pgraft_cluster_t *pgraft_core_get_shared_memory(void);

#endif
