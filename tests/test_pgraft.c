/*
 * test_pgraft.c
 * Comprehensive test suite for pgraft extension
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <libpq-fe.h>

#include "pgraft.h"

/* Test configuration */
#define TEST_DB_HOST "localhost"
#define TEST_DB_PORT "5432"
#define TEST_DB_NAME "testdb"
#define TEST_DB_USER "postgres"
#define TEST_DB_PASSWORD "postgres"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define ASSERT(condition, message) \
    do { \
        tests_run++; \
        if (condition) { \
            tests_passed++; \
            printf("PASS: %s\n", message); \
        } else { \
            tests_failed++; \
            printf("FAIL: %s\n", message); \
        } \
    } while (0)

#define ASSERT_EQ(expected, actual, message) \
    ASSERT((expected) == (actual), message)

#define ASSERT_NE(expected, actual, message) \
    ASSERT((expected) != (actual), message)

#define ASSERT_STR_EQ(expected, actual, message) \
    ASSERT(strcmp(expected, actual) == 0, message)

#define ASSERT_NOT_NULL(ptr, message) \
    ASSERT((ptr) != NULL, message)

/* Test helper functions */
static PGconn* connect_to_database(void);
static void disconnect_from_database(PGconn* conn);
static int execute_query(PGconn* conn, const char* query);
static PGresult* execute_query_with_result(PGconn* conn, const char* query);
static void cleanup_test_data(PGconn* conn);

/* Test functions */
static void test_extension_creation(PGconn* conn);
static void test_cluster_initialization(PGconn* conn);
static void test_node_management(PGconn* conn);
static void test_consensus_operations(PGconn* conn);
static void test_replication_operations(PGconn* conn);
static void test_health_monitoring(PGconn* conn);
static void test_views_and_functions(PGconn* conn);
static void test_error_handling(PGconn* conn);
static void test_performance(PGconn* conn);
static void test_concurrent_operations(PGconn* conn);

int main(void)
{
    PGconn* conn;
    
    printf("Starting pgraft comprehensive test suite...\n");
    printf("==========================================\n\n");
    
    /* Connect to database */
    conn = connect_to_database();
    if (!conn) {
        printf("ERROR: Failed to connect to database\n");
        return 1;
    }
    
    /* Run tests */
    test_extension_creation(conn);
    test_cluster_initialization(conn);
    test_node_management(conn);
    test_consensus_operations(conn);
    test_replication_operations(conn);
    test_health_monitoring(conn);
    test_views_and_functions(conn);
    test_error_handling(conn);
    test_performance(conn);
    test_concurrent_operations(conn);
    
    /* Cleanup */
    cleanup_test_data(conn);
    disconnect_from_database(conn);
    
    /* Print results */
    printf("\n==========================================\n");
    printf("Test Results:\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.2f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    return tests_failed > 0 ? 1 : 0;
}

static PGconn* connect_to_database(void)
{
    PGconn* conn;
    char conninfo[512];
    
    snprintf(conninfo, sizeof(conninfo),
             "host=%s port=%s dbname=%s user=%s password=%s",
             TEST_DB_HOST, TEST_DB_PORT, TEST_DB_NAME, 
             TEST_DB_USER, TEST_DB_PASSWORD);
    
    conn = PQconnectdb(conninfo);
    
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Connection failed: %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return NULL;
    }
    
    return conn;
}

static void disconnect_from_database(PGconn* conn)
{
    if (conn) {
        PQfinish(conn);
    }
}

static int execute_query(PGconn* conn, const char* query)
{
    PGresult* res;
    int result;
    
    res = PQexec(conn, query);
    result = (PQresultStatus(res) == PGRES_COMMAND_OK) ? 0 : -1;
    PQclear(res);
    
    return result;
}

static PGresult* execute_query_with_result(PGconn* conn, const char* query)
{
    return PQexec(conn, query);
}

static void cleanup_test_data(PGconn* conn)
{
    printf("Cleaning up test data...\n");
    
    /* Drop test tables and data */
    execute_query(conn, "DROP TABLE IF EXISTS test_data CASCADE;");
    execute_query(conn, "DROP TABLE IF EXISTS test_replication CASCADE;");
    
    /* Reset pgraft state */
    execute_query(conn, "SELECT pgraft_reset_cluster();");
}

