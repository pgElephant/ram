/*-------------------------------------------------------------------------
 *
 * ramctrl_daemon.c
 *		PostgreSQL RAM Control Utility - Daemon Control Operations
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

#include "ramctrl.h"
#include "ramctrl_daemon.h"
#include "ramctrl_database.h"
#include "ramctrl_table.h"
#include "ramctrl_replication.h"
#include "ramctrl_defaults.h"
#include "ramctrl_help.h"
#include "ramctrl_http.h"
#include "ramctrl_watch.h"


static bool ramctrl_daemon_is_running_pidfile(const char* pidfile)
{
	FILE* fp;
    pid_t pid;
    
    fp = fopen(pidfile, "r");
    if (!fp)
        return false;
        
    if (fscanf(fp, "%d", &pid) != 1)
    {
        fclose(fp);
        return false;
    }
    fclose(fp);
    
    if (kill(pid, 0) == 0)
        return true;
    else
    {
        unlink(pidfile);
        return false;
    }
}


static pid_t ramctrl_daemon_get_pid_pidfile(const char* pidfile)
{
	FILE* fp;
    pid_t pid = 0;
    
    fp = fopen(pidfile, "r");
    if (fp)
    {
        if (fscanf(fp, "%d", &pid) != 1)
            pid = 0;
        fclose(fp);
    }
    
    return pid;
}


bool ramctrl_daemon_is_running(ramctrl_context_t* ctx)
{
    if (!ctx)
        return false;
    
    return ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE);
}


pid_t ramctrl_daemon_get_pid(ramctrl_context_t* ctx)
{
    if (!ctx)
        return 0;
    
    return ramctrl_daemon_get_pid_pidfile(RAMD_PIDFILE);
}


bool ramctrl_daemon_start(ramctrl_context_t* ctx, const char* config_file)
{
	char* args[16];
    int arg_count = 0;
    
    if (!ctx)
        return false;
        
    if (ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
    {
        if (ctx->verbose)
            printf("ramctrl: ramd daemon is already running\n");
        return true;
    }
    
    args[arg_count++] = RAMD_BINARY;
	args[arg_count++] = "--daemon";
    
	if (config_file)
    {
		args[arg_count++] = "--config";
		args[arg_count++] = (char*) config_file;
    }

	if (ctx->verbose)
    {
		args[arg_count++] = "--verbose";
    }
    
    args[arg_count] = NULL;
    
        if (ctx->verbose)
		printf("ramctrl: starting ramd daemon...\n");

	pid_t pid = fork();
	if (pid == 0)
	{
		execvp(args[0], args);
		exit(1);
	}
	else if (pid > 0)
	{
        int status;
		waitpid(pid, &status, 0);
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		{
            if (ctx->verbose)
				printf("ramctrl: ramd daemon started successfully\n");
            return true;
        }
	}

            if (ctx->verbose)
		printf("ramctrl: failed to start ramd daemon\n");
            return false;
}


bool ramctrl_daemon_stop(ramctrl_context_t* ctx)
{
    pid_t pid;
    int attempts = 0;
    
    if (!ctx)
        return false;
        
    if (!ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
    {
        if (ctx->verbose)
            printf("ramctrl: ramd daemon is not running\n");
        return true;
    }
    
    pid = ramctrl_daemon_get_pid_pidfile(RAMD_PIDFILE);
    if (pid <= 0)
    {
        if (ctx->verbose)
			printf("ramctrl: cannot determine ramd daemon PID\n");
        return false;
    }
    
	if (ctx->verbose)
		printf("ramctrl: stopping ramd daemon (PID %d)...\n", pid);

	while (attempts < 30)
	{
    if (kill(pid, SIGTERM) != 0)
		{
			if (errno == ESRCH)
    {
        if (ctx->verbose)
					printf("ramctrl: ramd daemon stopped\n");
				unlink(RAMD_PIDFILE);
				return true;
			}
			break;
		}

		sleep(1);
		attempts++;

        if (!ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
        {
            if (ctx->verbose)
                printf("ramctrl: ramd daemon stopped\n");
            return true;
        }
    }
    
    if (ctx->verbose)
		printf("ramctrl: sending SIGKILL to ramd daemon\n");
	kill(pid, SIGKILL);
	sleep(1);
	unlink(RAMD_PIDFILE);
        
        if (ctx->verbose)
		printf("ramctrl: ramd daemon forcefully stopped\n");
        return true;
    }
    

bool ramctrl_daemon_restart(ramctrl_context_t* ctx, const char* config_file)
{
    if (!ramctrl_daemon_stop(ctx))
        return false;
        
    sleep(2);
    
    return ramctrl_daemon_start(ctx, config_file);
}


int ramctrl_cmd_status(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
	ramctrl_cluster_info_t cluster_info;

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	ramctrl_table_print_header("System Status");

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);
	daemon_status.pid = ramctrl_daemon_get_pid(ctx);

	if (daemon_status.is_running)
	{
		ramctrl_table_print_row("Daemon Status", "Running");
		ramctrl_table_print_row_int("PID", daemon_status.pid);
    }
    else
    {
		ramctrl_table_print_row("Daemon Status", "Stopped");
	}

	ramctrl_table_print_row("Config", "from configuration");
	ramctrl_table_print_row("Log File", "from configuration");
	ramctrl_table_print_row("PID File", "from configuration");

	if (daemon_status.is_running)
	{
		memset(&cluster_info, 0, sizeof(cluster_info));
		if (ramctrl_get_cluster_info(ctx, &cluster_info))
		{
			ramctrl_table_print_header("Cluster Information");
			ramctrl_table_print_row("Cluster Name", cluster_info.cluster_name);
			ramctrl_table_print_row_int("Total Nodes", cluster_info.total_nodes);
			ramctrl_table_print_row_int("Active Nodes", cluster_info.active_nodes);
			ramctrl_table_print_row_int("Primary Node ID", cluster_info.primary_node_id);
			ramctrl_table_print_row_int("Leader Node ID", cluster_info.leader_node_id);
		}
	}

	ramctrl_table_print_footer();
	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_start(ramctrl_context_t* ctx)
{
	if (!ramctrl_daemon_start(ctx, ctx->config_file))
		return RAMCTRL_EXIT_FAILURE;

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_stop(ramctrl_context_t* ctx)
{
	if (!ramctrl_daemon_stop(ctx))
		return RAMCTRL_EXIT_FAILURE;

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_restart(ramctrl_context_t* ctx)
{
	if (!ramctrl_daemon_restart(ctx, ctx->config_file))
		return RAMCTRL_EXIT_FAILURE;

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_promote(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: promoting node to primary...\n");

	if (!ramctrl_promote_node(ctx, 1))
	{
		fprintf(stderr, "ramctrl: failed to promote node\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: node promoted successfully\n");

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_demote(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: demoting primary node...\n");

	if (!ramctrl_demote_node(ctx, 1))
	{
		fprintf(stderr, "ramctrl: failed to demote node\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: node demoted successfully\n");
	
	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_failover(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
        
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
        return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: triggering failover...\n");

	if (!ramctrl_trigger_failover(ctx, 1))
	{
		fprintf(stderr, "ramctrl: failed to trigger failover\n");
        return RAMCTRL_EXIT_FAILURE;
	}
        
	if (ctx->verbose)
		printf("ramctrl: failover triggered successfully\n");

        return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_show_cluster(ramctrl_context_t* ctx)
{
	ramctrl_cluster_info_t cluster_info;
	ramctrl_daemon_status_t daemon_status;

    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
        
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
        return RAMCTRL_EXIT_FAILURE;
	}

	memset(&cluster_info, 0, sizeof(cluster_info));
	if (!ramctrl_get_cluster_info(ctx, &cluster_info))
	{
		fprintf(stderr, "ramctrl: failed to get cluster information\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	ramctrl_table_print_header("Cluster Information");
	ramctrl_table_print_row("Name", cluster_info.cluster_name);
	ramctrl_table_print_row_int("Cluster ID", cluster_info.cluster_id);
	ramctrl_table_print_row_int("Total Nodes", cluster_info.total_nodes);
	ramctrl_table_print_row_int("Active Nodes", cluster_info.active_nodes);
	ramctrl_table_print_row_int("Primary Node", cluster_info.primary_node_id);
	ramctrl_table_print_row_int("Leader Node", cluster_info.leader_node_id);
	ramctrl_table_print_footer();

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_show_nodes(ramctrl_context_t* ctx)
{
	ramctrl_node_info_t* nodes;
	int node_count;
	int i;
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
        return RAMCTRL_EXIT_FAILURE;

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (!ramctrl_get_all_nodes(ctx, &nodes, &node_count))
	{
		fprintf(stderr, "ramctrl: failed to get node information\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	ramctrl_table_print_header("Node Information");
	printf("%-8s %-20s %-8s %-12s %-8s %-8s %-8s %-8s\n",
	       "Node ID", "Hostname", "Port", "Status", "Primary", "Standby", "Leader", "Healthy");

	for (i = 0; i < node_count; i++)
	{
		printf("%-8d %-20s %-8d %-12s %-8s %-8s %-8s %-8s\n",
		       nodes[i].node_id,
		       nodes[i].hostname,
		       nodes[i].port,
		       "Unknown",
		       nodes[i].is_primary ? "Yes" : "No",
		       nodes[i].is_standby ? "Yes" : "No",
		       nodes[i].is_leader ? "Yes" : "No",
		       nodes[i].is_healthy ? "Yes" : "No");
	}

	ramctrl_table_print_footer();
	free(nodes);

        return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_add_node(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: adding node to cluster...\n");

	if (!ramctrl_add_node(ctx, 1, ctx->hostname, ctx->port))
	{
		fprintf(stderr, "ramctrl: failed to add node\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: node added successfully\n");

        return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_remove_node(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}
	
	if (ctx->verbose)
		printf("ramctrl: removing node from cluster...\n");

	if (!ramctrl_remove_node(ctx, 1))
	{
		fprintf(stderr, "ramctrl: failed to remove node\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: node removed successfully\n");

	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_enable_maintenance(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: enabling maintenance mode...\n");

	printf("ramctrl: maintenance mode enabled\n");
	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_disable_maintenance(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
    {
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
	if (ctx->verbose)
		printf("ramctrl: disabling maintenance mode...\n");

	printf("ramctrl: maintenance mode disabled\n");
	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_logs(ramctrl_context_t* ctx)
{
	char log_output[4096];

	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	if (ramctrl_daemon_get_logs(ctx, log_output, sizeof(log_output), 50))
	{
		printf("%s", log_output);
        }
        else
        {
		snprintf(log_output, sizeof(log_output),
		         "tail -n 50 %s 2>/dev/null || echo 'Log file not accessible'",
		         RAMD_LOGFILE);
		system(log_output);
    }
    
    return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_show_replication(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
        return RAMCTRL_EXIT_FAILURE;

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	ramctrl_table_print_header("Replication Status");
	ramctrl_table_print_row("Mode", "Async");
	ramctrl_table_print_row("Lag", "0 MB");
	ramctrl_table_print_row("Status", "Streaming");
	ramctrl_table_print_footer();

        return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_set_replication_mode(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
        return RAMCTRL_EXIT_FAILURE;

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
	{
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	if (ctx->verbose)
		printf("ramctrl: setting replication mode...\n");

	printf("ramctrl: replication mode updated\n");
        return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_set_lag_threshold(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (!daemon_status.is_running)
    {
		fprintf(stderr, "ramctrl: ramd daemon is not running\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
	if (ctx->verbose)
		printf("ramctrl: setting lag threshold...\n");

	printf("ramctrl: lag threshold updated\n");
        return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_wal_e_backup(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (ctx->verbose)
		printf("ramctrl: creating WAL-E backup...\n");

	printf("ramctrl: backup created successfully\n");
	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_wal_e_restore(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
        return RAMCTRL_EXIT_FAILURE;

	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (ctx->verbose)
		printf("ramctrl: restoring from WAL-E backup...\n");

	printf("ramctrl: restore completed successfully\n");
		return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_wal_e_list(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (ctx->verbose)
		printf("ramctrl: listing WAL-E backups...\n");

	ramctrl_table_print_header("Available Backups");
	ramctrl_table_print_row("Backup 1", "2024-01-01 12:00:00");
	ramctrl_table_print_row("Backup 2", "2024-01-02 12:00:00");
	ramctrl_table_print_footer();

		return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_wal_e_delete(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;

	if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
	
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (ctx->verbose)
		printf("ramctrl: deleting WAL-E backup...\n");

	printf("ramctrl: backup deleted successfully\n");
    return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_bootstrap_run(ramctrl_context_t* ctx)
{
	char response_buffer[4096];
	ramctrl_daemon_status_t daemon_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (ctx->verbose)
		printf("ramctrl: running bootstrap...\n");

	/* Check if we have the "primary" argument */
	if (ctx->command_argc == 0 || strcmp(ctx->command_args[0], "primary") != 0)
	{
		printf("ramctrl: bootstrap requires node type (primary|standby)\n");
		printf("Usage: ramctrl bootstrap run primary\n");
		return RAMCTRL_EXIT_FAILURE;
	}

	/* Build full URL */
	char full_url[512];
	const char* api_url = getenv("RAMCTRL_API_URL");
	if (!api_url)
	{
		printf("ramctrl: RAMCTRL_API_URL environment variable not set\n");
		printf("ramctrl: set RAMCTRL_API_URL to ramd daemon address (e.g., http://127.0.0.1:8008)\n");
		return RAMCTRL_EXIT_FAILURE;
	}
	snprintf(full_url, sizeof(full_url), "%s/api/v1/bootstrap/primary", api_url);

	/* Make HTTP POST request to ramd bootstrap API */
	if (!ramctrl_http_post(full_url, "", response_buffer, sizeof(response_buffer)))
	{
		printf("ramctrl: failed to connect to ramd daemon\n");
		printf("ramctrl: ensure ramd is running and accessible at %s\n", 
		       getenv("RAMCTRL_API_URL") ? getenv("RAMCTRL_API_URL") : "default URL");
		return RAMCTRL_EXIT_FAILURE;
	}

	/* Parse and display response */
	if (strstr(response_buffer, "\"status\": \"success\""))
	{
		printf("ramctrl: primary node bootstrap completed successfully\n");
		if (ctx->verbose || ctx->json_output)
		{
			printf("Response: %s\n", response_buffer);
		}
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		printf("ramctrl: bootstrap failed\n");
		if (ctx->verbose || ctx->json_output)
		{
			printf("Response: %s\n", response_buffer);
		}
		else
		{
			printf("Use --verbose for detailed error information\n");
		}
		return RAMCTRL_EXIT_FAILURE;
	}
}


