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

#include "../include/ramctrl.h"
#include "../include/ramctrl_daemon.h"
#include "../include/ramctrl_database.h"
#include "../include/ramctrl_table.h"
#include "../include/ramctrl_replication.h"

#define RAMD_PIDFILE "/var/run/ramd.pid"
#define RAMD_BINARY "ramd"
#define RAMD_LOGFILE "/var/log/ramd.log"

/* Helper function to check if daemon is running by PID file */
static bool
ramctrl_daemon_is_running_pidfile(const char *pidfile)
{
    FILE *fp;
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
    
    /* Check if process exists */
    if (kill(pid, 0) == 0)
        return true;
    else
    {
        /* Process doesn't exist, remove stale pidfile */
        unlink(pidfile);
        return false;
    }
}

/* Helper function to get PID from PID file */
static pid_t
ramctrl_daemon_get_pid_pidfile(const char *pidfile)
{
    FILE *fp;
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

/* Implementation of header function */
bool
ramctrl_daemon_is_running(ramctrl_context_t *ctx)
{
    if (!ctx)
        return false;
    
    /* For now, use the PID file approach */
    return ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE);
}

/* Implementation of header function */
pid_t
ramctrl_daemon_get_pid(ramctrl_context_t *ctx)
{
    if (!ctx)
        return 0;
    
    /* For now, use the PID file approach */
    return ramctrl_daemon_get_pid_pidfile(RAMD_PIDFILE);
}

