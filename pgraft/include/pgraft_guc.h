#ifndef PGRAFT_GUC_H
#define PGRAFT_GUC_H

#include "postgres.h"

/* GUC variables */
extern char	   *pgraft_config_file;
extern int		pgraft_node_id;
extern int		pgraft_port;
extern char	   *pgraft_address;
extern char	   *pgraft_cluster_name;
extern int		pgraft_heartbeat_interval;
extern int		pgraft_election_timeout;
extern int		pgraft_max_log_entries;
extern bool		pgraft_debug_enabled;
extern int		pgraft_network_timeout;
extern int		pgraft_replication_timeout;
extern bool		pgraft_auto_recovery;
extern int		pgraft_snapshot_interval;

/* GUC functions */
void		pgraft_guc_init(void);
void		pgraft_guc_shutdown(void);
void		pgraft_register_guc_variables(void);
void		pgraft_validate_configuration(void);

#endif
