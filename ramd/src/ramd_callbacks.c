/*-------------------------------------------------------------------------
 *
 * ramd_callbacks.c
 *		Custom callback system for automation hooks
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>

#include "ramd_callbacks.h"
#include "ramd_logging.h"
#include "ramd_config.h"

/* Global callback registry */
static ramd_callback_config_t g_callbacks[RAMD_MAX_TOTAL_CALLBACKS];
static int32_t g_callback_count = 0;
static pthread_mutex_t g_callback_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool g_callbacks_initialized = false;

/* Event name mapping */
static const struct
{
	ramd_callback_event_t event;
	const char* name;
} g_event_names[] = {
    {RAMD_CALLBACK_PRE_FAILOVER, "pre_failover"},
    {RAMD_CALLBACK_POST_FAILOVER, "post_failover"},
    {RAMD_CALLBACK_PRE_PROMOTE, "pre_promote"},
    {RAMD_CALLBACK_POST_PROMOTE, "post_promote"},
    {RAMD_CALLBACK_PRE_DEMOTE, "pre_demote"},
    {RAMD_CALLBACK_POST_DEMOTE, "post_demote"},
    {RAMD_CALLBACK_NODE_UNHEALTHY, "node_unhealthy"},
    {RAMD_CALLBACK_NODE_HEALTHY, "node_healthy"},
    {RAMD_CALLBACK_CLUSTER_DEGRADED, "cluster_degraded"},
    {RAMD_CALLBACK_CLUSTER_HEALTHY, "cluster_healthy"},
    {RAMD_CALLBACK_PRIMARY_LOST, "primary_lost"},
    {RAMD_CALLBACK_PRIMARY_ELECTED, "primary_elected"},
    {RAMD_CALLBACK_REPLICA_CONNECTED, "replica_connected"},
    {RAMD_CALLBACK_REPLICA_DISCONNECTED, "replica_disconnected"},
    {RAMD_CALLBACK_MAINTENANCE_START, "maintenance_start"},
    {RAMD_CALLBACK_MAINTENANCE_END, "maintenance_end"},
    {RAMD_CALLBACK_CONFIG_RELOAD, "config_reload"},
    {RAMD_CALLBACK_CUSTOM, "custom"}};

bool ramd_callbacks_init(void)
{
	pthread_mutex_lock(&g_callback_mutex);

	if (g_callbacks_initialized)
	{
		pthread_mutex_unlock(&g_callback_mutex);
		return true;
	}

	/* Initialize callback registry */
	memset(g_callbacks, 0, sizeof(g_callbacks));
	g_callback_count = 0;
	g_callbacks_initialized = true;

	pthread_mutex_unlock(&g_callback_mutex);

	ramd_log_info("Callback system initialized");
	return true;
}


void ramd_callbacks_cleanup(void)
{
	pthread_mutex_lock(&g_callback_mutex);

	if (!g_callbacks_initialized)
	{
		pthread_mutex_unlock(&g_callback_mutex);
		return;
	}

	memset(g_callbacks, 0, sizeof(g_callbacks));
	g_callback_count = 0;
	g_callbacks_initialized = false;

	pthread_mutex_unlock(&g_callback_mutex);

	ramd_log_info("Callback system cleaned up");
}


bool ramd_callback_register(ramd_callback_event_t event,
                            const char* script_path, bool async,
                            int32_t timeout_seconds, bool abort_on_failure)
{
	ramd_callback_config_t* callback;

	if (!script_path || strlen(script_path) == 0)
	{
		ramd_log_error("Callback registration failed: empty script path");
		return false;
	}

	/* Check if script exists and is executable */
	if (access(script_path, X_OK) != 0)
	{
		ramd_log_error(
		    "Callback registration failed: script '%s' not executable: %s",
		    script_path, strerror(errno));
		return false;
	}

	pthread_mutex_lock(&g_callback_mutex);

	if (g_callback_count >= RAMD_MAX_TOTAL_CALLBACKS)
	{
		pthread_mutex_unlock(&g_callback_mutex);
		ramd_log_error(
		    "Callback registration failed: maximum callbacks reached (%d)",
		    RAMD_MAX_TOTAL_CALLBACKS);
		return false;
	}

	/* Check for duplicate */
	for (int i = 0; i < g_callback_count; i++)
	{
		if (g_callbacks[i].event == event &&
		    strcmp(g_callbacks[i].script_path, script_path) == 0)
		{
			pthread_mutex_unlock(&g_callback_mutex);
			ramd_log_warning("Callback registration: script '%s' already "
			                 "registered for event '%s'",
			                 script_path, ramd_callback_event_to_string(event));
			return true; /* Already registered */
		}
	}

	/* Add new callback */
	callback = &g_callbacks[g_callback_count];
	callback->event = event;
	strncpy(callback->script_path, script_path,
	        sizeof(callback->script_path) - 1);
	callback->script_path[sizeof(callback->script_path) - 1] = '\0';
	callback->enabled = true;
	callback->async = async;
	callback->timeout_seconds = timeout_seconds > 0 ? timeout_seconds : 30;
	callback->log_output = true;
	callback->abort_on_failure = abort_on_failure;

	g_callback_count++;

	pthread_mutex_unlock(&g_callback_mutex);

	ramd_log_info(
	    "Callback registered: event='%s', script='%s', async=%s, timeout=%d",
	    ramd_callback_event_to_string(event), script_path,
	    async ? "true" : "false", callback->timeout_seconds);

	return true;
}