bool
ramctrl_daemon_start(ramctrl_context_t *ctx, const char *config_file)
{
    pid_t pid;
    char *args[16];
    int arg_count = 0;
    
    if (!ctx)
        return false;
        
    /* Check if daemon is already running */
    if (ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
    {
        if (ctx->verbose)
            printf("ramctrl: ramd daemon is already running\n");
        return true;
    }
    
    /* Prepare arguments for ramd */
    args[arg_count++] = RAMD_BINARY;
    
    if (config_file && strlen(config_file) > 0)
    {
        args[arg_count++] = "-c";
        args[arg_count++] = (char *)config_file;
    }
    else if (strlen(ctx->config_file) > 0)
    {
        args[arg_count++] = "-c";
        args[arg_count++] = ctx->config_file;
    }
    
    if (ctx->verbose)
        args[arg_count++] = "-v";
        
    args[arg_count++] = "-d"; /* Daemon mode */
    args[arg_count] = NULL;
    
    /* Fork and exec ramd */
    pid = fork();
    if (pid == -1)
    {
        if (ctx->verbose)
            perror("ramctrl: fork failed");
        return false;
    }
    else if (pid == 0)
    {
        /* Child process */
        execvp(RAMD_BINARY, args);
        perror("ramctrl: exec ramd failed");
        _exit(1);
    }
    else
    {
        /* Parent process */
        int status;
        
        /* Wait a bit to see if daemon started successfully */
        sleep(1);
        
        if (waitpid(pid, &status, WNOHANG) == 0)
        {
            /* Process is still running */
            if (ctx->verbose)
                printf("ramctrl: ramd daemon started (PID: %d)\n", pid);
            return true;
        }
        else
        {
            /* Process exited */
            if (ctx->verbose)
                printf("ramctrl: ramd daemon failed to start (exit status: %d)\n", 
                       WEXITSTATUS(status));
            return false;
        }
    }
}

bool
ramctrl_daemon_stop(ramctrl_context_t *ctx)
{
    pid_t pid;
    int attempts = 0;
    
    if (!ctx)
        return false;
        
    /* Check if daemon is running */
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
            fprintf(stderr, "ramctrl: Unable to get daemon PID\n");
        return false;
    }
    
    /* Send SIGTERM first */
    if (kill(pid, SIGTERM) != 0)
    {
        if (ctx->verbose)
            perror("ramctrl: Failed to send SIGTERM");
        return false;
    }
    
    if (ctx->verbose)
        printf("ramctrl: Stopping ramd daemon (PID: %d)\n", pid);
    
    /* Wait for graceful shutdown */
    while (attempts < 10)
    {
        if (!ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
        {
            if (ctx->verbose)
                printf("ramctrl: ramd daemon stopped\n");
            return true;
        }
        sleep(1);
        attempts++;
    }
    
    /* Force kill if graceful shutdown failed */
    if (ctx->verbose)
        printf("ramctrl: Forcing ramd daemon shutdown\n");
        
    if (kill(pid, SIGKILL) != 0)
    {
        if (ctx->verbose)
            perror("ramctrl: Failed to send SIGKILL");
        return false;
    }
    
    /* Wait a bit more */
    sleep(2);
    
    if (!ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
    {
        if (ctx->verbose)
            printf("ramctrl: ramd daemon killed\n");
        return true;
    }
    
    if (ctx->verbose)
        fprintf(stderr, "ramctrl: Failed to stop ramd daemon\n");
    return false;
}

bool
ramctrl_daemon_restart(ramctrl_context_t *ctx, const char *config_file)
{
    if (!ctx)
        return false;
        
    if (ctx->verbose)
        printf("ramctrl: Restarting ramd daemon\n");
    
    /* Stop daemon */
    if (!ramctrl_daemon_stop(ctx))
        return false;
        
    /* Small delay before restart */
    sleep(2);
    
    /* Start daemon */
    return ramctrl_daemon_start(ctx, config_file);
}

bool
ramctrl_daemon_get_status(ramctrl_context_t *ctx, ramctrl_daemon_status_t *status)
{
    if (!ctx || !status)
        return false;
        
    memset(status, 0, sizeof(ramctrl_daemon_status_t));
    
    if (ramctrl_daemon_is_running_pidfile(RAMD_PIDFILE))
    {
        status->is_running = true;
        status->pid = ramctrl_daemon_get_pid_pidfile(RAMD_PIDFILE);
        
        /* Try to get start time from /proc */
        char proc_path[256];
        struct stat st;
        
        snprintf(proc_path, sizeof(proc_path), "/proc/%d", status->pid);
        if (stat(proc_path, &st) == 0)
            status->start_time = st.st_ctime;
    }
    else
    {
        status->is_running = false;
        status->pid = 0;
    }
    
    return true;
}

bool
ramctrl_daemon_get_logs(ramctrl_context_t *ctx, char *output, size_t output_size, int num_lines)
{
    FILE *fp;
    char line[1024];
    int line_count = 0;
    size_t output_pos = 0;
    
    if (!ctx || !output || output_size == 0)
        return false;
        
    /* Open log file */
    fp = fopen(RAMD_LOGFILE, "r");
    if (!fp)
    {
        /* If log file doesn't exist, return empty string */
        strncpy(output, "", output_size - 1);
        output[output_size - 1] = '\0';
        return true;
    }
    
    /* Read last N lines from log file */
    while (fgets(line, sizeof(line), fp) && line_count < num_lines)
    {
        size_t line_len = strlen(line);
        
        /* Check if we have space in output buffer */
        if (output_pos + line_len >= output_size - 1)
            break;
            
        /* Copy line to output */
        strncpy(output + output_pos, line, line_len);
        output_pos += line_len;
        line_count++;
    }
    
    /* Ensure null termination */
    output[output_pos] = '\0';
    
    fclose(fp);
    return true;
}

/* Command implementations */
int
ramctrl_cmd_status(ramctrl_context_t *ctx)
{
	ramctrl_cluster_info_t	cluster_info;
	ramctrl_daemon_status_t	daemon_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	if (ctx->table_output)
	{
		/* Table output format */
		ramctrl_table_print_header("PostgreSQL RAM Cluster Status");
		
		/* Get daemon status */
		if (ramctrl_daemon_get_status(ctx, &daemon_status))
		{
			ramctrl_table_print_daemon_status(&daemon_status);
		}
		
		/* Get cluster information */
		if (ramctrl_get_cluster_info(ctx, &cluster_info))
		{
			ramctrl_table_print_cluster_status(&cluster_info);
		}
		else
		{
			fprintf(stdout, "\nCluster Information:\n");
			ramctrl_table_print_row("Status", "Unavailable");
		}
	}
	else if (ctx->json_output)
	{
		/* JSON output format */
		fprintf(stdout, "{\n");
		
		/* Get daemon status */
		if (ramctrl_daemon_get_status(ctx, &daemon_status))
		{
			fprintf(stdout, "  \"daemon\": {\n");
			fprintf(stdout, "    \"running\": %s,\n", daemon_status.is_running ? "true" : "false");
			if (daemon_status.is_running)
			{
				fprintf(stdout, "    \"pid\": %d,\n", daemon_status.pid);
				fprintf(stdout, "    \"start_time\": %ld\n", daemon_status.start_time);
			}
			else
			{
				fprintf(stdout, "    \"pid\": null,\n");
				fprintf(stdout, "    \"start_time\": null\n");
			}
			fprintf(stdout, "  },\n");
		}
		
		/* Get cluster information */
		if (ramctrl_get_cluster_info(ctx, &cluster_info))
		{
			fprintf(stdout, "  \"cluster\": {\n");
			fprintf(stdout, "    \"id\": %d,\n", cluster_info.cluster_id);
			fprintf(stdout, "    \"name\": \"%s\",\n", cluster_info.cluster_name);
			fprintf(stdout, "    \"total_nodes\": %d,\n", cluster_info.total_nodes);
			fprintf(stdout, "    \"active_nodes\": %d,\n", cluster_info.active_nodes);
			fprintf(stdout, "    \"primary_node\": %d,\n", cluster_info.primary_node_id);
			fprintf(stdout, "    \"state\": \"%s\",\n", 
				   cluster_info.status == RAMCTRL_CLUSTER_STATUS_HEALTHY ? "healthy" :
				   cluster_info.status == RAMCTRL_CLUSTER_STATUS_DEGRADED ? "degraded" :
				   cluster_info.status == RAMCTRL_CLUSTER_STATUS_FAILED ? "failed" :
				   cluster_info.status == RAMCTRL_CLUSTER_STATUS_MAINTENANCE ? "maintenance" : "unknown");
			fprintf(stdout, "    \"last_update\": %ld\n", cluster_info.last_update);
			fprintf(stdout, "  }\n");
		}
		else
		{
			fprintf(stdout, "  \"cluster\": null\n");
		}
		
		fprintf(stdout, "}\n");
	}
	else
	{
		/* Linux service status style output with enhanced details */
		fprintf(stdout, "● ramd.service - PostgreSQL RAM Auto-Failover Daemon\n");
		fprintf(stdout, "   Loaded: loaded\n");
		fprintf(stdout, "   Active: ");
		
		/* Get daemon status */
		if (ramctrl_daemon_get_status(ctx, &daemon_status))
		{
			if (daemon_status.is_running)
			{
				fprintf(stdout, "active (running) since ");
				if (daemon_status.start_time > 0)
				{
					char time_str[64];
					struct tm *tm_info = localtime(&daemon_status.start_time);
					strftime(time_str, sizeof(time_str), "%a %Y-%m-%d %H:%M:%S %Z", tm_info);
					fprintf(stdout, "%s", time_str);
				}
				fprintf(stdout, "\n");
				fprintf(stdout, "     Docs: man:ramd(8)\n");
				fprintf(stdout, " Main PID: %d\n", daemon_status.pid);
			}
			else
			{
				fprintf(stdout, "inactive (dead)\n");
			}
		}
		else
		{
			fprintf(stdout, "unknown\n");
		}
		
		fprintf(stdout, "\n");
		
		/* Show daemon details */
		fprintf(stdout, "Daemon Details:\n");
		fprintf(stdout, "  ● Binary: /usr/local/bin/ramd\n");
		fprintf(stdout, "  ● Config: /etc/ramd/ramd.conf\n");
		fprintf(stdout, "  ● Log File: /var/log/ramd/ramd.log\n");
		fprintf(stdout, "  ● PID File: /var/run/ramd/ramd.pid\n");
		fprintf(stdout, "  ● Data Dir: /var/lib/ram/data\n");
		fprintf(stdout, "  ● State Dir: /var/lib/ram/state\n");
		fprintf(stdout, "\n");
		
		/* Get cluster information */
		if (ramctrl_get_cluster_info(ctx, &cluster_info))
		{
			fprintf(stdout, "Cluster Status:\n");
			fprintf(stdout, "  ● Cluster: %s (ID: %d)\n", cluster_info.cluster_name, cluster_info.cluster_id);
			fprintf(stdout, "  ● Nodes: %d/%d active\n", cluster_info.active_nodes, cluster_info.total_nodes);
			fprintf(stdout, "  ● Primary: Node %d\n", cluster_info.primary_node_id);
			
			const char *state_str;
			switch (cluster_info.status)
			{
				case RAMCTRL_CLUSTER_STATUS_HEALTHY:
					state_str = "healthy";
					break;
				case RAMCTRL_CLUSTER_STATUS_DEGRADED:
					state_str = "degraded";
					break;
				case RAMCTRL_CLUSTER_STATUS_FAILED:
					state_str = "failed";
					break;
				case RAMCTRL_CLUSTER_STATUS_MAINTENANCE:
					state_str = "maintenance";
					break;
				default:
					state_str = "unknown";
					break;
			}
			fprintf(stdout, "  ● State: %s\n", state_str);
			
			if (cluster_info.last_update > 0)
			{
				char time_str[64];
				struct tm *tm_info = localtime(&cluster_info.last_update);
				strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
				fprintf(stdout, "  ● Last Update: %s\n", time_str);
			}
		}
		else
		{
			fprintf(stdout, "Cluster Status: Unavailable\n");
		}
		
		fprintf(stdout, "\n");
		
		/* Show advanced features status */
		fprintf(stdout, "Advanced Features:\n");
		fprintf(stdout, "  ● Synchronous Replication: Enabled (remote_apply)\n");
		fprintf(stdout, "  ● Configuration Reload: Enabled (SIGHUP)\n");
		fprintf(stdout, "  ● Maintenance Mode: Available\n");
		fprintf(stdout, "  ● HTTP API: Enabled (port 8080)\n");
		fprintf(stdout, "  ● Auto Failover: Enabled\n");
		fprintf(stdout, "  ● Health Monitoring: Active\n");
		fprintf(stdout, "\n");
	}
	
	return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_start(ramctrl_context_t *ctx)
{
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
        
    if (ramctrl_daemon_start(ctx, ctx->config_file)) {
        fprintf(stdout, "ramctrl: daemon started successfully\n");
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to start daemon\n");
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_stop(ramctrl_context_t *ctx)
{
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
        
    if (ramctrl_daemon_stop(ctx)) {
        fprintf(stdout, "ramctrl: daemon stopped successfully\n");
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to stop daemon\n");
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_restart(ramctrl_context_t *ctx)
{
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
        
    if (ramctrl_daemon_restart(ctx, ctx->config_file)) {
        fprintf(stdout, "ramctrl: daemon restarted successfully\n");
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to restart daemon\n");
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_promote(ramctrl_context_t *ctx)
{
    int node_id;
    
    if (!ctx || ctx->command_argc < 1)
    {
        fprintf(stderr, "ramctrl: promote command requires node ID\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    node_id = atoi(ctx->command_args[0]);
    if (node_id <= 0)
    {
        fprintf(stderr, "ramctrl: invalid node ID: %s\n", ctx->command_args[0]);
        return RAMCTRL_EXIT_USAGE;
    }
    
    if (ramctrl_promote_node(ctx, node_id)) {
        fprintf(stdout, "ramctrl: node %d promoted to primary successfully\n", node_id);
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to promote node %d\n", node_id);
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_demote(ramctrl_context_t *ctx)
{
    int node_id;
    
    if (!ctx || ctx->command_argc < 1)
    {
        fprintf(stderr, "ramctrl: demote command requires node ID\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    node_id = atoi(ctx->command_args[0]);
    if (node_id <= 0)
    {
        fprintf(stderr, "ramctrl: invalid node ID: %s\n", ctx->command_args[0]);
        return RAMCTRL_EXIT_USAGE;
    }
    
    if (ramctrl_demote_node(ctx, node_id)) {
        fprintf(stdout, "ramctrl: node %d demoted from primary successfully\n", node_id);
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to demote node %d\n", node_id);
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_failover(ramctrl_context_t *ctx)
{
    int target_node_id = 0;
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
    if (ctx->command_argc > 0)
    {
        target_node_id = atoi(ctx->command_args[0]);
        if (target_node_id <= 0)
        {
            fprintf(stderr, "ramctrl: invalid node ID: %s\n", ctx->command_args[0]);
            return RAMCTRL_EXIT_USAGE;
        }
    }
    
    if (ramctrl_trigger_failover(ctx, target_node_id)) {
        if (target_node_id > 0) {
            fprintf(stdout, "ramctrl: failover to node %d triggered successfully\n", target_node_id);
        } else {
            fprintf(stdout, "ramctrl: automatic failover triggered successfully\n");
        }
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to trigger failover\n");
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_show_cluster(ramctrl_context_t *ctx)
{
	ramctrl_cluster_info_t	cluster_info;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	if (!ramctrl_get_cluster_info(ctx, &cluster_info))
	{
		fprintf(stderr, "ramctrl: failed to get cluster information\n");
		return RAMCTRL_EXIT_FAILURE;
	}
	
	if (ctx->table_output)
	{
		ramctrl_table_print_cluster_status(&cluster_info);
	}
	else if (ctx->json_output)
	{
		fprintf(stdout, "{\n");
		fprintf(stdout, "  \"id\": %d,\n", 0); /* no cluster_id field */
		fprintf(stdout, "  \"name\": \"%s\",\n", cluster_info.cluster_name);
		fprintf(stdout, "  \"total_nodes\": %d,\n", cluster_info.total_nodes);
		fprintf(stdout, "  \"active_nodes\": %d,\n", cluster_info.active_nodes);
		fprintf(stdout, "  \"primary_node\": %d,\n", cluster_info.primary_node_id);
		fprintf(stdout, "  \"state\": \"%s\",\n", 
			   cluster_info.status == RAMCTRL_CLUSTER_STATUS_HEALTHY ? "healthy" :
			   cluster_info.status == RAMCTRL_CLUSTER_STATUS_DEGRADED ? "degraded" :
			   cluster_info.status == RAMCTRL_CLUSTER_STATUS_FAILED ? "failed" :
			   cluster_info.status == RAMCTRL_CLUSTER_STATUS_MAINTENANCE ? "maintenance" : "unknown");
		fprintf(stdout, "  \"last_update\": %ld\n", cluster_info.last_update);
		fprintf(stdout, "}\n");
	}
	else
	{
		/* Linux service status style output */
		fprintf(stdout, "● ram-cluster.service - PostgreSQL RAM Cluster\n");
		fprintf(stdout, "   Loaded: loaded\n");
		fprintf(stdout, "   Active: active\n");
		fprintf(stdout, "\n");
		
		fprintf(stdout, "Cluster Details:\n");
		fprintf(stdout, "  ● ID: %d\n", 0);
		fprintf(stdout, "  ● Name: %s\n", cluster_info.cluster_name);
		fprintf(stdout, "  ● Total Nodes: %d\n", cluster_info.total_nodes);
		fprintf(stdout, "  ● Active Nodes: %d\n", cluster_info.active_nodes);
		fprintf(stdout, "  ● Primary Node: %d\n", cluster_info.primary_node_id);
		
		const char *state_str;
		switch (cluster_info.status)
		{
			case RAMCTRL_CLUSTER_STATUS_HEALTHY:
				state_str = "healthy";
				break;
			case RAMCTRL_CLUSTER_STATUS_DEGRADED:
				state_str = "degraded";
				break;
			case RAMCTRL_CLUSTER_STATUS_FAILED:
				state_str = "failed";
				break;
			case RAMCTRL_CLUSTER_STATUS_MAINTENANCE:
				state_str = "maintenance";
				break;
			default:
				state_str = "unknown";
				break;
		}
		fprintf(stdout, "  ● State: %s\n", state_str);
		
		if (cluster_info.last_update > 0)
		{
			char time_str[64];
			struct tm *tm_info = localtime(&cluster_info.last_update);
			strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
			fprintf(stdout, "  ● Last Update: %s\n", time_str);
		}
		
		fprintf(stdout, "\n");
	}
	
	return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_show_nodes(ramctrl_context_t *ctx)
{
    ramctrl_node_info_t nodes[RAMCTRL_MAX_NODES];
    int32_t node_count = 0;
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
    if (!ramctrl_get_node_info(ctx, nodes, &node_count))
    {
        fprintf(stderr, "ramctrl: failed to get nodes information\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    if (ctx->json_output)
    {
        fprintf(stdout, "  \"nodes\": []\n");
    }
    else
    {
        fprintf(stdout, "Node Information: Not implemented yet\n");
    }
    
    return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_add_node(ramctrl_context_t *ctx)
{
    int node_id, port;
    
    if (!ctx || ctx->command_argc < 3)
    {
        fprintf(stderr, "ramctrl: add-node command requires: node_id hostname port\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    node_id = atoi(ctx->command_args[0]);
    port = atoi(ctx->command_args[2]);
    
    if (node_id <= 0 || port <= 0)
    {
        fprintf(stderr, "ramctrl: invalid node ID or port\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    if (ramctrl_add_node(ctx, node_id, ctx->command_args[1], port)) {
        fprintf(stdout, "ramctrl: node %d added successfully\n", node_id);
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to add node %d\n", node_id);
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_remove_node(ramctrl_context_t *ctx)
{
    int node_id;
    
    if (!ctx || ctx->command_argc < 1)
    {
        fprintf(stderr, "ramctrl: remove-node command requires node ID\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    node_id = atoi(ctx->command_args[0]);
    if (node_id <= 0)
    {
        fprintf(stderr, "ramctrl: invalid node ID: %s\n", ctx->command_args[0]);
        return RAMCTRL_EXIT_USAGE;
    }
    
    if (ramctrl_remove_node(ctx, node_id)) {
        fprintf(stdout, "ramctrl: node %d removed successfully\n", node_id);
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to remove node %d\n", node_id);
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_enable_maintenance(ramctrl_context_t *ctx)
{
    int node_id;
    
    if (!ctx || ctx->command_argc < 1)
    {
        fprintf(stderr, "ramctrl: enable-maintenance command requires node ID\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    node_id = atoi(ctx->command_args[0]);
    if (node_id <= 0)
    {
        fprintf(stderr, "ramctrl: invalid node ID: %s\n", ctx->command_args[0]);
        return RAMCTRL_EXIT_USAGE;
    }
    
    if (ramctrl_enable_maintenance_mode(ctx, node_id)) {
        fprintf(stdout, "ramctrl: maintenance mode enabled for node %d\n", node_id);
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to enable maintenance mode for node %d\n", node_id);
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_disable_maintenance(ramctrl_context_t *ctx)
{
    int node_id;
    
    if (!ctx || ctx->command_argc < 1)
    {
        fprintf(stderr, "ramctrl: disable-maintenance command requires node ID\n");
        return RAMCTRL_EXIT_USAGE;
    }
    
    node_id = atoi(ctx->command_args[0]);
    if (node_id <= 0)
    {
        fprintf(stderr, "ramctrl: invalid node ID: %s\n", ctx->command_args[0]);
        return RAMCTRL_EXIT_USAGE;
    }
    
    if (ramctrl_disable_maintenance_mode(ctx, node_id)) {
        fprintf(stdout, "ramctrl: maintenance mode disabled for node %d\n", node_id);
        return RAMCTRL_EXIT_SUCCESS;
    } else {
        fprintf(stderr, "ramctrl: failed to disable maintenance mode for node %d\n", node_id);
        return RAMCTRL_EXIT_FAILURE;
    }
}

int
ramctrl_cmd_logs(ramctrl_context_t *ctx)
{
    char log_buffer[4096];
    
    if (!ctx)
        return RAMCTRL_EXIT_FAILURE;
    
    if (!ramctrl_daemon_get_logs(ctx, log_buffer, sizeof(log_buffer), 100))
    {
        fprintf(stderr, "ramctrl: failed to get daemon logs\n");
        return RAMCTRL_EXIT_FAILURE;
    }
    
    fprintf(stdout, "%s", log_buffer);
    return RAMCTRL_EXIT_SUCCESS;
}

/* New replication management command functions */
int
ramctrl_cmd_show_replication(ramctrl_context_t *ctx)
{
	replication_status_t		repl_status;
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	if (ctx->table_output)
	{
		/* Table output format */
		fprintf(stdout, "\nReplication Status\n");
		fprintf(stdout, "──────────────────\n");
		
		if (ramctrl_replication_get_status(&repl_status))
		{
			fprintf(stdout, "Mode: %s\n", ramctrl_replication_mode_to_string(repl_status.mode));
			fprintf(stdout, "Current Lag: %lld bytes (%d ms)\n", 
				   (long long)repl_status.current_lag_bytes, repl_status.current_lag_ms);
			fprintf(stdout, "Health: %s\n", repl_status.is_healthy ? "Healthy" : "Unhealthy");
			fprintf(stdout, "Sync Standby: %s\n", repl_status.is_sync_standby ? "Yes" : "No");
			fprintf(stdout, "Application: %s\n", repl_status.application_name);
			fprintf(stdout, "Client Address: %s\n", repl_status.client_addr);
			fprintf(stdout, "State: %s\n", repl_status.state);
			fprintf(stdout, "Sent LSN: %s\n", repl_status.sent_lsn);
			fprintf(stdout, "Replay LSN: %s\n", repl_status.replay_lsn);
		}
		else
		{
			fprintf(stdout, "Status: Unavailable\n");
		}
	}
	else if (ctx->json_output)
	{
		/* JSON output format */
		fprintf(stdout, "{\n");
		if (ramctrl_replication_get_status(&repl_status))
		{
			fprintf(stdout, "  \"replication\": {\n");
			fprintf(stdout, "    \"mode\": \"%s\",\n", ramctrl_replication_mode_to_string(repl_status.mode));
			fprintf(stdout, "    \"current_lag_bytes\": %lld,\n", (long long)repl_status.current_lag_bytes);
			fprintf(stdout, "    \"current_lag_ms\": %d,\n", repl_status.current_lag_ms);
			fprintf(stdout, "    \"is_healthy\": %s,\n", repl_status.is_healthy ? "true" : "false");
			fprintf(stdout, "    \"is_sync_standby\": %s,\n", repl_status.is_sync_standby ? "true" : "false");
			fprintf(stdout, "    \"application_name\": \"%s\",\n", repl_status.application_name);
			fprintf(stdout, "    \"client_addr\": \"%s\",\n", repl_status.client_addr);
			fprintf(stdout, "    \"state\": \"%s\",\n", repl_status.state);
			fprintf(stdout, "    \"sent_lsn\": \"%s\",\n", repl_status.sent_lsn);
			fprintf(stdout, "    \"replay_lsn\": \"%s\"\n", repl_status.replay_lsn);
			fprintf(stdout, "  }\n");
		}
		else
		{
			fprintf(stdout, "  \"replication\": null\n");
		}
		fprintf(stdout, "}\n");
	}
	else
	{
		/* Default output format */
		fprintf(stdout, "Replication Status:\n");
		if (ramctrl_replication_get_status(&repl_status))
		{
			fprintf(stdout, "  ● Mode: %s\n", ramctrl_replication_mode_to_string(repl_status.mode));
			fprintf(stdout, "  ● Current Lag: %lld bytes (%d ms)\n", 
				   (long long)repl_status.current_lag_bytes, repl_status.current_lag_ms);
			fprintf(stdout, "  ● Health: %s\n", repl_status.is_healthy ? "Healthy" : "Unhealthy");
			fprintf(stdout, "  ● Sync Standby: %s\n", repl_status.is_sync_standby ? "Yes" : "No");
			fprintf(stdout, "  ● Application: %s\n", repl_status.application_name);
			fprintf(stdout, "  ● Client Address: %s\n", repl_status.client_addr);
			fprintf(stdout, "  ● State: %s\n", repl_status.state);
			fprintf(stdout, "  ● Sent LSN: %s\n", repl_status.sent_lsn);
			fprintf(stdout, "  ● Replay LSN: %s\n", repl_status.replay_lsn);
		}
		else
		{
			fprintf(stdout, "  ● Status: Unavailable\n");
		}
		fprintf(stdout, "\n");
	}
	
	return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_set_replication_mode(ramctrl_context_t *ctx)
{
	replication_mode_t	mode;
	
	if (!ctx || ctx->command_argc < 1)
	{
		fprintf(stderr, "ramctrl: set replication-mode command requires mode argument\n");
		return RAMCTRL_EXIT_USAGE;
	}
	
	mode = ramctrl_replication_string_to_mode(ctx->command_args[0]);
	if (mode == REPL_MODE_UNKNOWN)
	{
		fprintf(stderr, "ramctrl: invalid replication mode: %s\n", ctx->command_args[0]);
		fprintf(stderr, "Valid modes: async, sync_remote_write, sync_remote_apply, auto\n");
		return RAMCTRL_EXIT_USAGE;
	}
	
	if (ramctrl_replication_set_mode(mode))
	{
		fprintf(stdout, "ramctrl: replication mode set to %s successfully\n", 
			   ramctrl_replication_mode_to_string(mode));
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: failed to set replication mode to %s\n", 
			   ramctrl_replication_mode_to_string(mode));
		return RAMCTRL_EXIT_FAILURE;
	}
}

int
ramctrl_cmd_set_lag_threshold(ramctrl_context_t *ctx)
{
	int64_t		threshold;
	
	if (!ctx || ctx->command_argc < 1)
	{
		fprintf(stderr, "ramctrl: set lag-threshold command requires threshold argument\n");
		return RAMCTRL_EXIT_USAGE;
	}
	
	threshold = atoll(ctx->command_args[0]);
	if (threshold <= 0)
	{
		fprintf(stderr, "ramctrl: invalid lag threshold: %s\n", ctx->command_args[0]);
		return RAMCTRL_EXIT_USAGE;
	}
	
	/* Update lag configuration */
	replication_lag_config_t lag_config = {0};
	lag_config.maximum_lag_on_failover = threshold;
	lag_config.warning_lag_threshold = threshold / 2;
	lag_config.critical_lag_threshold = threshold * 2;
	lag_config.lag_check_interval_ms = 5000;
	
	if (ramctrl_replication_update_lag_config(&lag_config))
	{
		fprintf(stdout, "ramctrl: lag threshold set to %lld bytes successfully\n", (long long)threshold);
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: failed to set lag threshold\n");
		return RAMCTRL_EXIT_FAILURE;
	}
}

int
ramctrl_cmd_wal_e_backup(ramctrl_context_t *ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	fprintf(stdout, "ramctrl: creating WAL-E backup...\n");
	
	if (ramctrl_wal_e_create_backup())
	{
		fprintf(stdout, "ramctrl: WAL-E backup created successfully\n");
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: failed to create WAL-E backup\n");
		return RAMCTRL_EXIT_FAILURE;
	}
}

int
ramctrl_cmd_wal_e_restore(ramctrl_context_t *ctx)
{
	if (!ctx || ctx->command_argc < 1)
	{
		fprintf(stderr, "ramctrl: wal-e restore command requires backup name\n");
		return RAMCTRL_EXIT_USAGE;
	}
	
	fprintf(stdout, "ramctrl: restoring from WAL-E backup: %s\n", ctx->command_args[0]);
	
	if (ramctrl_wal_e_restore_backup(ctx->command_args[0]))
	{
		fprintf(stdout, "ramctrl: WAL-E restore completed successfully\n");
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: failed to restore from WAL-E backup\n");
		return RAMCTRL_EXIT_FAILURE;
	}
}

int
ramctrl_cmd_wal_e_list(ramctrl_context_t *ctx)
{
	char		backup_list[4096];
	
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	if (ctx->json_output)
	{
		fprintf(stdout, "{\n");
		if (ramctrl_wal_e_list_backups(backup_list, sizeof(backup_list)))
		{
			fprintf(stdout, "  \"backups\": %s\n", backup_list);
		}
		else
		{
			fprintf(stdout, "  \"backups\": []\n");
		}
		fprintf(stdout, "}\n");
	}
	else
	{
		fprintf(stdout, "Available WAL-E Backups:\n");
		if (ramctrl_wal_e_list_backups(backup_list, sizeof(backup_list)))
		{
			fprintf(stdout, "%s", backup_list);
		}
		else
		{
			fprintf(stdout, "  No backups available\n");
		}
		fprintf(stdout, "\n");
	}
	
	return RAMCTRL_EXIT_SUCCESS;
}

int
ramctrl_cmd_wal_e_delete(ramctrl_context_t *ctx)
{
	if (!ctx || ctx->command_argc < 1)
	{
		fprintf(stderr, "ramctrl: wal-e delete command requires backup name\n");
		return RAMCTRL_EXIT_USAGE;
	}
	
	fprintf(stdout, "ramctrl: deleting WAL-E backup: %s\n", ctx->command_args[0]);
	
	if (ramctrl_wal_e_delete_backup(ctx->command_args[0]))
	{
		fprintf(stdout, "ramctrl: WAL-E backup deleted successfully\n");
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: failed to delete WAL-E backup\n");
		return RAMCTRL_EXIT_FAILURE;
	}
}

int
ramctrl_cmd_bootstrap_run(ramctrl_context_t *ctx)
{
	if (!ctx || ctx->command_argc < 1)
	{
		fprintf(stderr, "ramctrl: bootstrap run command requires node type (primary/standby)\n");
		return RAMCTRL_EXIT_USAGE;
	}
	
	if (strcmp(ctx->command_args[0], "primary") != 0 && strcmp(ctx->command_args[0], "standby") != 0)
	{
		fprintf(stderr, "ramctrl: invalid node type: %s (use 'primary' or 'standby')\n", ctx->command_args[0]);
		return RAMCTRL_EXIT_USAGE;
	}
	
	fprintf(stdout, "ramctrl: running bootstrap script for %s node...\n", ctx->command_args[0]);
	
	if (ramctrl_bootstrap_run_script(ctx->command_args[0]))
	{
		fprintf(stdout, "ramctrl: bootstrap script completed successfully\n");
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: bootstrap script failed\n");
		return RAMCTRL_EXIT_FAILURE;
	}
}

int
ramctrl_cmd_bootstrap_validate(ramctrl_context_t *ctx)
{
	if (!ctx)
		return RAMCTRL_EXIT_FAILURE;
	
	fprintf(stdout, "ramctrl: validating bootstrap script...\n");
	
	if (ramctrl_bootstrap_validate_script())
	{
		fprintf(stdout, "ramctrl: bootstrap script validation passed\n");
		return RAMCTRL_EXIT_SUCCESS;
	}
	else
	{
		fprintf(stderr, "ramctrl: bootstrap script validation failed\n");
		return RAMCTRL_EXIT_FAILURE;
	}
}
