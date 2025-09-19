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

typedef struct pgraft_comm_state
{
    int socket_fd;
    int port;
    char *address;
    uint64_t node_id;
    bool is_listening;
    bool is_connected;
    int connected_nodes;
    int connections[10];  // Ensure this is correctly defined
    pthread_t comm_thread;
    pthread_mutex_t comm_mutex;
    pthread_cond_t comm_cond;
    bool shutdown_requested;
} pgraft_comm_state_t;  // Ensure this is the only definition

static pgraft_comm_state_t *comm_state = NULL;
uint64_t g_max_nodes = 10;

static bool is_node_connected(uint64_t node_id);
static int pgraft_get_connection_to_node(uint64_t node_id);

// Implement the new functions early
static bool is_node_connected(uint64_t node_id) {
    if (comm_state && comm_state->connections[node_id] > 0) {
        return true;
    }
    return false;
}

static int pgraft_get_connection_to_node(uint64_t node_id) {
    if (comm_state && comm_state->connections[node_id] > 0) {
        return comm_state->connections[node_id];
    }
    return -1;
}

typedef enum pgraft_msg_type
{
    PGRaft_MSG_APPEND_ENTRIES = 1,
    PGRaft_MSG_APPEND_ENTRIES_RESPONSE = 2,
    PGRaft_MSG_REQUEST_VOTE = 3,
    PGRaft_MSG_REQUEST_VOTE_RESPONSE = 4,
    PGRaft_MSG_INSTALL_SNAPSHOT = 5,
    PGRaft_MSG_INSTALL_SNAPSHOT_RESPONSE = 6,
    PGRaft_MSG_HEARTBEAT = 7,
    PGRaft_MSG_HEARTBEAT_RESPONSE = 8,
    PGRaft_MSG_LEADER_ELECTION = 9,
    PGRaft_MSG_LEADER_ELECTION_RESPONSE = 10,
    PGRaft_MSG_CLUSTER_JOIN = 11,
    PGRaft_MSG_CLUSTER_JOIN_RESPONSE = 12,
    PGRaft_MSG_CLUSTER_LEAVE = 13,
    PGRaft_MSG_CLUSTER_LEAVE_RESPONSE = 14,
    PGRaft_MSG_STATUS_REQUEST = 15,
    PGRaft_MSG_STATUS_RESPONSE = 16
} pgraft_msg_type_t;

/* pgraft_message_t is defined in pgraft.h */

typedef struct pgraft_append_entries_request
{
    int term;
    int leader_id;
    int prev_log_index;
    int prev_log_term;
    int leader_commit;
    int entry_count;
    char *entries;
} pgraft_append_entries_request_t;

typedef struct pgraft_append_entries_response
{
    int term;
    bool success;
    int next_index;
} pgraft_append_entries_response_t;

typedef struct pgraft_request_vote_request
{
    int term;
    int candidate_id;
    int last_log_index;
    int last_log_term;
} pgraft_request_vote_request_t;

typedef struct pgraft_request_vote_response
{
    int term;
    bool vote_granted;
} pgraft_request_vote_response_t;

typedef struct pgraft_heartbeat_request
{
    int term;
    int leader_id;
    int commit_index;
} pgraft_heartbeat_request_t;

typedef struct pgraft_heartbeat_response
{
    int term;
    bool success;
} pgraft_heartbeat_response_t;

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

/*
 * Send message to specific node
 */
int
pgraft_send_message(uint64_t to_node, int msg_type, const char* data, size_t data_size, uint64_t term, uint64_t index) {
    size_t serialized_size;
    char* serialized_buffer;
    pgraft_message_t message;
    
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
    
    int sock_fd = pgraft_get_connection_to_node(to_node);
    if (sock_fd < 0) {
        elog(ERROR, "pgraft_send_message: no connection to node %llu", (unsigned long long)to_node);
        if (message.data) pfree(message.data);
        pfree(serialized_buffer);
        return -1;
    }
    
    ssize_t bytes_sent = send(sock_fd, serialized_buffer, serialized_size, 0);
    if (bytes_sent != serialized_size) {
        elog(WARNING, "pgraft_send_message: partial send to node %llu", (unsigned long long)to_node);
        if (message.data) pfree(message.data);
        pfree(serialized_buffer);
        close(sock_fd);
            return -1;
    }
    close(sock_fd);
    
    elog(DEBUG1, "pgraft_send_message: message sent successfully to node %llu", (unsigned long long)to_node);
    if (message.data) pfree(message.data);
    pfree(serialized_buffer);
    return 0;
}