bool ramd_callback_unregister(ramd_callback_event_t event,
                              const char* script_path)
{
	int i, j;
	bool found = false;

	if (!script_path)
	{
		ramd_log_error("Callback unregistration failed: empty script path");
		return false;
	}

	pthread_mutex_lock(&g_callback_mutex);

	/* Find and remove callback */
	for (i = 0; i < g_callback_count; i++)
	{
		if (g_callbacks[i].event == event &&
		    strcmp(g_callbacks[i].script_path, script_path) == 0)
		{
			/* Shift remaining callbacks down */
			for (j = i; j < g_callback_count - 1; j++)
			{
				g_callbacks[j] = g_callbacks[j + 1];
			}
			g_callback_count--;
			found = true;
			break;
		}
	}

	pthread_mutex_unlock(&g_callback_mutex);

	if (found)
	{
		ramd_log_info("Callback unregistered: event='%s', script='%s'",
		              ramd_callback_event_to_string(event), script_path);
	}
	else
	{
		ramd_log_warning(
		    "Callback unregistration: script '%s' not found for event '%s'",
		    script_path, ramd_callback_event_to_string(event));
	}

	return found;
}


static bool ramd_callback_execute_script(ramd_callback_config_t* callback,
                                         ramd_callback_context_t* context)
{
	pid_t pid;
	int status;
	int exit_status;
	char* argv[4];
	char context_json[2048];
	/* Implement script output capture for logging */
	FILE* script_fp;
	char output_line[256];

	/* Execute script and capture output */
	script_fp = popen(callback->script_path, "r");
	if (script_fp)
	{
		ramd_log_debug("Executing callback script: %s", callback->script_path);

		/* Read and log script output */
		while (fgets(output_line, sizeof(output_line), script_fp) != NULL)
		{
			/* Remove trailing newline */
			output_line[strcspn(output_line, "\n")] = 0;
			ramd_log_info("Script output: %s", output_line);
		}

		exit_status = pclose(script_fp);
		if (exit_status == 0)
		{
			ramd_log_info("Callback script executed successfully");
			return true;
		}
		else
		{
			ramd_log_error("Callback script failed with exit code: %d",
			               exit_status);
			return false;
		}
	}
	else
	{
		ramd_log_error("Failed to execute callback script: %s",
		               callback->script_path);
		return false;
	}
	/* FILE *script_output = NULL; */
	/* char output_buffer[1024]; */

	/* Prepare context as JSON */
	snprintf(context_json, sizeof(context_json),
	         "{"
	         "\"event\":\"%s\","
	         "\"node_id\":%d,"
	         "\"node_name\":\"%s\","
	         "\"timestamp\":%ld,"
	         "\"cluster_id\":%d,"
	         "\"cluster_name\":\"%s\","
	         "\"data\":%s"
	         "}",
	         ramd_callback_event_to_string(context->event), context->node_id,
	         context->node_name, context->timestamp, context->cluster_id,
	         context->cluster_name,
	         context->event_data[0] ? context->event_data : "{}");

	ramd_log_debug("Executing callback: %s", callback->script_path);

	/* Fork and execute script */
	pid = fork();
	if (pid == -1)
	{
		ramd_log_error("Callback execution failed: fork error: %s",
		               strerror(errno));
		return false;
	}
	else if (pid == 0)
	{
		/* Child process */
		argv[0] = callback->script_path;
		argv[1] = context_json;
		argv[2] = NULL;

		/* Set environment variables */
		setenv("RAMD_EVENT", ramd_callback_event_to_string(context->event), 1);
		setenv("RAMD_NODE_ID", "0", 1); /* Convert node_id to string */
		setenv("RAMD_NODE_NAME", context->node_name, 1);
		setenv("RAMD_CLUSTER_ID", "0", 1); /* Convert cluster_id to string */
		setenv("RAMD_CLUSTER_NAME", context->cluster_name, 1);

		execv(callback->script_path, argv);

		/* If we get here, execv failed */
		fprintf(stderr, "Callback execution failed: execv error: %s\n",
		        strerror(errno));
		_exit(1);
	}
	else
	{
		/* Parent process */
		if (callback->async)
		{
			/* For async callbacks, don't wait */
			ramd_log_debug("Callback '%s' started asynchronously (pid=%d)",
			               callback->script_path, pid);
			return true;
		}
		else
		{
			/* For sync callbacks, wait with timeout */
			int wait_result;
			time_t start_time = time(NULL);

			while (true)
			{
				wait_result = waitpid(pid, &status, WNOHANG);

				if (wait_result == pid)
				{
					/* Process finished */
					break;
				}
				else if (wait_result == -1)
				{
					ramd_log_error(
					    "Callback execution failed: waitpid error: %s",
					    strerror(errno));
					return false;
				}
				else if (time(NULL) - start_time >= callback->timeout_seconds)
				{
					/* Timeout - kill the process */
					ramd_log_warning("Callback '%s' timed out after %d "
					                 "seconds, killing process",
					                 callback->script_path,
					                 callback->timeout_seconds);
					kill(pid, SIGTERM);
					sleep(2);
					kill(pid, SIGKILL);
					waitpid(pid, &status, 0);
					return false;
				}

				/* Wait a bit before checking again */
				usleep(100000); /* 100ms */
			}

			/* Check exit status */
			if (WIFEXITED(status))
			{
				int exit_code = WEXITSTATUS(status);
				if (exit_code == 0)
				{
					ramd_log_debug("Callback '%s' completed successfully",
					               callback->script_path);
					return true;
				}
				else
				{
					ramd_log_error("Callback '%s' failed with exit code %d",
					               callback->script_path, exit_code);
					return false;
				}
			}
			else if (WIFSIGNALED(status))
			{
				ramd_log_error("Callback '%s' killed by signal %d",
				               callback->script_path, WTERMSIG(status));
				return false;
			}
			else
			{
				ramd_log_error("Callback '%s' terminated abnormally",
				               callback->script_path);
				return false;
			}
		}
	}

	return true;
}


