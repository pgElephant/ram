/*
 * comm.c
 * Communication and message handling for pgraft extension
 *
 * This file handles network communication, message serialization,
 * and Raft protocol message processing for the pgraft extension.
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

/* Communication state */
typedef struct pgraft_comm_state
{
    int socket_fd;
    int port;
    char *address;
    bool is_listening;
    bool is_connected;
    pthread_t comm_thread;
    pthread_mutex_t comm_mutex;
    pthread_cond_t comm_cond;
    bool shutdown_requested;
} pgraft_comm_state_t;

static pgraft_comm_state_t *comm_state = NULL;

/* Message types */
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
    PGRaft_MSG_LOG_REPLICATION = 11,
    PGRaft_MSG_LOG_REPLICATION_RESPONSE = 12,
    PGRaft_MSG_SNAPSHOT_REQUEST = 13,
    PGRaft_MSG_SNAPSHOT_RESPONSE = 14,
    PGRaft_MSG_CONFIG_CHANGE = 15,
    PGRaft_MSG_CONFIG_CHANGE_RESPONSE = 16
} pgraft_msg_type_t;

/* Message header */
typedef struct pgraft_msg_header
{
    uint32_t magic;          /* Magic number for validation */
    uint32_t version;        /* Protocol version */
    uint32_t msg_type;       /* Message type */
    uint32_t msg_size;       /* Message payload size */
    uint64_t from_node;      /* Source node ID */
    uint64_t to_node;        /* Destination node ID */
    uint64_t term;           /* Current term */
    uint64_t index;          /* Log index */
    uint64_t timestamp;      /* Message timestamp */
    uint32_t checksum;       /* Message checksum */
} pgraft_msg_header_t;

#define PGRaft_MAGIC 0x50475241  /* "PGRA" */
#define PGRaft_VERSION 1

/* Message buffer */
typedef struct pgraft_msg_buffer
{
    pgraft_msg_header_t header;
    char *data;
    size_t data_size;
    size_t buffer_size;
} pgraft_msg_buffer_t;

/* Node connection info */
typedef struct pgraft_node_conn
{
    uint64_t node_id;
    char *address;
    int port;
    int socket_fd;
    bool is_connected;
    time_t last_heartbeat;
    uint64_t last_term;
    uint64_t last_index;
    pthread_mutex_t conn_mutex;
} pgraft_node_conn_t;

/* Connection pool */
typedef struct pgraft_conn_pool
{
    pgraft_node_conn_t *connections;
    int max_connections;
    int num_connections;
    pthread_mutex_t pool_mutex;
} pgraft_conn_pool_t;

static pgraft_conn_pool_t *conn_pool = NULL;

/* Forward declarations */
static int pgraft_create_socket(void);
static int pgraft_bind_socket(int sockfd, const char *address, int port);
static int pgraft_listen_socket(int sockfd);
static void *pgraft_comm_thread_main(void *arg);
static int pgraft_handle_incoming_message(int client_fd, const pgraft_msg_header_t *header);
static int pgraft_send_message_to_node(uint64_t node_id, const pgraft_msg_buffer_t *msg);
static int pgraft_serialize_message(const pgraft_msg_buffer_t *msg, char **buffer, size_t *size);
static int pgraft_deserialize_message(const char *buffer, size_t size, pgraft_msg_buffer_t *msg);
static uint32_t pgraft_calculate_checksum(const char *data, size_t size);
static int pgraft_validate_message(const pgraft_msg_header_t *header);
static int pgraft_add_node_connection(uint64_t node_id, const char *address, int port);
static int pgraft_remove_node_connection(uint64_t node_id);
static pgraft_node_conn_t *pgraft_find_node_connection(uint64_t node_id);
static int pgraft_connect_to_node(pgraft_node_conn_t *conn);
static void pgraft_close_node_connection(pgraft_node_conn_t *conn);