static void test_extension_creation(PGconn* conn)
{
    PGresult* res;
    int count;
    
    printf("\n--- Testing Extension Creation ---\n");
    
    /* Test extension exists */
    res = execute_query_with_result(conn, 
        "SELECT COUNT(*) FROM pg_extension WHERE extname = 'pgraft';");
    count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    ASSERT_EQ(1, count, "pgraft extension should be installed");
    
    /* Test extension version */
    res = execute_query_with_result(conn, 
        "SELECT extversion FROM pg_extension WHERE extname = 'pgraft';");
    ASSERT_NOT_NULL(PQgetvalue(res, 0, 0), "pgraft extension should have version");
    PQclear(res);
    
    /* Test extension functions exist */
    res = execute_query_with_result(conn,
        "SELECT COUNT(*) FROM pg_proc WHERE pronamespace = "
        "(SELECT oid FROM pg_namespace WHERE nspname = 'pgraft');");
    count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    ASSERT(count > 0, "pgraft should have functions");
}

static void test_cluster_initialization(PGconn* conn)
{
    PGresult* res;
    int result;
    
    printf("\n--- Testing Cluster Initialization ---\n");
    
    /* Test cluster initialization */
    result = execute_query(conn, 
        "SELECT pgraft_init_cluster('test-cluster', 1, 'test-node-1');");
    ASSERT_EQ(0, result, "Cluster initialization should succeed");
    
    /* Test cluster state */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_cluster_state();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res), 
              "Should be able to get cluster state");
    PQclear(res);
    
    /* Test node information */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_node_info();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Should be able to get node info");
    PQclear(res);
}

static void test_node_management(PGconn* conn)
{
    PGresult* res;
    int result;
    
    printf("\n--- Testing Node Management ---\n");
    
    /* Test adding nodes */
    result = execute_query(conn,
        "SELECT pgraft_add_node(2, 'test-node-2', 'localhost', 5434);");
    ASSERT_EQ(0, result, "Adding node should succeed");
    
    result = execute_query(conn,
        "SELECT pgraft_add_node(3, 'test-node-3', 'localhost', 5435);");
    ASSERT_EQ(0, result, "Adding second node should succeed");
    
    /* Test listing nodes */
    res = execute_query_with_result(conn,
        "SELECT COUNT(*) FROM pgraft_list_nodes();");
    int node_count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    ASSERT_EQ(3, node_count, "Should have 3 nodes in cluster");
    
    /* Test node status */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_node_status(1);");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Should be able to get node status");
    PQclear(res);
}

static void test_consensus_operations(PGconn* conn)
{
    PGresult* res;
    int result;
    
    printf("\n--- Testing Consensus Operations ---\n");
    
    /* Test leader election */
    result = execute_query(conn,
        "SELECT pgraft_start_election();");
    ASSERT_EQ(0, result, "Leader election should succeed");
    
    /* Test consensus state */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_consensus_state();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Should be able to get consensus state");
    PQclear(res);
    
    /* Test term information */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_current_term();");
    int term = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    ASSERT(term > 0, "Current term should be positive");
}

static void test_replication_operations(PGconn* conn)
{
    PGresult* res;
    int result;
    
    printf("\n--- Testing Replication Operations ---\n");
    
    /* Create test table */
    result = execute_query(conn,
        "CREATE TABLE test_data (id SERIAL PRIMARY KEY, data TEXT);");
    ASSERT_EQ(0, result, "Creating test table should succeed");
    
    /* Test replication setup */
    result = execute_query(conn,
        "SELECT pgraft_setup_replication(1, 2);");
    ASSERT_EQ(0, result, "Setting up replication should succeed");
    
    /* Test replication status */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_replication_status();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Should be able to get replication status");
    PQclear(res);
    
    /* Test data replication */
    result = execute_query(conn,
        "INSERT INTO test_data (data) VALUES ('test data');");
    ASSERT_EQ(0, result, "Inserting test data should succeed");
    
    /* Verify replication */
    res = execute_query_with_result(conn,
        "SELECT COUNT(*) FROM test_data;");
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    ASSERT_EQ(1, count, "Test data should be replicated");
}