bool ramd_callback_execute_all(ramd_callback_event_t event,
                               ramd_callback_context_t* context)
{
	int i;
	bool all_success = true;
	int executed_count = 0;

	if (!g_callbacks_initialized)
	{
		ramd_log_debug("Callbacks not initialized, skipping event '%s'",
		               ramd_callback_event_to_string(event));
		return true;
	}

	pthread_mutex_lock(&g_callback_mutex);

	/* Execute all callbacks for this event */
	for (i = 0; i < g_callback_count; i++)
	{
		ramd_callback_config_t* callback = &g_callbacks[i];

		if (callback->event != event || !callback->enabled)
			continue;

		executed_count++;

		bool success = ramd_callback_execute_script(callback, context);

		if (!success)
		{
			all_success = false;

			if (callback->abort_on_failure)
			{
				ramd_log_error("Callback '%s' failed and abort_on_failure is "
				               "set, aborting operation",
				               callback->script_path);
				break;
			}
		}
	}

	pthread_mutex_unlock(&g_callback_mutex);

	if (executed_count > 0)
	{
		ramd_log_info("Executed %d callbacks for event '%s', success=%s",
		              executed_count, ramd_callback_event_to_string(event),
		              all_success ? "true" : "false");
	}

	return all_success;
}


const char* ramd_callback_event_to_string(ramd_callback_event_t event)
{
	for (size_t i = 0; i < sizeof(g_event_names) / sizeof(g_event_names[0]);
	     i++)
	{
		if (g_event_names[i].event == event)
			return g_event_names[i].name;
	}
	return "unknown";
}