/* Initialize communication system */
int
pgraft_comm_init(const char *address, int port)
{
    int result;
    
    if (comm_state != NULL)
    {
        ereport(WARNING, (errmsg("pgraft: communication already initialized")));
        return 0;
    }
    
    comm_state = (pgraft_comm_state_t *) palloc0(sizeof(pgraft_comm_state_t));
    if (comm_state == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                       errmsg("pgraft: failed to allocate communication state")));
        return -1;
    }
    
    comm_state->address = pstrdup(address);
    comm_state->port = port;
    comm_state->is_listening = false;
    comm_state->is_connected = false;
    comm_state->shutdown_requested = false;
    
    if (pthread_mutex_init(&comm_state->comm_mutex, NULL) != 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to initialize communication mutex")));
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    if (pthread_cond_init(&comm_state->comm_cond, NULL) != 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to initialize communication condition")));
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    /* Initialize connection pool */
    conn_pool = (pgraft_conn_pool_t *) palloc0(sizeof(pgraft_conn_pool_t));
    if (conn_pool == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                       errmsg("pgraft: failed to allocate connection pool")));
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    conn_pool->max_connections = 100;
    conn_pool->num_connections = 0;
    conn_pool->connections = (pgraft_node_conn_t *) palloc0(
        sizeof(pgraft_node_conn_t) * conn_pool->max_connections);
    
    if (conn_pool->connections == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                       errmsg("pgraft: failed to allocate connection array")));
        pfree(conn_pool);
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    if (pthread_mutex_init(&conn_pool->pool_mutex, NULL) != 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to initialize pool mutex")));
        pfree(conn_pool->connections);
        pfree(conn_pool);
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    /* Create listening socket */
    comm_state->socket_fd = pgraft_create_socket();
    if (comm_state->socket_fd < 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to create socket")));
        pthread_mutex_destroy(&conn_pool->pool_mutex);
        pfree(conn_pool->connections);
        pfree(conn_pool);
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    /* Bind socket */
    result = pgraft_bind_socket(comm_state->socket_fd, address, port);
    if (result < 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to bind socket to %s:%d", address, port)));
        close(comm_state->socket_fd);
        pthread_mutex_destroy(&conn_pool->pool_mutex);
        pfree(conn_pool->connections);
        pfree(conn_pool);
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    /* Start listening */
    result = pgraft_listen_socket(comm_state->socket_fd);
    if (result < 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to listen on socket")));
        close(comm_state->socket_fd);
        pthread_mutex_destroy(&conn_pool->pool_mutex);
        pfree(conn_pool->connections);
        pfree(conn_pool);
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    comm_state->is_listening = true;
    
    /* Start communication thread */
    if (pthread_create(&comm_state->comm_thread, NULL, pgraft_comm_thread_main, comm_state) != 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to create communication thread")));
        close(comm_state->socket_fd);
        pthread_mutex_destroy(&conn_pool->pool_mutex);
        pfree(conn_pool->connections);
        pfree(conn_pool);
        pthread_cond_destroy(&comm_state->comm_cond);
        pthread_mutex_destroy(&comm_state->comm_mutex);
        pfree(comm_state->address);
        pfree(comm_state);
        comm_state = NULL;
        return -1;
    }
    
    if (pgraft_log_level >= 2)
        ereport(LOG, (errmsg("pgraft: communication initialized on %s:%d", address, port)));
    
    return 0;
}

/* Shutdown communication system */
int
pgraft_comm_shutdown(void)
{
    if (comm_state == NULL)
        return 0;
    
    /* Signal shutdown */
    pthread_mutex_lock(&comm_state->comm_mutex);
    comm_state->shutdown_requested = true;
    pthread_cond_signal(&comm_state->comm_cond);
    pthread_mutex_unlock(&comm_state->comm_mutex);
    
    /* Wait for thread to finish */
    if (pthread_join(comm_state->comm_thread, NULL) != 0)
    {
        ereport(WARNING, (errmsg("pgraft: failed to join communication thread")));
    }
    
    /* Close all connections */
    if (conn_pool != NULL)
    {
        int i;
        for (i = 0; i < conn_pool->num_connections; i++)
        {
            pgraft_close_node_connection(&conn_pool->connections[i]);
        }
        
        pthread_mutex_destroy(&conn_pool->pool_mutex);
        pfree(conn_pool->connections);
        pfree(conn_pool);
        conn_pool = NULL;
    }
    
    /* Close listening socket */
    if (comm_state->socket_fd >= 0)
    {
        close(comm_state->socket_fd);
    }
    
    /* Cleanup state */
    pthread_cond_destroy(&comm_state->comm_cond);
    pthread_mutex_destroy(&comm_state->comm_mutex);
    pfree(comm_state->address);
    pfree(comm_state);
    comm_state = NULL;
    
    if (pgraft_log_level >= 2)
        ereport(LOG, (errmsg("pgraft: communication shutdown complete")));
    
    return 0;
}

/* Send message to specific node */
int
pgraft_send_message(uint64_t to_node, int msg_type, 
                   const char *data, size_t data_size, uint64_t term, uint64_t index)
{
    pgraft_msg_buffer_t msg;
    int result;
    
    if (comm_state == NULL || !comm_state->is_listening)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: communication not initialized")));
        return -1;
    }
    
    /* Prepare message */
    memset(&msg, 0, sizeof(msg));
    msg.header.magic = PGRaft_MAGIC;
    msg.header.version = PGRaft_VERSION;
    msg.header.msg_type = msg_type;
    msg.header.msg_size = data_size;
    msg.header.from_node = pgraft_node_id;
    msg.header.to_node = to_node;
    msg.header.term = term;
    msg.header.index = index;
    msg.header.timestamp = time(NULL);
    
    if (data != NULL && data_size > 0)
    {
        msg.data = palloc(data_size);
        if (msg.data == NULL)
        {
            ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                           errmsg("pgraft: failed to allocate message data")));
            return -1;
        }
        memcpy(msg.data, data, data_size);
        msg.data_size = data_size;
    }
    
    msg.header.checksum = pgraft_calculate_checksum(msg.data, msg.data_size);
    
    /* Send message */
    result = pgraft_send_message_to_node(to_node, &msg);
    
    if (msg.data != NULL)
        pfree(msg.data);
    
    return result;
}

