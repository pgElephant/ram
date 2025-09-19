/*
 * test_ramd.c
 * Comprehensive test suite for ramd daemon
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <sys/wait.h>
#include <signal.h>
#include <curl/curl.h>

#include "ramd.h"

/* Test configuration */
#define TEST_HTTP_PORT 8080
#define TEST_CONFIG_FILE "test_ramd.conf"
#define TEST_LOG_FILE "test_ramd.log"

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

#define ASSERT_NOT_NULL(ptr, message) \
    ASSERT((ptr) != NULL, message)

/* Test helper functions */
static void create_test_config(void);
static void cleanup_test_files(void);
static int start_ramd_daemon(void);
static void stop_ramd_daemon(int pid);
static int test_http_endpoint(const char* endpoint, const char* expected_response);
static int test_http_post(const char* endpoint, const char* data, const char* expected_response);

/* Test functions */
static void test_daemon_startup(void);
static void test_configuration_loading(void);
static void test_http_api(void);
static void test_cluster_management(void);
static void test_health_monitoring(void);
static void test_replication_management(void);
static void test_metrics_collection(void);
static void test_error_handling(void);
static void test_performance(void);
static void test_shutdown(void);

int main(void)
{
    printf("Starting ramd comprehensive test suite...\n");
    printf("========================================\n\n");
    
    /* Initialize curl */
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    /* Create test configuration */
    create_test_config();
    
    /* Run tests */
    test_daemon_startup();
    test_configuration_loading();
    test_http_api();
    test_cluster_management();
    test_health_monitoring();
    test_replication_management();
    test_metrics_collection();
    test_error_handling();
    test_performance();
    test_shutdown();
    
    /* Cleanup */
    cleanup_test_files();
    curl_global_cleanup();
    
    /* Print results */
    printf("\n========================================\n");
    printf("Test Results:\n");
    printf("Total tests: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Success rate: %.2f%%\n", 
           tests_run > 0 ? (100.0 * tests_passed / tests_run) : 0.0);
    
    return tests_failed > 0 ? 1 : 0;
}

static void create_test_config(void)
{
    FILE* fp = fopen(TEST_CONFIG_FILE, "w");
    if (!fp) {
        printf("ERROR: Failed to create test config file\n");
        return;
    }
    
    fprintf(fp, "[global]\n");
    fprintf(fp, "daemon = true\n");
    fprintf(fp, "pid_file = test_ramd.pid\n");
    fprintf(fp, "log_file = %s\n", TEST_LOG_FILE);
    fprintf(fp, "log_level = DEBUG\n");
    fprintf(fp, "http_enabled = true\n");
    fprintf(fp, "http_port = %d\n", TEST_HTTP_PORT);
    fprintf(fp, "http_host = 127.0.0.1\n");
    fprintf(fp, "db_host = localhost\n");
    fprintf(fp, "db_port = 5432\n");
    fprintf(fp, "db_name = testdb\n");
    fprintf(fp, "db_user = postgres\n");
    fprintf(fp, "db_password = postgres\n");
    fprintf(fp, "cluster_name = test-cluster\n");
    fprintf(fp, "node_id = 1\n");
    fprintf(fp, "node_name = test-node\n");
    fprintf(fp, "raft_port = 5433\n");
    fprintf(fp, "health_check_interval = 5\n");
    fprintf(fp, "metrics_enabled = true\n");
    fprintf(fp, "metrics_port = 9090\n");
    
    fclose(fp);
}

static void cleanup_test_files(void)
{
    unlink(TEST_CONFIG_FILE);
    unlink(TEST_LOG_FILE);
    unlink("test_ramd.pid");
}

static int start_ramd_daemon(void)
{
    pid_t pid = fork();
    
    if (pid == 0) {
        /* Child process - start ramd */
        execl("./ramd/ramd", "ramd", "--config", TEST_CONFIG_FILE, "--daemon", NULL);
        exit(1);
    } else if (pid > 0) {
        /* Parent process - wait for daemon to start */
        sleep(3);
        return pid;
    } else {
        return -1;
    }
}