ramd_callback_event_t ramd_callback_string_to_event(const char* event_str)
{
	if (!event_str)
		return RAMD_CALLBACK_CUSTOM;

	for (size_t i = 0; i < sizeof(g_event_names) / sizeof(g_event_names[0]);
	     i++)
	{
		if (strcmp(g_event_names[i].name, event_str) == 0)
			return g_event_names[i].event;
	}
	return RAMD_CALLBACK_CUSTOM;
}


void ramd_callback_context_init(ramd_callback_context_t* context,
                                ramd_callback_event_t event)
{
	if (!context)
		return;

	memset(context, 0, sizeof(ramd_callback_context_t));
	context->event = event;
	context->timestamp = time(NULL);
	context->node_id = -1;
	context->cluster_id = -1;
}


void ramd_callback_context_set_node(ramd_callback_context_t* context,
                                    int32_t node_id, const char* node_name)
{
	if (!context)
		return;

	context->node_id = node_id;
	if (node_name)
	{
		strncpy(context->node_name, node_name, sizeof(context->node_name) - 1);
		context->node_name[sizeof(context->node_name) - 1] = '\0';
	}
}


void ramd_callback_context_set_data(ramd_callback_context_t* context,
                                    const char* json_data)
{
	if (!context || !json_data)
		return;

	strncpy(context->event_data, json_data, sizeof(context->event_data) - 1);
	context->event_data[sizeof(context->event_data) - 1] = '\0';
}


bool ramd_callback_trigger_failover(int32_t old_primary_id,
                                    int32_t new_primary_id)
{
	ramd_callback_context_t context;
	char data[512];

	/* Pre-failover callbacks */
	ramd_callback_context_init(&context, RAMD_CALLBACK_PRE_FAILOVER);
	snprintf(data, sizeof(data),
	         "{\"old_primary_id\":%d,\"new_primary_id\":%d}", old_primary_id,
	         new_primary_id);
	ramd_callback_context_set_data(&context, data);

	bool pre_success =
	    ramd_callback_execute_all(RAMD_CALLBACK_PRE_FAILOVER, &context);
	if (!pre_success)
	{
		ramd_log_warning(
		    "Pre-failover callbacks failed, continuing with failover");
	}

	/* Post-failover callbacks (typically called after failover completion) */
	ramd_callback_context_init(&context, RAMD_CALLBACK_POST_FAILOVER);
	ramd_callback_context_set_data(&context, data);

	bool post_success =
	    ramd_callback_execute_all(RAMD_CALLBACK_POST_FAILOVER, &context);

	return pre_success && post_success;
}


bool ramd_callback_trigger_promotion(int32_t node_id)
{
	ramd_callback_context_t context;

	/* Pre-promotion callbacks */
	ramd_callback_context_init(&context, RAMD_CALLBACK_PRE_PROMOTE);
	ramd_callback_context_set_node(&context, node_id, "");

	bool pre_success =
	    ramd_callback_execute_all(RAMD_CALLBACK_PRE_PROMOTE, &context);
	if (!pre_success)
	{
		ramd_log_warning(
		    "Pre-promotion callbacks failed, continuing with promotion");
	}

	/* Post-promotion callbacks (typically called after promotion completion) */
	ramd_callback_context_init(&context, RAMD_CALLBACK_POST_PROMOTE);
	ramd_callback_context_set_node(&context, node_id, "");

	bool post_success =
	    ramd_callback_execute_all(RAMD_CALLBACK_POST_PROMOTE, &context);

	return pre_success && post_success;
}


bool ramd_callback_trigger_health_change(int32_t node_id, bool healthy)
{
	ramd_callback_context_t context;
	ramd_callback_event_t event =
	    healthy ? RAMD_CALLBACK_NODE_HEALTHY : RAMD_CALLBACK_NODE_UNHEALTHY;

	ramd_callback_context_init(&context, event);
	ramd_callback_context_set_node(&context, node_id, "");

	return ramd_callback_execute_all(event, &context);
}


bool ramd_callback_trigger_cluster_state_change(bool healthy)
{
	ramd_callback_context_t context;
	ramd_callback_event_t event = healthy ? RAMD_CALLBACK_CLUSTER_HEALTHY
	                                      : RAMD_CALLBACK_CLUSTER_DEGRADED;

	ramd_callback_context_init(&context, event);

	return ramd_callback_execute_all(event, &context);
}