/* Broadcast message to all nodes */
int
pgraft_broadcast_message(int msg_type, const char *data, 
                        size_t data_size, uint64_t term, uint64_t index)
{
    int result = 0;
    int i;
    
    if (conn_pool == NULL)
        return -1;
    
    pthread_mutex_lock(&conn_pool->pool_mutex);
    
    for (i = 0; i < conn_pool->num_connections; i++)
    {
        if (conn_pool->connections[i].is_connected)
        {
            int node_result = pgraft_send_message(conn_pool->connections[i].node_id,
                                                msg_type, data, data_size, term, index);
            if (node_result < 0)
                result = -1;
        }
    }
    
    pthread_mutex_unlock(&conn_pool->pool_mutex);
    
    return result;
}

/* Add node to connection pool */
int
pgraft_add_node_comm(uint64_t node_id, const char *address, int port)
{
    return pgraft_add_node_connection(node_id, address, port);
}

/* Remove node from connection pool */
int
pgraft_remove_node_comm(uint64_t node_id)
{
    return pgraft_remove_node_connection(node_id);
}

/* Get connection status */
bool
pgraft_is_node_connected(uint64_t node_id)
{
    pgraft_node_conn_t *conn = pgraft_find_node_connection(node_id);
    return (conn != NULL && conn->is_connected);
}

/* Get number of connected nodes */
int
pgraft_get_connected_nodes_count(void)
{
    int count = 0;
    int i;
    
    if (conn_pool == NULL)
        return 0;
    
    pthread_mutex_lock(&conn_pool->pool_mutex);
    
    for (i = 0; i < conn_pool->num_connections; i++)
    {
        if (conn_pool->connections[i].is_connected)
            count++;
    }
    
    pthread_mutex_unlock(&conn_pool->pool_mutex);
    
    return count;
}

/* Implementation functions */

static int
pgraft_create_socket(void)
{
    int sockfd;
    int opt = 1;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        int saved_errno = errno;
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to create socket: %s", strerror(saved_errno))));
        return -1;
    }
    
    /* Set socket options */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        int saved_errno = errno;
        ereport(WARNING, (errmsg("pgraft: failed to set SO_REUSEADDR: %s", strerror(saved_errno))));
    }
    
    return sockfd;
}