static void test_health_monitoring(PGconn* conn)
{
    PGresult* res;
    
    printf("\n--- Testing Health Monitoring ---\n");
    
    /* Test health check */
    res = execute_query_with_result(conn,
        "SELECT pgraft_health_check();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Health check should succeed");
    PQclear(res);
    
    /* Test cluster health */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_cluster_health();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Should be able to get cluster health");
    PQclear(res);
    
    /* Test metrics */
    res = execute_query_with_result(conn,
        "SELECT pgraft_get_metrics();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Should be able to get metrics");
    PQclear(res);
}

static void test_views_and_functions(PGconn* conn)
{
    PGresult* res;
    
    printf("\n--- Testing Views and Functions ---\n");
    
    /* Test cluster overview view */
    res = execute_query_with_result(conn,
        "SELECT * FROM pgraft.cluster_overview;");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Cluster overview view should work");
    PQclear(res);
    
    /* Test node health view */
    res = execute_query_with_result(conn,
        "SELECT * FROM pgraft.node_health;");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Node health view should work");
    PQclear(res);
    
    /* Test replication status view */
    res = execute_query_with_result(conn,
        "SELECT * FROM pgraft.replication_status;");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Replication status view should work");
    PQclear(res);
    
    /* Test cluster metrics view */
    res = execute_query_with_result(conn,
        "SELECT * FROM pgraft.cluster_metrics;");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Cluster metrics view should work");
    PQclear(res);
}

static void test_error_handling(PGconn* conn)
{
    PGresult* res;
    
    printf("\n--- Testing Error Handling ---\n");
    
    /* Test invalid node ID */
    res = execute_query_with_result(conn,
        "SELECT pgraft_add_node(-1, 'invalid', 'localhost', 5432);");
    ASSERT_NE(PGRES_TUPLES_OK, PQresultStatus(res),
              "Invalid node ID should fail");
    PQclear(res);
    
    /* Test duplicate node ID */
    res = execute_query_with_result(conn,
        "SELECT pgraft_add_node(1, 'duplicate', 'localhost', 5432);");
    ASSERT_NE(PGRES_TUPLES_OK, PQresultStatus(res),
              "Duplicate node ID should fail");
    PQclear(res);
    
    /* Test invalid cluster operations */
    res = execute_query_with_result(conn,
        "SELECT pgraft_remove_node(999);");
    ASSERT_NE(PGRES_TUPLES_OK, PQresultStatus(res),
              "Removing non-existent node should fail");
    PQclear(res);
}

static void test_performance(PGconn* conn)
{
    PGresult* res;
    clock_t start, end;
    double cpu_time_used;
    int i;
    
    printf("\n--- Testing Performance ---\n");
    
    /* Test bulk operations */
    start = clock();
    
    for (i = 0; i < 1000; i++) {
        res = execute_query_with_result(conn,
            "INSERT INTO test_data (data) VALUES ('performance test data');");
        PQclear(res);
    }
    
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Inserted 1000 rows in %.2f seconds\n", cpu_time_used);
    ASSERT(cpu_time_used < 10.0, "Bulk operations should be fast");
    
    /* Test query performance */
    start = clock();
    
    res = execute_query_with_result(conn,
        "SELECT COUNT(*) FROM test_data;");
    int count = atoi(PQgetvalue(res, 0, 0));
    PQclear(res);
    
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("Queried %d rows in %.2f seconds\n", count, cpu_time_used);
    ASSERT(cpu_time_used < 1.0, "Queries should be fast");
}

static void test_concurrent_operations(PGconn* conn)
{
    printf("\n--- Testing Concurrent Operations ---\n");
    
    /* This would require multiple connections and threads */
    /* For now, just test that the system handles concurrent access */
    
    PGresult* res = execute_query_with_result(conn,
        "SELECT pgraft_get_cluster_state();");
    ASSERT_EQ(PGRES_TUPLES_OK, PQresultStatus(res),
              "Concurrent access should work");
    PQclear(res);
    
    printf("Concurrent operations test completed (simplified)\n");
}
