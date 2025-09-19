/*-------------------------------------------------------------------------
 *
 * comm.c
 *		Communication and message handling for pgraft extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "../include/pgraft.h"
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/select.h>

typedef struct PgraftCommState
{
	int			socket_fd;
	int			port;
	char	   *address;
	uint64_t	node_id;
	bool		is_listening;
	bool		is_connected;
	int			connected_nodes;
	int			connections[10];
	pthread_t	comm_thread;
	pthread_mutex_t comm_mutex;
	pthread_cond_t comm_cond;
	bool		shutdown_requested;
} pgraft_comm_state_t;

static pgraft_comm_state_t *comm_state = NULL;
static uint64_t g_max_nodes = 10;

static bool is_node_connected(uint64_t node_id);

static bool
is_node_connected(uint64_t node_id)
{
	if (comm_state && comm_state->connections[node_id] > 0)
	{
		return true;
	}
	return false;
}

int
pgraft_get_connection_to_node(uint64_t node_id)
{
	if (comm_state && comm_state->connections[node_id] > 0)
	{
		return comm_state->connections[node_id];
	}
	return -1;
}

int
pgraft_comm_init(const char *address, int port)
{
	if (comm_state != NULL)
	{
		elog(WARNING, "pgraft_comm_init: communication already initialized");
		return 0;
	}

	comm_state = (pgraft_comm_state_t *) palloc(sizeof(pgraft_comm_state_t));
	if (comm_state == NULL)
	{
		elog(ERROR, "pgraft_comm_init: failed to allocate communication state");
		return -1;
	}

	memset(comm_state, 0, sizeof(pgraft_comm_state_t));
	comm_state->port = port;
	comm_state->address = pstrdup(address);
	comm_state->socket_fd = -1;
	comm_state->is_listening = false;
	comm_state->is_connected = false;
	comm_state->connected_nodes = 0;
	comm_state->shutdown_requested = false;

	if (pthread_mutex_init(&comm_state->comm_mutex, NULL) != 0)
	{
		elog(ERROR, "pgraft_comm_init: failed to initialize mutex");
		pfree(comm_state->address);
		pfree(comm_state);
		comm_state = NULL;
		return -1;
	}

	if (pthread_cond_init(&comm_state->comm_cond, NULL) != 0)
	{
		elog(ERROR, "pgraft_comm_init: failed to initialize condition variable");
		pthread_mutex_destroy(&comm_state->comm_mutex);
		pfree(comm_state->address);
		pfree(comm_state);
		comm_state = NULL;
		return -1;
	}

	elog(INFO, "pgraft_comm_init: communication subsystem initialized on %s:%d",
		 address, port);
	return 0;
}

int
pgraft_comm_start(void)
{
	if (comm_state == NULL)
	{
		elog(ERROR, "pgraft_comm_start: communication not initialized");
		return -1;
	}

	if (comm_state->is_listening)
	{
		elog(WARNING, "pgraft_comm_start: communication already started");
		return 0;
	}

	comm_state->is_listening = true;
	elog(INFO, "pgraft_comm_start: communication thread started");
	return 0;
}

static void
pgraft_comm_stop(void)
{
	if (comm_state == NULL || !comm_state->is_listening)
	{
		return;
	}

	pthread_mutex_lock(&comm_state->comm_mutex);
	comm_state->shutdown_requested = true;
	pthread_cond_signal(&comm_state->comm_cond);
	pthread_mutex_unlock(&comm_state->comm_mutex);

	pthread_join(comm_state->comm_thread, NULL);

	if (comm_state->socket_fd >= 0)
	{
		close(comm_state->socket_fd);
		comm_state->socket_fd = -1;
	}

	comm_state->is_listening = false;
	elog(INFO, "pgraft_comm_stop: communication thread stopped");
}

static void
pgraft_comm_cleanup(void)
{
	if (comm_state == NULL)
	{
		return;
	}

	pgraft_comm_stop();

	pthread_mutex_destroy(&comm_state->comm_mutex);
	pthread_cond_destroy(&comm_state->comm_cond);

	if (comm_state->address != NULL)
	{
		pfree(comm_state->address);
	}

	pfree(comm_state);
	comm_state = NULL;

	elog(INFO, "pgraft_comm_cleanup: communication subsystem cleaned up");
}

int
pgraft_comm_shutdown(void)
{
	pgraft_comm_cleanup();
	return 0;
}

bool
pgraft_comm_initialized(void)
{
	return (comm_state != NULL);
}

int
pgraft_comm_get_active_connections(void)
{
	if (comm_state == NULL)
	{
		return 0;
	}
	return comm_state->connected_nodes;
}

pgraft_message_t *
pgraft_comm_receive_message(void)
{
	pgraft_message_t *result;
	int healthy_count;

	if (comm_state == NULL)
	{
		return NULL;
	}

	healthy_count = 0;
	for (int i = 0; i < g_max_nodes; i++)
	{
		if (is_node_connected(i))
		{
			healthy_count++;
		}
	}

	if (healthy_count == 0)
	{
		return NULL;
	}

	result = (pgraft_message_t *) palloc(sizeof(pgraft_message_t));
	if (result == NULL)
	{
		return NULL;
	}

	result->msg_type = 0;
	result->from_node = 0;
	result->to_node = 0;
	result->term = 0;
	result->index = 0;
	result->data_size = 0;
	result->data = NULL;

	return result;
}

void
pgraft_comm_free_message(pgraft_message_t *msg)
{
	if (msg == NULL)
	{
		return;
	}

	if (msg->data)
	{
		pfree(msg->data);
	}

	pfree(msg);
}

int
pgraft_send_message(uint64_t to_node, int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index)
{
	pgraft_message_t message;
	size_t serialized_size;
	char *serialized_buffer;
	int sock_fd;
	ssize_t bytes_sent;

	if (comm_state == NULL)
	{
		elog(ERROR, "pgraft_send_message: communication not initialized");
		return -1;
	}

	serialized_size = sizeof(pgraft_message_t);
	if (data) serialized_size += data_size;

	serialized_buffer = (char*)palloc(serialized_size);

	message.msg_type = msg_type;
	message.from_node = 0;
	message.to_node = to_node;
	message.term = term;
	message.index = index;
	message.data_size = data_size;
	message.data = (data ? palloc(data_size) : NULL);
	if (message.data && data) memcpy(message.data, data, data_size);

	memcpy(serialized_buffer, &message, sizeof(pgraft_message_t));
	if (message.data) memcpy(serialized_buffer + sizeof(pgraft_message_t), message.data, message.data_size);

	sock_fd = pgraft_get_connection_to_node(to_node);
	if (sock_fd < 0) {
		elog(ERROR, "pgraft_send_message: no connection to node %llu", (unsigned long long)to_node);
		if (message.data) pfree(message.data);
		pfree(serialized_buffer);
		return -1;
	}

	bytes_sent = send(sock_fd, serialized_buffer, serialized_size, 0);
	if (bytes_sent != serialized_size) {
		elog(WARNING, "pgraft_send_message: partial send to node %llu", (unsigned long long)to_node);
		if (message.data) pfree(message.data);
		pfree(serialized_buffer);
		close(sock_fd);
		return -1;
	}

	if (message.data) pfree(message.data);
	pfree(serialized_buffer);
	close(sock_fd);
	return 0;
}

int
pgraft_broadcast_message(int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index)
{
	pgraft_message_t message;
	size_t serialized_size;
	char *serialized_buffer;
	int sock_fd;
	ssize_t bytes_sent;

	if (comm_state == NULL)
	{
		elog(ERROR, "pgraft_broadcast_message: communication not initialized");
		return -1;
	}

	serialized_size = sizeof(pgraft_message_t);
	if (data) serialized_size += data_size;

	serialized_buffer = (char*)palloc(serialized_size);

	message.msg_type = msg_type;
	message.from_node = comm_state->node_id;
	message.to_node = 0;
	message.term = term;
	message.index = index;
	message.data_size = data_size;
	message.data = (data ? palloc(data_size) : NULL);
	if (message.data && data) memcpy(message.data, data, data_size);

	memcpy(serialized_buffer, &message, sizeof(pgraft_message_t));
	if (message.data) memcpy(serialized_buffer + sizeof(pgraft_message_t), message.data, message.data_size);

	for (uint64_t node_id = 1; node_id <= g_max_nodes; node_id++) {
		if (is_node_connected(node_id)) {
			sock_fd = pgraft_get_connection_to_node(node_id);
			if (sock_fd >= 0) {
				bytes_sent = send(sock_fd, serialized_buffer, serialized_size, 0);
				if (bytes_sent != serialized_size) {
					elog(WARNING, "pgraft_broadcast_message: partial send to node %llu", (unsigned long long)node_id);
				}
				close(sock_fd);
			}
		}
	}

	if (message.data) pfree(message.data);
	pfree(serialized_buffer);
	return 0;
}

int
pgraft_add_node_comm(uint64_t node_id, const char *address, int port)
{
	struct sockaddr_in server_addr;
	int sock_fd;
	int opt;
	int flags;
	int connect_result;
	int select_result;
	fd_set write_fds;
	struct timeval timeout;
	int error;
	socklen_t len;

	if (comm_state == NULL)
	{
		elog(ERROR, "pgraft_add_node_comm: communication not initialized");
		return -1;
	}

	elog(INFO, "pgraft_add_node_comm: adding node %llu at %s:%d",
		 (unsigned long long) node_id, address, port);

	if (comm_state)
	{
		sock_fd = socket(AF_INET, SOCK_STREAM, 0);

		if (sock_fd < 0)
		{
			elog(ERROR, "pgraft_add_node_comm: failed to create socket for node %llu", 
				 (unsigned long long) node_id);
			return -1;
		}

		memset(&server_addr, 0, sizeof(server_addr));
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(port);

		if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0)
		{
			elog(ERROR, "pgraft_add_node_comm: invalid address %s", address);
			close(sock_fd);
			return -1;
		}

		opt = 1;
		if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		{
			elog(WARNING, "pgraft_add_node_comm: failed to set socket options");
		}

		flags = fcntl(sock_fd, F_GETFL, 0);
		fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);

		connect_result = connect(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));

		if (connect_result < 0)
		{
			if (errno == EINPROGRESS)
			{
				FD_ZERO(&write_fds);
				FD_SET(sock_fd, &write_fds);
				timeout.tv_sec = 5;
				timeout.tv_usec = 0;

				select_result = select(sock_fd + 1, NULL, &write_fds, NULL, &timeout);

				if (select_result > 0)
				{
					len = sizeof(error);
					if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
					{
						elog(ERROR, "pgraft_add_node_comm: failed to get socket error");
						close(sock_fd);
						return -1;
					}

					if (error != 0)
					{
						elog(ERROR, "pgraft_add_node_comm: connection failed: %s", strerror(error));
						close(sock_fd);
						return -1;
					}
				}
				else
				{
					elog(ERROR, "pgraft_add_node_comm: connection timeout");
					close(sock_fd);
					return -1;
				}
			}
			else
			{
				char error_msg[256];
				snprintf(error_msg, sizeof(error_msg), "pgraft_add_node_comm: connection failed: %s", strerror(errno));
				elog(ERROR, "%s", error_msg);
				close(sock_fd);
				return -1;
			}
		}

		comm_state->connections[node_id] = sock_fd;
		comm_state->connected_nodes++;

		elog(INFO, "pgraft_add_node_comm: successfully connected to node %llu", (unsigned long long) node_id);
	}

	return 0;
}

int
pgraft_remove_node_comm(uint64_t node_id)
{
	if (comm_state == NULL)
	{
		elog(ERROR, "pgraft_remove_node_comm: communication not initialized");
		return -1;
	}

	if (comm_state->connections[node_id] > 0)
	{
		close(comm_state->connections[node_id]);
		comm_state->connections[node_id] = 0;
		comm_state->connected_nodes--;
		elog(INFO, "pgraft_remove_node_comm: removed node %llu", (unsigned long long) node_id);
	}

	return 0;
}

bool
pgraft_is_node_connected(uint64_t node_id)
{
	return is_node_connected(node_id);
}

int
pgraft_get_connected_nodes_count(void)
{
	return pgraft_comm_get_active_connections();
}

int
pgraft_get_healthy_nodes_count(void)
{
	int healthy_count = 0;

	if (comm_state == NULL)
	{
		return 0;
	}

	for (int i = 0; i < g_max_nodes; i++)
	{
		if (is_node_connected(i))
		{
			healthy_count++;
		}
	}

	return healthy_count;
}

int
pgraft_serialize_message(const pgraft_message_t *msg, char **buffer, size_t *buffer_size)
{
	size_t total_size;
	char *buf;

	if (msg == NULL || buffer == NULL || buffer_size == NULL)
	{
		return -1;
	}

	total_size = sizeof(pgraft_message_t) + msg->data_size;
	buf = (char *) palloc(total_size);
	if (buf == NULL)
	{
		return -1;
	}

	memcpy(buf, msg, sizeof(pgraft_message_t));
	if (msg->data && msg->data_size > 0)
	{
		memcpy(buf + sizeof(pgraft_message_t), msg->data, msg->data_size);
	}

	*buffer = buf;
	*buffer_size = total_size;
	return 0;
}