static int
pgraft_bind_socket(int sockfd, const char *address, int port)
{
    struct sockaddr_in addr;
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address, &addr.sin_addr) <= 0)
    {
        /* Try to resolve hostname */
        struct hostent *he = gethostbyname(address);
        if (he == NULL)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                           errmsg("pgraft: invalid address: %s", address)));
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        int saved_errno = errno;
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to bind socket: %s", strerror(saved_errno))));
        return -1;
    }
    
    return 0;
}

static int
pgraft_listen_socket(int sockfd)
{
    if (listen(sockfd, 10) < 0)
    {
        int saved_errno = errno;
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to listen on socket: %s", strerror(saved_errno))));
        return -1;
    }
    
    return 0;
}

static void *
pgraft_comm_thread_main(void *arg)
{
    pgraft_comm_state_t *state = (pgraft_comm_state_t *) arg;
    fd_set read_fds;
    int max_fd;
    struct timeval timeout;
    int result;
    pgraft_msg_header_t header;
    ssize_t bytes_read;
    
    while (!state->shutdown_requested)
    {
        FD_ZERO(&read_fds);
        FD_SET(state->socket_fd, &read_fds);
        max_fd = state->socket_fd;
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        result = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (result < 0)
        {
            int saved_errno;
            if (errno == EINTR)
                continue;
            
            saved_errno = errno;
            ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                           errmsg("pgraft: select failed: %s", strerror(saved_errno))));
            break;
        }
        
        if (result == 0)
            continue; /* Timeout */
        
        if (FD_ISSET(state->socket_fd, &read_fds))
        {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd;
            
            client_fd = accept(state->socket_fd, (struct sockaddr *) &client_addr, &client_len);
            if (client_fd < 0)
            {
                int saved_errno;
                if (errno == EINTR)
                    continue;
                
                saved_errno = errno;
                ereport(WARNING, (errmsg("pgraft: accept failed: %s", strerror(saved_errno))));
                continue;
            }
            
            /* Handle client connection in a separate thread or process */
            /* For now, we'll handle it synchronously */
            
            bytes_read = recv(client_fd, &header, sizeof(header), MSG_WAITALL);
            if (bytes_read == sizeof(header))
            {
                if (pgraft_validate_message(&header) == 0)
                {
                    pgraft_handle_incoming_message(client_fd, &header);
                }
            }
            
            close(client_fd);
        }
    }
    
    return NULL;
}

static int
pgraft_handle_incoming_message(int client_fd, const pgraft_msg_header_t *header)
{
    char *data = NULL;
    ssize_t bytes_read;
    
    if (header->msg_size > 0)
    {
        data = palloc(header->msg_size);
        if (data == NULL)
        {
            ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                           errmsg("pgraft: failed to allocate message data")));
            return -1;
        }
        
        bytes_read = recv(client_fd, data, header->msg_size, MSG_WAITALL);
        if (bytes_read != header->msg_size)
        {
            ereport(WARNING, (errmsg("pgraft: incomplete message received")));
            pfree(data);
            return -1;
        }
    }
    
    /* Process message based on type */
    switch (header->msg_type)
    {
        case PGRaft_MSG_APPEND_ENTRIES:
            /* Handle append entries request */
            break;
        case PGRaft_MSG_REQUEST_VOTE:
            /* Handle vote request */
            break;
        case PGRaft_MSG_HEARTBEAT:
            /* Handle heartbeat */
            break;
        default:
            ereport(WARNING, (errmsg("pgraft: unknown message type: %d", header->msg_type)));
            break;
    }
    
    if (data != NULL)
        pfree(data);
    
    return 0;
}

static int
pgraft_send_message_to_node(uint64_t node_id, const pgraft_msg_buffer_t *msg)
{
    pgraft_node_conn_t *conn = pgraft_find_node_connection(node_id);
    char *buffer;
    size_t buffer_size;
    ssize_t bytes_sent;
    int result;
    
    if (conn == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: node %llu not found in connection pool", 
                              (unsigned long long) node_id)));
        return -1;
    }
    
    if (!conn->is_connected)
    {
        result = pgraft_connect_to_node(conn);
        if (result < 0)
            return -1;
    }
    
    result = pgraft_serialize_message(msg, &buffer, &buffer_size);
    if (result < 0)
        return -1;
    
    bytes_sent = send(conn->socket_fd, buffer, buffer_size, 0);
    if (bytes_sent != buffer_size)
    {
        ereport(WARNING, (errmsg("pgraft: failed to send complete message to node %llu", 
                                 (unsigned long long) node_id)));
        pfree(buffer);
        return -1;
    }
    
    pfree(buffer);
    return 0;
}