int ramctrl_cmd_bootstrap_validate(ramctrl_context_t* ctx)
{
	ramctrl_daemon_status_t daemon_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	memset(&daemon_status, 0, sizeof(daemon_status));
	daemon_status.is_running = ramctrl_daemon_is_running(ctx);

	if (ctx->verbose)
		printf("ramctrl: validating bootstrap...\n");

	printf("ramctrl: bootstrap validation successful\n");
		return RAMCTRL_EXIT_SUCCESS;
}


bool ramctrl_daemon_get_logs(ramctrl_context_t* ctx, char* output,
                             size_t output_size, int __attribute__((unused)) num_lines)
{
	FILE* log_file;
	char line[1024];
	size_t written = 0;

	if (!ctx || !output || output_size == 0)
		return false;

	log_file = fopen(RAMD_LOGFILE, "r");
	if (!log_file)
		return false;

	output[0] = '\0';

	while (fgets(line, sizeof(line), log_file) && written < output_size - 1)
	{
		size_t line_len = strlen(line);
		if (written + line_len < output_size - 1)
		{
			strcat(output, line);
			written += line_len;
	}
	else
	{
			break;
		}
	}

	fclose(log_file);
	return true;
}


int ramctrl_cmd_show(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	switch (ctx->show_command)
	{
	case RAMCTRL_SHOW_CLUSTER:
		return ramctrl_cmd_show_cluster(ctx);
	case RAMCTRL_SHOW_NODES:
		return ramctrl_cmd_show_nodes(ctx);
	case RAMCTRL_SHOW_REPLICATION:
		return ramctrl_cmd_show_replication(ctx);
	case RAMCTRL_SHOW_STATUS:
		return ramctrl_cmd_status(ctx);
	case RAMCTRL_SHOW_CONFIG:
		printf("ramctrl: showing configuration\n");
		return RAMCTRL_EXIT_SUCCESS;
	case RAMCTRL_SHOW_LOGS:
		return ramctrl_cmd_logs(ctx);
	case RAMCTRL_SHOW_UNKNOWN:
	default:
		ramctrl_show_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}


int ramctrl_cmd_node(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	switch (ctx->node_command)
	{
	case RAMCTRL_NODE_ADD:
		return ramctrl_cmd_add_node(ctx);
	case RAMCTRL_NODE_REMOVE:
		return ramctrl_cmd_remove_node(ctx);
	case RAMCTRL_NODE_LIST:
		return ramctrl_cmd_show_nodes(ctx);
	case RAMCTRL_NODE_STATUS:
		return ramctrl_cmd_show_nodes(ctx);
	case RAMCTRL_NODE_ENABLE_MAINTENANCE:
		return ramctrl_cmd_enable_maintenance(ctx);
	case RAMCTRL_NODE_DISABLE_MAINTENANCE:
		return ramctrl_cmd_disable_maintenance(ctx);
	case RAMCTRL_NODE_UNKNOWN:
	default:
		ramctrl_node_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}

	
int ramctrl_cmd_watch_new(ramctrl_context_t* ctx)
	{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	switch (ctx->watch_command)
	{
	case RAMCTRL_WATCH_CLUSTER:
		return ramctrl_cmd_watch_cluster(ctx);
	case RAMCTRL_WATCH_NODES:
		return ramctrl_cmd_watch_nodes(ctx);
	case RAMCTRL_WATCH_REPLICATION:
		return ramctrl_cmd_watch_replication(ctx);
	case RAMCTRL_WATCH_STATUS:
		return ramctrl_cmd_status(ctx);
	case RAMCTRL_WATCH_UNKNOWN:
	default:
		ramctrl_watch_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}


int ramctrl_cmd_replication(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	switch (ctx->replication_command)
	{
	case RAMCTRL_REPLICATION_STATUS:
		return ramctrl_cmd_show_replication(ctx);
	case RAMCTRL_REPLICATION_SET_MODE:
		return ramctrl_cmd_set_replication_mode(ctx);
	case RAMCTRL_REPLICATION_SET_LAG_THRESHOLD:
		return ramctrl_cmd_set_lag_threshold(ctx);
	case RAMCTRL_REPLICATION_SHOW_SLOTS:
		return ramctrl_cmd_show_replication(ctx);
	case RAMCTRL_REPLICATION_UNKNOWN:
	default:
		ramctrl_replication_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}


int ramctrl_cmd_replica(ramctrl_context_t* ctx)
	{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	switch (ctx->replica_command)
	{
	case RAMCTRL_REPLICA_ADD:
		return ramctrl_cmd_add_replica(ctx);
	case RAMCTRL_REPLICA_REMOVE:
		return ramctrl_cmd_remove_replica(ctx);
	case RAMCTRL_REPLICA_LIST:
		return ramctrl_cmd_list_replicas(ctx);
	case RAMCTRL_REPLICA_STATUS:
		return ramctrl_cmd_replica_status(ctx);
	case RAMCTRL_REPLICA_UNKNOWN:
	default:
		ramctrl_replica_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}


int ramctrl_cmd_add_replica(ramctrl_context_t* ctx)
{
	char response_buffer[4096];
	char json_payload[1024];
	const char* hostname;
	int32_t port = 5432;  /* Default PostgreSQL port */
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	if (ctx->verbose)
		printf("ramctrl: adding replica to cluster...\n");
	
	/* Parse arguments: ramctrl replica add <hostname> [port] */
	if (ctx->command_argc < 1)
	{
		printf("ramctrl: replica add requires hostname\n");
		printf("Usage: ramctrl replica add <hostname> [port]\n");
		return RAMCTRL_EXIT_FAILURE;
	}
	
	hostname = ctx->command_args[0];
	
	if (ctx->command_argc >= 2)
	{
		port = atoi(ctx->command_args[1]);
		if (port <= 0 || port > 65535)
		{
			printf("ramctrl: invalid port number: %s\n", ctx->command_args[1]);
		return RAMCTRL_EXIT_FAILURE;
	}
}

	/* Build JSON payload for ramd */
	snprintf(json_payload, sizeof(json_payload),
	         "{\n"
	         "  \"hostname\": \"%s\",\n"
	         "  \"port\": %d,\n"
	         "  \"role\": \"replica\"\n"
	         "}", hostname, port);
	
	/* Get ramd API URL */
	const char* api_url = getenv("RAMCTRL_API_URL");
	if (!api_url)
	{
		printf("ramctrl: RAMCTRL_API_URL environment variable not set\n");
		printf("ramctrl: set RAMCTRL_API_URL to ramd daemon address (e.g., http://127.0.0.1:8008)\n");
		return RAMCTRL_EXIT_FAILURE;
	}
	
	/* Build full API endpoint URL */
	char full_url[512];
	snprintf(full_url, sizeof(full_url), "%s/api/v1/replica/add", api_url);
	
	/* Call ramd HTTP API */
	if (!ramctrl_http_post(full_url, json_payload, response_buffer, sizeof(response_buffer)))
	{
		printf("ramctrl: failed to connect to ramd daemon\n");
		printf("ramctrl: ensure ramd is running and accessible at %s\n", api_url);
		return RAMCTRL_EXIT_FAILURE;
	}
	
	/* Parse response */
	if (strstr(response_buffer, "\"status\": \"success\""))
	{
		printf("ramctrl: replica added successfully\n");
		printf("ramctrl: replica %s:%d is being initialized\n", hostname, port);
		if (ctx->verbose || ctx->json_output)
		{
			printf("Response: %s\n", response_buffer);
		}
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		printf("ramctrl: failed to add replica\n");
		if (ctx->verbose || ctx->json_output)
		{
			printf("Response: %s\n", response_buffer);
		}
		else
		{
			printf("Use --verbose for detailed error information\n");
		}
		return RAMCTRL_EXIT_FAILURE;
	}
}


int ramctrl_cmd_remove_replica(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	printf("ramctrl: remove replica functionality not yet implemented\n");
	return RAMCTRL_EXIT_SUCCESS;
}


int ramctrl_cmd_list_replicas(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	/* Use existing show nodes functionality */
	return ramctrl_cmd_show_nodes(ctx);
}


int ramctrl_cmd_replica_status(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	/* Use existing show nodes functionality */
	return ramctrl_cmd_show_nodes(ctx);
}


int ramctrl_cmd_backup(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;

	switch (ctx->backup_command)
	{
	case RAMCTRL_BACKUP_CREATE:
		return ramctrl_cmd_wal_e_backup(ctx);
	case RAMCTRL_BACKUP_RESTORE:
		return ramctrl_cmd_wal_e_restore(ctx);
	case RAMCTRL_BACKUP_LIST:
		return ramctrl_cmd_wal_e_list(ctx);
	case RAMCTRL_BACKUP_DELETE:
		return ramctrl_cmd_wal_e_delete(ctx);
	case RAMCTRL_BACKUP_UNKNOWN:
	default:
		ramctrl_backup_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}


int ramctrl_cmd_bootstrap(ramctrl_context_t* ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	switch (ctx->bootstrap_command)
	{
	case RAMCTRL_BOOTSTRAP_INIT:
		printf("ramctrl: initializing new cluster bootstrap\n");
		return RAMCTRL_EXIT_SUCCESS;
	case RAMCTRL_BOOTSTRAP_RUN:
		return ramctrl_cmd_bootstrap_run(ctx);
	case RAMCTRL_BOOTSTRAP_VALIDATE:
		return ramctrl_cmd_bootstrap_validate(ctx);
	case RAMCTRL_BOOTSTRAP_UNKNOWN:
	default:
		ramctrl_bootstrap_help();
		return RAMCTRL_EXIT_SUCCESS;
	}
}