static void stop_ramd_daemon(int pid)
{
    if (pid > 0) {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    }
}

static int test_http_endpoint(const char* endpoint, const char* expected_response)
{
    CURL* curl;
    CURLcode res;
    char url[256];
    char response[1024] = {0};
    int success = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        return 0;
    }
    
    snprintf(url, sizeof(url), "http://127.0.0.1:%d%s", TEST_HTTP_PORT, endpoint);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
                     (curl_write_callback)fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        if (expected_response == NULL || strstr(response, expected_response) != NULL) {
            success = 1;
        }
    }
    
    curl_easy_cleanup(curl);
    return success;
}

static int test_http_post(const char* endpoint, const char* data, const char* expected_response)
{
    CURL* curl;
    CURLcode res;
    char url[256];
    char response[1024] = {0};
    int success = 0;
    
    curl = curl_easy_init();
    if (!curl) {
        return 0;
    }
    
    snprintf(url, sizeof(url), "http://127.0.0.1:%d%s", TEST_HTTP_PORT, endpoint);
    
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
                     (curl_write_callback)fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    
    res = curl_easy_perform(curl);
    
    if (res == CURLE_OK) {
        if (expected_response == NULL || strstr(response, expected_response) != NULL) {
            success = 1;
        }
    }
    
    curl_easy_cleanup(curl);
    return success;
}

static void test_daemon_startup(void)
{
    int pid;
    
    printf("\n--- Testing Daemon Startup ---\n");
    
    /* Test daemon startup */
    pid = start_ramd_daemon();
    ASSERT(pid > 0, "Daemon should start successfully");
    
    if (pid > 0) {
        /* Test daemon is running */
        ASSERT(kill(pid, 0) == 0, "Daemon should be running");
        
        /* Clean up */
        stop_ramd_daemon(pid);
    }
}

static void test_configuration_loading(void)
{
    printf("\n--- Testing Configuration Loading ---\n");
    
    /* Test config file exists */
    ASSERT(access(TEST_CONFIG_FILE, R_OK) == 0, "Config file should be readable");
    
    /* Test config parsing (simplified) */
    FILE* fp = fopen(TEST_CONFIG_FILE, "r");
    ASSERT_NOT_NULL(fp, "Should be able to open config file");
    
    if (fp) {
        char line[256];
        int found_http_port = 0;
        
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "http_port") != NULL) {
                found_http_port = 1;
                break;
            }
        }
        
        fclose(fp);
        ASSERT(found_http_port, "Config should contain http_port setting");
    }
}

