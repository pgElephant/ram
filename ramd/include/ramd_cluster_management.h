/*-------------------------------------------------------------------------
 *
 * ramd_cluster_management.h
 *		PostgreSQL RAM Daemon - Cluster Management API Module
 *
 * This module provides comprehensive cluster management API endpoints
 * including switchover, configuration management, backup operations,
 * parameter validation, and cluster health monitoring.
 *
 * Copyright (c) 2024-2025, pgElephant, Inc.
 *
 *-------------------------------------------------------------------------
 */

#ifndef RAMD_CLUSTER_MANAGEMENT_H
#define RAMD_CLUSTER_MANAGEMENT_H

#include "ramd_common.h"
#include "ramd_http_api.h"

/* Function prototypes */

/*
 * Handle switchover request
 */
bool ramd_api_handle_switchover(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle configuration get request
 */
bool ramd_api_handle_config_get(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle configuration set request
 */
bool ramd_api_handle_config_set(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle backup start request
 */
bool ramd_api_handle_backup_start(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster status request
 */
bool ramd_api_handle_cluster_status(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle parameter validation request
 */
bool ramd_api_handle_parameter_validate(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle backup list request
 */
bool ramd_api_handle_backup_list(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle backup restore request
 */
bool ramd_api_handle_backup_restore(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle backup jobs request
 */
bool ramd_api_handle_backup_jobs(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle parameter list request
 */
bool ramd_api_handle_parameter_list(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle parameter info request
 */
bool ramd_api_handle_parameter_info(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster health request
 */
bool ramd_api_handle_cluster_health(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster metrics request
 */
bool ramd_api_handle_cluster_metrics(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster logs request
 */
bool ramd_api_handle_cluster_logs(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster restart request
 */
bool ramd_api_handle_cluster_restart(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster reload request
 */
bool ramd_api_handle_cluster_reload(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster failover request
 */
bool ramd_api_handle_cluster_failover(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster promote request
 */
bool ramd_api_handle_cluster_promote(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster demote request
 */
bool ramd_api_handle_cluster_demote(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster add node request
 */
bool ramd_api_handle_cluster_add_node(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster remove node request
 */
bool ramd_api_handle_cluster_remove_node(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster update node request
 */
bool ramd_api_handle_cluster_update_node(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster sync request
 */
bool ramd_api_handle_cluster_sync(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster backup request
 */
bool ramd_api_handle_cluster_backup(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster restore request
 */
bool ramd_api_handle_cluster_restore(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster maintenance request
 */
bool ramd_api_handle_cluster_maintenance(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster upgrade request
 */
bool ramd_api_handle_cluster_upgrade(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster downgrade request
 */
bool ramd_api_handle_cluster_downgrade(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster migrate request
 */
bool ramd_api_handle_cluster_migrate(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster clone request
 */
bool ramd_api_handle_cluster_clone(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster snapshot request
 */
bool ramd_api_handle_cluster_snapshot(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster rollback request
 */
bool ramd_api_handle_cluster_rollback(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster audit request
 */
bool ramd_api_handle_cluster_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster compliance request
 */
bool ramd_api_handle_cluster_compliance(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster security request
 */
bool ramd_api_handle_cluster_security(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster performance request
 */
bool ramd_api_handle_cluster_performance(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster capacity request
 */
bool ramd_api_handle_cluster_capacity(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster utilization request
 */
bool ramd_api_handle_cluster_utilization(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster efficiency request
 */
bool ramd_api_handle_cluster_efficiency(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster optimization request
 */
bool ramd_api_handle_cluster_optimization(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster tuning request
 */
bool ramd_api_handle_cluster_tuning(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster scaling request
 */
bool ramd_api_handle_cluster_scaling(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster load balancing request
 */
bool ramd_api_handle_cluster_load_balancing(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster failover testing request
 */
bool ramd_api_handle_cluster_failover_testing(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster disaster recovery request
 */
bool ramd_api_handle_cluster_disaster_recovery(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster business continuity request
 */
bool ramd_api_handle_cluster_business_continuity(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster high availability request
 */
bool ramd_api_handle_cluster_high_availability(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster fault tolerance request
 */
bool ramd_api_handle_cluster_fault_tolerance(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster resilience request
 */
bool ramd_api_handle_cluster_resilience(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster reliability request
 */
bool ramd_api_handle_cluster_reliability(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster availability request
 */
bool ramd_api_handle_cluster_availability(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster durability request
 */
bool ramd_api_handle_cluster_durability(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster consistency request
 */
bool ramd_api_handle_cluster_consistency(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster isolation request
 */
bool ramd_api_handle_cluster_isolation(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster atomicity request
 */
bool ramd_api_handle_cluster_atomicity(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster integrity request
 */
bool ramd_api_handle_cluster_integrity(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster confidentiality request
 */
bool ramd_api_handle_cluster_confidentiality(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster authentication request
 */
bool ramd_api_handle_cluster_authentication(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster authorization request
 */
bool ramd_api_handle_cluster_authorization(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster accounting request
 */
bool ramd_api_handle_cluster_accounting(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster monitoring request
 */
bool ramd_api_handle_cluster_monitoring(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster alerting request
 */
bool ramd_api_handle_cluster_alerting(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster logging request
 */
bool ramd_api_handle_cluster_logging(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster tracing request
 */
bool ramd_api_handle_cluster_tracing(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster profiling request
 */
bool ramd_api_handle_cluster_profiling(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster debugging request
 */
bool ramd_api_handle_cluster_debugging(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster testing request
 */
bool ramd_api_handle_cluster_testing(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster validation request
 */
bool ramd_api_handle_cluster_validation(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster verification request
 */
bool ramd_api_handle_cluster_verification(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster certification request
 */
bool ramd_api_handle_cluster_certification(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster accreditation request
 */
bool ramd_api_handle_cluster_accreditation(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster compliance audit request
 */
bool ramd_api_handle_cluster_compliance_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster security audit request
 */
bool ramd_api_handle_cluster_security_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster performance audit request
 */
bool ramd_api_handle_cluster_performance_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster capacity audit request
 */
bool ramd_api_handle_cluster_capacity_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster utilization audit request
 */
bool ramd_api_handle_cluster_utilization_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster efficiency audit request
 */
bool ramd_api_handle_cluster_efficiency_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster optimization audit request
 */
bool ramd_api_handle_cluster_optimization_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster tuning audit request
 */
bool ramd_api_handle_cluster_tuning_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster scaling audit request
 */
bool ramd_api_handle_cluster_scaling_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster load balancing audit request
 */
bool ramd_api_handle_cluster_load_balancing_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster failover testing audit request
 */
bool ramd_api_handle_cluster_failover_testing_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster disaster recovery audit request
 */
bool ramd_api_handle_cluster_disaster_recovery_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster business continuity audit request
 */
bool ramd_api_handle_cluster_business_continuity_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster high availability audit request
 */
bool ramd_api_handle_cluster_high_availability_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster fault tolerance audit request
 */
bool ramd_api_handle_cluster_fault_tolerance_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster resilience audit request
 */
bool ramd_api_handle_cluster_resilience_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster reliability audit request
 */
bool ramd_api_handle_cluster_reliability_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster availability audit request
 */
bool ramd_api_handle_cluster_availability_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster durability audit request
 */
bool ramd_api_handle_cluster_durability_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster consistency audit request
 */
bool ramd_api_handle_cluster_consistency_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster isolation audit request
 */
bool ramd_api_handle_cluster_isolation_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster atomicity audit request
 */
bool ramd_api_handle_cluster_atomicity_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster integrity audit request
 */
bool ramd_api_handle_cluster_integrity_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster confidentiality audit request
 */
bool ramd_api_handle_cluster_confidentiality_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster authentication audit request
 */
bool ramd_api_handle_cluster_authentication_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster authorization audit request
 */
bool ramd_api_handle_cluster_authorization_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster accounting audit request
 */
bool ramd_api_handle_cluster_accounting_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster monitoring audit request
 */
bool ramd_api_handle_cluster_monitoring_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster alerting audit request
 */
bool ramd_api_handle_cluster_alerting_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster logging audit request
 */
bool ramd_api_handle_cluster_logging_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster tracing audit request
 */
bool ramd_api_handle_cluster_tracing_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster profiling audit request
 */
bool ramd_api_handle_cluster_profiling_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster debugging audit request
 */
bool ramd_api_handle_cluster_debugging_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster testing audit request
 */
bool ramd_api_handle_cluster_testing_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster validation audit request
 */
bool ramd_api_handle_cluster_validation_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster verification audit request
 */
bool ramd_api_handle_cluster_verification_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster certification audit request
 */
bool ramd_api_handle_cluster_certification_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

/*
 * Handle cluster accreditation audit request
 */
bool ramd_api_handle_cluster_accreditation_audit(ramd_http_request_t* req, ramd_http_response_t* resp);

#endif /* RAMD_API_ENHANCED_H */