static int
pgraft_serialize_message(const pgraft_msg_buffer_t *msg, char **buffer, size_t *size)
{
    size_t total_size = sizeof(pgraft_msg_header_t) + msg->data_size;
    
    *buffer = palloc(total_size);
    if (*buffer == NULL)
    {
        ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                       errmsg("pgraft: failed to allocate serialization buffer")));
        return -1;
    }
    
    memcpy(*buffer, &msg->header, sizeof(pgraft_msg_header_t));
    if (msg->data != NULL && msg->data_size > 0)
    {
        memcpy(*buffer + sizeof(pgraft_msg_header_t), msg->data, msg->data_size);
    }
    
    *size = total_size;
    return 0;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static int
pgraft_deserialize_message(const char *buffer, size_t size, pgraft_msg_buffer_t *msg)
{
    if (size < sizeof(pgraft_msg_header_t))
    {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                       errmsg("pgraft: message too small")));
        return -1;
    }
    
    memcpy(&msg->header, buffer, sizeof(pgraft_msg_header_t));
    
    if (msg->header.msg_size > 0)
    {
        msg->data = palloc(msg->header.msg_size);
        if (msg->data == NULL)
        {
            ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
                           errmsg("pgraft: failed to allocate message data")));
            return -1;
        }
        
        if (size < sizeof(pgraft_msg_header_t) + msg->header.msg_size)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                           errmsg("pgraft: message truncated")));
            pfree(msg->data);
            return -1;
        }
        
        memcpy(msg->data, buffer + sizeof(pgraft_msg_header_t), msg->header.msg_size);
        msg->data_size = msg->header.msg_size;
    }
    
    return 0;
}
#pragma GCC diagnostic pop

static uint32_t
pgraft_calculate_checksum(const char *data, size_t size)
{
    uint32_t checksum = 0;
    size_t i;
    
    for (i = 0; i < size; i++)
    {
        checksum = (checksum << 1) ^ data[i];
    }
    
    return checksum;
}

static int
pgraft_validate_message(const pgraft_msg_header_t *header)
{
    if (header->magic != PGRaft_MAGIC)
    {
        ereport(WARNING, (errmsg("pgraft: invalid magic number: 0x%x", header->magic)));
        return -1;
    }
    
    if (header->version != PGRaft_VERSION)
    {
        ereport(WARNING, (errmsg("pgraft: unsupported version: %d", header->version)));
        return -1;
    }
    
    if (header->msg_type < PGRaft_MSG_APPEND_ENTRIES || 
        header->msg_type > PGRaft_MSG_CONFIG_CHANGE_RESPONSE)
    {
        ereport(WARNING, (errmsg("pgraft: invalid message type: %d", header->msg_type)));
        return -1;
    }
    
    return 0;
}

static int
pgraft_add_node_connection(uint64_t node_id, const char *address, int port)
{
    pgraft_node_conn_t *conn;
    
    if (conn_pool == NULL || conn_pool->num_connections >= conn_pool->max_connections)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: connection pool full")));
        return -1;
    }
    
    pthread_mutex_lock(&conn_pool->pool_mutex);
    
    conn = &conn_pool->connections[conn_pool->num_connections];
    conn->node_id = node_id;
    conn->address = pstrdup(address);
    conn->port = port;
    conn->socket_fd = -1;
    conn->is_connected = false;
    conn->last_heartbeat = 0;
    conn->last_term = 0;
    conn->last_index = 0;
    
    if (pthread_mutex_init(&conn->conn_mutex, NULL) != 0)
    {
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to initialize connection mutex")));
        pfree(conn->address);
        pthread_mutex_unlock(&conn_pool->pool_mutex);
        return -1;
    }
    
    conn_pool->num_connections++;
    
    pthread_mutex_unlock(&conn_pool->pool_mutex);
    
    if (pgraft_log_level >= 2)
        ereport(LOG, (errmsg("pgraft: added node %llu (%s:%d) to connection pool", 
                             (unsigned long long) node_id, address, port)));
    
    return 0;
}