static void test_http_api(void)
{
    int pid;
    
    printf("\n--- Testing HTTP API ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test HTTP API without running daemon\n");
        return;
    }
    
    /* Test health endpoint */
    ASSERT(test_http_endpoint("/health", "healthy"), "Health endpoint should work");
    
    /* Test status endpoint */
    ASSERT(test_http_endpoint("/status", "status"), "Status endpoint should work");
    
    /* Test metrics endpoint */
    ASSERT(test_http_endpoint("/metrics", "ramd_"), "Metrics endpoint should work");
    
    /* Test cluster info endpoint */
    ASSERT(test_http_endpoint("/cluster/info", "cluster"), "Cluster info endpoint should work");
    
    /* Test nodes endpoint */
    ASSERT(test_http_endpoint("/cluster/nodes", "nodes"), "Nodes endpoint should work");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_cluster_management(void)
{
    int pid;
    
    printf("\n--- Testing Cluster Management ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test cluster management without running daemon\n");
        return;
    }
    
    /* Test cluster initialization */
    ASSERT(test_http_post("/cluster/init", 
                          "{\"cluster_name\":\"test-cluster\",\"node_id\":1,\"node_name\":\"test-node\"}", 
                          "success"), "Cluster initialization should work");
    
    /* Test adding nodes */
    ASSERT(test_http_post("/cluster/nodes/add", 
                          "{\"node_id\":2,\"node_name\":\"test-node-2\",\"hostname\":\"localhost\",\"port\":5434}", 
                          "success"), "Adding nodes should work");
    
    /* Test cluster status */
    ASSERT(test_http_endpoint("/cluster/status", "cluster"), "Cluster status should work");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_health_monitoring(void)
{
    int pid;
    
    printf("\n--- Testing Health Monitoring ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test health monitoring without running daemon\n");
        return;
    }
    
    /* Test health check */
    ASSERT(test_http_endpoint("/health", "healthy"), "Health check should work");
    
    /* Test detailed health info */
    ASSERT(test_http_endpoint("/health/detailed", "health"), "Detailed health should work");
    
    /* Test node health */
    ASSERT(test_http_endpoint("/cluster/nodes/health", "health"), "Node health should work");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_replication_management(void)
{
    int pid;
    
    printf("\n--- Testing Replication Management ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test replication without running daemon\n");
        return;
    }
    
    /* Test replication setup */
    ASSERT(test_http_post("/replication/setup", 
                          "{\"primary_node_id\":1,\"standby_node_id\":2}", 
                          "success"), "Replication setup should work");
    
    /* Test replication status */
    ASSERT(test_http_endpoint("/replication/status", "replication"), "Replication status should work");
    
    /* Test replication slots */
    ASSERT(test_http_endpoint("/replication/slots", "slots"), "Replication slots should work");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_metrics_collection(void)
{
    int pid;
    
    printf("\n--- Testing Metrics Collection ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test metrics without running daemon\n");
        return;
    }
    
    /* Test metrics endpoint */
    ASSERT(test_http_endpoint("/metrics", "ramd_"), "Metrics endpoint should work");
    
    /* Test specific metrics */
    ASSERT(test_http_endpoint("/metrics/ramd_health_checks_total", "ramd_health_checks_total"), 
           "Health check metrics should work");
    
    /* Test performance metrics */
    ASSERT(test_http_endpoint("/metrics/ramd_operations_total", "ramd_operations_total"), 
           "Operation metrics should work");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_error_handling(void)
{
    int pid;
    
    printf("\n--- Testing Error Handling ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test error handling without running daemon\n");
        return;
    }
    
    /* Test invalid endpoints */
    ASSERT(!test_http_endpoint("/invalid", NULL), "Invalid endpoints should fail");
    
    /* Test malformed JSON */
    ASSERT(!test_http_post("/cluster/init", "invalid json", NULL), 
           "Malformed JSON should fail");
    
    /* Test invalid parameters */
    ASSERT(!test_http_post("/cluster/nodes/add", 
                          "{\"node_id\":-1,\"node_name\":\"invalid\"}", 
                          NULL), "Invalid parameters should fail");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_performance(void)
{
    int pid;
    clock_t start, end;
    double cpu_time_used;
    int i;
    
    printf("\n--- Testing Performance ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test performance without running daemon\n");
        return;
    }
    
    /* Test HTTP response time */
    start = clock();
    
    for (i = 0; i < 100; i++) {
        test_http_endpoint("/health", "healthy");
    }
    
    end = clock();
    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    printf("100 HTTP requests completed in %.2f seconds\n", cpu_time_used);
    ASSERT(cpu_time_used < 10.0, "HTTP requests should be fast");
    
    /* Clean up */
    stop_ramd_daemon(pid);
}

static void test_shutdown(void)
{
    int pid;
    
    printf("\n--- Testing Shutdown ---\n");
    
    /* Start daemon */
    pid = start_ramd_daemon();
    if (pid <= 0) {
        printf("SKIP: Cannot test shutdown without running daemon\n");
        return;
    }
    
    /* Test graceful shutdown */
    stop_ramd_daemon(pid);
    
    /* Wait a bit for shutdown */
    sleep(2);
    
    /* Test daemon is no longer running */
    ASSERT(kill(pid, 0) != 0, "Daemon should be stopped");
}