/*
 * Broadcast message to all nodes
 */
int
pgraft_broadcast_message(int msg_type, const char *data, size_t data_size, uint64_t term, uint64_t index)
{
    pgraft_message_t message;
    
    if (comm_state == NULL || !comm_state->is_listening)
    {
        elog(ERROR, "pgraft_broadcast_message: communication not initialized");
        return -1;
    }
    
    message.msg_type = msg_type;
    message.from_node = comm_state->node_id;
    message.to_node = 0;
    message.term = term;
    message.index = index;
    message.data_size = data_size;
    message.data = (data ? palloc(data_size) : NULL);
    if (message.data && data) memcpy(message.data, data, data_size);
    
    size_t serialized_size = sizeof(pgraft_message_t);
    if (data) serialized_size += data_size;
    char* serialized_buffer = (char*)palloc(serialized_size);
    
    memcpy(serialized_buffer, &message, sizeof(pgraft_message_t));
    if (message.data) memcpy(serialized_buffer + sizeof(pgraft_message_t), message.data, message.data_size);
    
    for (uint64_t node_id = 1; node_id <= g_max_nodes; node_id++) {
        if (is_node_connected(node_id)) {
            int sock_fd = pgraft_get_connection_to_node(node_id);
            if (sock_fd >= 0) {
                ssize_t bytes_sent = send(sock_fd, serialized_buffer, serialized_size, 0);
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

/*
 * Add node to communication
 */
int
pgraft_add_node_comm(uint64_t node_id, const char *address, int port)
{
    if (comm_state == NULL)
    {
        elog(ERROR, "pgraft_add_node_comm: communication not initialized");
        return -1;
    }
    
    /* Implement actual node addition to communication */
    elog(INFO, "pgraft_add_node_comm: adding node %llu at %s:%d", 
         (unsigned long long)node_id, address, port);
    
    /* Add node to connection pool */
    /* Implement real connection management */
    if (comm_state)
    {
        /* Create actual socket connection to the node */
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0)
        {
            elog(ERROR, "pgraft_add_node_comm: failed to create socket for node %llu", 
                 (unsigned long long)node_id);
            return -1;
        }
        
        /* Set up address structure */
        struct sockaddr_in server_addr;
        int opt, flags, connect_result, select_result;
        fd_set write_fds;
    struct timeval timeout;
        int error;
        socklen_t len;
        
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        
        /* Convert address string to binary */
        if (inet_pton(AF_INET, address, &server_addr.sin_addr) <= 0)
        {
            elog(ERROR, "pgraft_add_node_comm: invalid address %s for node %llu", 
                 address, (unsigned long long)node_id);
            close(sock_fd);
            return -1;
        }
        
        /* Set socket options */
        opt = 1;
        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            elog(WARNING, "pgraft_add_node_comm: failed to set socket options for node %llu", 
                 (unsigned long long)node_id);
        }
        
        /* Set non-blocking mode for connection attempt */
        flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        
        /* Attempt connection */
        connect_result = connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (connect_result < 0)
        {
            if (errno == EINPROGRESS)
            {
                /* Connection in progress, wait for completion */
                FD_ZERO(&write_fds);
                FD_SET(sock_fd, &write_fds);
                timeout.tv_sec = 5;  /* 5 second timeout */
                timeout.tv_usec = 0;
                
                select_result = select(sock_fd + 1, NULL, &write_fds, NULL, &timeout);
                if (select_result > 0)
                {
                    /* Check if connection was successful */
                    error = 0;
                    len = sizeof(error);
                    if (getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
                    {
                        elog(ERROR, "pgraft_add_node_comm: connection failed to node %llu at %s:%d", 
                             (unsigned long long)node_id, address, port);
                        close(sock_fd);
                        return -1;
                    }
                }
                else
                {
                    elog(ERROR, "pgraft_add_node_comm: connection timeout to node %llu at %s:%d", 
                         (unsigned long long)node_id, address, port);
                    close(sock_fd);
                    return -1;
                }
            }
            else
            {
                char error_msg[256];
                snprintf(error_msg, sizeof(error_msg), "failed to connect to node %llu at %s:%d: %s", 
                         (unsigned long long)node_id, address, port, strerror(errno));
                elog(ERROR, "pgraft_add_node_comm: %s", error_msg);
                close(sock_fd);
            return -1;
            }
        }
        
        /* Restore blocking mode */
        fcntl(sock_fd, F_SETFL, flags);
        
        /* Store connection in state (in real implementation, use a connection pool) */
        comm_state->connections[node_id] = sock_fd; // Store the socket file descriptor
        comm_state->connected_nodes++;
        
        /* Close the test connection (in real implementation, keep it open) */
        // close(sock_fd); // This line was commented out in the original file, keeping it commented.
    }
    else
    {
        elog(ERROR, "pgraft_add_node_comm: communication state not initialized");
        return -1;
    }
    
    elog(INFO, "pgraft_add_node_comm: successfully added node %llu to communication", 
         (unsigned long long)node_id);
    return 0;
}

/*
 * Remove node from communication
 */
int
pgraft_remove_node_comm(uint64_t node_id)
{
    if (comm_state == NULL)
    {
        elog(ERROR, "pgraft_remove_node_comm: communication not initialized");
        return -1;
    }
    
    /* Implement actual node removal from communication */
    elog(INFO, "pgraft_remove_node_comm: removing node %llu", (unsigned long long)node_id);
    
    /* Implement real connection removal with error handling */
    if (comm_state && comm_state->connections[node_id] > 0) {
        if (close(comm_state->connections[node_id]) == 0) {
            comm_state->connections[node_id] = -1;  // Mark as closed
            comm_state->connected_nodes--;
            elog(DEBUG1, "pgraft_remove_node_comm: removed node %llu from connection pool", (unsigned long long)node_id);
        } else {
            elog(WARNING, "pgraft_remove_node_comm: failed to close connection for node %llu", (unsigned long long)node_id);
        }
    } else {
        elog(WARNING, "pgraft_remove_node_comm: node %llu not in connection pool", (unsigned long long)node_id);
    }
    return 0;
}

/*
 * Serialize message for transmission
 */
static int
pgraft_serialize_message(const pgraft_message_t *msg, char **buffer, size_t *buffer_size)
{
    size_t required_size;
    
    if (!msg || !buffer || !buffer_size)
        return -1;
    
    /* Calculate required buffer size */
    required_size = sizeof(pgraft_message_t) + (msg->data ? msg->data_size : 0);
    
    *buffer = palloc(required_size);
    if (!*buffer)
        return -1;
    
    /* Copy message header */
    memcpy(*buffer, msg, sizeof(pgraft_message_t));
    
    /* Copy message data if present */
    if (msg->data && msg->data_size > 0)
    {
        memcpy(*buffer + sizeof(pgraft_message_t), msg->data, msg->data_size);
    }
    
    *buffer_size = required_size;
    return 0;
}

/*
 * Check if communication system is initialized
 */
bool
pgraft_comm_initialized(void)
{
    return (comm_state != NULL);
}

/*
 * Get number of active connections
 */
int
pgraft_comm_get_active_connections(void)
{
    if (!comm_state)
    {
        return 0;
    }
    
    /* Return actual connection count from state */
    return comm_state->connected_nodes;
}

/*
 * Get number of healthy nodes
 */
int
pgraft_get_healthy_nodes_count(void)
{
    if (!comm_state)
    {
        return 0;
    }
    
    int healthy_count = 0;
    for (uint64_t i = 1; i <= g_max_nodes; i++) {
        if (comm_state->connections[i] > 0 && is_node_connected(i)) {  // Use existing function to check
            healthy_count++;
        }
    }
    return healthy_count;
}

/*
 * Receive message from communication layer
 */
pgraft_message_t *
pgraft_comm_receive_message(void)
{
    if (!comm_state)
    {
        return NULL;
    }
    
    pgraft_message_t *result = (pgraft_message_t *)palloc(sizeof(pgraft_message_t));
    if (comm_state->socket_fd >= 0) {
        char buffer[1024];  // Buffer for incoming data
        ssize_t bytes_received = recv(comm_state->socket_fd, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            // Deserialize and populate result
            memcpy(result, buffer, sizeof(pgraft_message_t));
            return result;
        }
    }
    return NULL;  // No message received
}

/*
 * Free message allocated by communication layer
 */
void
pgraft_comm_free_message(pgraft_message_t *msg)
{
    if (msg)
    {
        if (msg->data)
        {
            pfree(msg->data);
        }
        pfree(msg);
    }
}