static int
pgraft_remove_node_connection(uint64_t node_id)
{
    int i;
    bool found = false;
    
    if (conn_pool == NULL)
        return -1;
    
    pthread_mutex_lock(&conn_pool->pool_mutex);
    
    for (i = 0; i < conn_pool->num_connections; i++)
    {
        if (conn_pool->connections[i].node_id == node_id)
        {
            found = true;
            pgraft_close_node_connection(&conn_pool->connections[i]);
            
            /* Move last connection to this position */
            if (i < conn_pool->num_connections - 1)
            {
                memcpy(&conn_pool->connections[i], 
                       &conn_pool->connections[conn_pool->num_connections - 1],
                       sizeof(pgraft_node_conn_t));
            }
            
            conn_pool->num_connections--;
            break;
        }
    }
    
    pthread_mutex_unlock(&conn_pool->pool_mutex);
    
    if (!found)
    {
        ereport(WARNING, (errmsg("pgraft: node %llu not found in connection pool", 
                                 (unsigned long long) node_id)));
        return -1;
    }
    
    if (pgraft_log_level >= 2)
        ereport(LOG, (errmsg("pgraft: removed node %llu from connection pool", 
                             (unsigned long long) node_id)));
    
    return 0;
}

static pgraft_node_conn_t *
pgraft_find_node_connection(uint64_t node_id)
{
    int i;
    pgraft_node_conn_t *conn = NULL;
    
    if (conn_pool == NULL)
        return NULL;
    
    pthread_mutex_lock(&conn_pool->pool_mutex);
    
    for (i = 0; i < conn_pool->num_connections; i++)
    {
        if (conn_pool->connections[i].node_id == node_id)
        {
            conn = &conn_pool->connections[i];
            break;
        }
    }
    
    pthread_mutex_unlock(&conn_pool->pool_mutex);
    
    return conn;
}

static int
pgraft_connect_to_node(pgraft_node_conn_t *conn)
{
    struct sockaddr_in addr;
    int result;
    
    if (conn->is_connected)
        return 0;
    
    conn->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->socket_fd < 0)
    {
        int saved_errno = errno;
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR),
                       errmsg("pgraft: failed to create socket for node %llu: %s", 
                              (unsigned long long) conn->node_id, strerror(saved_errno))));
        return -1;
    }
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(conn->port);
    
    if (inet_pton(AF_INET, conn->address, &addr.sin_addr) <= 0)
    {
        /* Try to resolve hostname */
        struct hostent *he = gethostbyname(conn->address);
        if (he == NULL)
        {
            ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                           errmsg("pgraft: invalid address for node %llu: %s", 
                                  (unsigned long long) conn->node_id, conn->address)));
            close(conn->socket_fd);
            conn->socket_fd = -1;
            return -1;
        }
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    }
    
    result = connect(conn->socket_fd, (struct sockaddr *) &addr, sizeof(addr));
    if (result < 0)
    {
        int saved_errno = errno;
        ereport(WARNING, (errmsg("pgraft: failed to connect to node %llu (%s:%d): %s", 
                                 (unsigned long long) conn->node_id, conn->address, conn->port, 
                                 strerror(saved_errno))));
        close(conn->socket_fd);
        conn->socket_fd = -1;
        return -1;
    }
    
    conn->is_connected = true;
    conn->last_heartbeat = time(NULL);
    
    if (pgraft_log_level >= 2)
        ereport(LOG, (errmsg("pgraft: connected to node %llu (%s:%d)", 
                             (unsigned long long) conn->node_id, conn->address, conn->port)));
    
    return 0;
}

static void
pgraft_close_node_connection(pgraft_node_conn_t *conn)
{
    if (conn->socket_fd >= 0)
    {
        close(conn->socket_fd);
        conn->socket_fd = -1;
    }
    
    conn->is_connected = false;
    
    if (conn->address != NULL)
    {
        pfree(conn->address);
        conn->address = NULL;
    }
    
    pthread_mutex_destroy(&conn->conn_mutex);
}
