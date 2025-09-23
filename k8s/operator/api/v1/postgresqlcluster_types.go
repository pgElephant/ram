package v1

import (
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// PostgreSQLClusterSpec defines the desired state of PostgreSQLCluster
type PostgreSQLClusterSpec struct {
	// Replicas is the number of PostgreSQL replicas in the cluster
	// +kubebuilder:validation:Minimum=1
	// +kubebuilder:validation:Maximum=10
	// +kubebuilder:default=3
	Replicas int32 `json:"replicas"`

	// PostgreSQL configuration
	PostgreSQL PostgreSQLSpec `json:"postgresql"`

	// RAMD daemon configuration
	RAMD RAMDSpec `json:"ramd"`

	// Networking configuration
	Networking NetworkingSpec `json:"networking,omitempty"`

	// Monitoring configuration
	Monitoring MonitoringSpec `json:"monitoring,omitempty"`
}

// PostgreSQLSpec defines PostgreSQL-specific configuration
type PostgreSQLSpec struct {
	// Version of PostgreSQL to use
	// +kubebuilder:default="17"
	Version string `json:"version,omitempty"`

	// Docker image for PostgreSQL
	// +kubebuilder:default="postgres:17"
	Image string `json:"image,omitempty"`

	// Resource requirements
	Resources corev1.ResourceRequirements `json:"resources,omitempty"`

	// PostgreSQL configuration parameters
	Parameters map[string]string `json:"parameters,omitempty"`

	// Storage configuration
	Storage StorageSpec `json:"storage,omitempty"`

	// Backup configuration
	Backup BackupSpec `json:"backup,omitempty"`
}

// RAMDSpec defines RAMD daemon configuration
type RAMDSpec struct {
	// Docker image for RAMD daemon
	// +kubebuilder:default="pgraft/ramd:latest"
	Image string `json:"image,omitempty"`

	// Resource requirements
	Resources corev1.ResourceRequirements `json:"resources,omitempty"`

	// RAMD configuration
	Config RAMDConfig `json:"config,omitempty"`
}

// RAMDConfig defines RAMD-specific configuration
type RAMDConfig struct {
	// Cluster name
	ClusterName string `json:"clusterName,omitempty"`

	// Monitoring configuration
	Monitoring RAMDMonitoringConfig `json:"monitoring,omitempty"`

	// Security configuration
	Security RAMDSecurityConfig `json:"security,omitempty"`
}

// RAMDMonitoringConfig defines monitoring configuration
type RAMDMonitoringConfig struct {
	// Prometheus port
	// +kubebuilder:default=9090
	PrometheusPort int32 `json:"prometheusPort,omitempty"`

	// Metrics collection interval
	// +kubebuilder:default="10s"
	MetricsInterval string `json:"metricsInterval,omitempty"`
}

// RAMDSecurityConfig defines security configuration
type RAMDSecurityConfig struct {
	// Enable SSL/TLS
	// +kubebuilder:default=true
	EnableSSL bool `json:"enableSSL,omitempty"`

	// Enable rate limiting
	// +kubebuilder:default=true
	RateLimiting bool `json:"rateLimiting,omitempty"`

	// Enable audit logging
	// +kubebuilder:default=true
	AuditLogging bool `json:"auditLogging,omitempty"`
}

// StorageSpec defines storage configuration
type StorageSpec struct {
	// Size of the persistent volume
	// +kubebuilder:default="20Gi"
	Size string `json:"size,omitempty"`

	// Storage class for persistent volumes
	StorageClass string `json:"storageClass,omitempty"`
}

// BackupSpec defines backup configuration
type BackupSpec struct {
	// Enable backups
	// +kubebuilder:default=true
	Enabled bool `json:"enabled,omitempty"`

	// Cron schedule for backups
	// +kubebuilder:default="0 2 * * *"
	Schedule string `json:"schedule,omitempty"`

	// Number of days to retain backups
	// +kubebuilder:default=7
	Retention int32 `json:"retention,omitempty"`
}

// NetworkingSpec defines networking configuration
type NetworkingSpec struct {
	// Service type
	// +kubebuilder:validation:Enum=ClusterIP;NodePort;LoadBalancer
	// +kubebuilder:default=ClusterIP
	ServiceType corev1.ServiceType `json:"serviceType,omitempty"`

	// Port configuration
	Ports PortsSpec `json:"ports,omitempty"`
}

// PortsSpec defines port configuration
type PortsSpec struct {
	// PostgreSQL port
	// +kubebuilder:default=5432
	PostgreSQL int32 `json:"postgresql,omitempty"`

	// RAMD port
	// +kubebuilder:default=8080
	RAMD int32 `json:"ramd,omitempty"`

	// Prometheus port
	// +kubebuilder:default=9090
	Prometheus int32 `json:"prometheus,omitempty"`
}

// MonitoringSpec defines monitoring configuration
type MonitoringSpec struct {
	// Enable monitoring
	// +kubebuilder:default=true
	Enabled bool `json:"enabled,omitempty"`

	// Grafana configuration
	Grafana GrafanaSpec `json:"grafana,omitempty"`

	// Prometheus configuration
	Prometheus PrometheusSpec `json:"prometheus,omitempty"`
}

// GrafanaSpec defines Grafana configuration
type GrafanaSpec struct {
	// Enable Grafana
	// +kubebuilder:default=true
	Enabled bool `json:"enabled,omitempty"`

	// Admin password
	AdminPassword string `json:"adminPassword,omitempty"`
}

// PrometheusSpec defines Prometheus configuration
type PrometheusSpec struct {
	// Enable Prometheus
	// +kubebuilder:default=true
	Enabled bool `json:"enabled,omitempty"`

	// Data retention period
	// +kubebuilder:default="30d"
	Retention string `json:"retention,omitempty"`
}

// PostgreSQLClusterStatus defines the observed state of PostgreSQLCluster
type PostgreSQLClusterStatus struct {
	// Current phase of the cluster
	// +kubebuilder:validation:Enum=Pending;Running;Failed;Updating
	Phase string `json:"phase,omitempty"`

	// Number of ready replicas
	ReadyReplicas int32 `json:"readyReplicas,omitempty"`

	// Total number of replicas
	TotalReplicas int32 `json:"totalReplicas,omitempty"`

	// Current leader node
	Leader string `json:"leader,omitempty"`

	// Conditions represent the latest available observations
	Conditions []metav1.Condition `json:"conditions,omitempty"`

	// Endpoints for the cluster
	Endpoints ClusterEndpoints `json:"endpoints,omitempty"`
}

// ClusterEndpoints defines cluster endpoints
type ClusterEndpoints struct {
	// Primary endpoint
	Primary string `json:"primary,omitempty"`

	// Replica endpoints
	Replicas []string `json:"replicas,omitempty"`
}

//+kubebuilder:object:root=true
//+kubebuilder:subresource:status
//+kubebuilder:printcolumn:name="Phase",type="string",JSONPath=".status.phase"
//+kubebuilder:printcolumn:name="Ready",type="string",JSONPath=".status.readyReplicas"
//+kubebuilder:printcolumn:name="Total",type="integer",JSONPath=".status.totalReplicas"
//+kubebuilder:printcolumn:name="Leader",type="string",JSONPath=".status.leader"
//+kubebuilder:printcolumn:name="Age",type="date",JSONPath=".metadata.creationTimestamp"

// PostgreSQLCluster is the Schema for the postgresqlclusters API
type PostgreSQLCluster struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   PostgreSQLClusterSpec   `json:"spec,omitempty"`
	Status PostgreSQLClusterStatus `json:"status,omitempty"`
}

//+kubebuilder:object:root=true

// PostgreSQLClusterList contains a list of PostgreSQLCluster
type PostgreSQLClusterList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []PostgreSQLCluster `json:"items"`
}

func init() {
	SchemeBuilder.Register(&PostgreSQLCluster{}, &PostgreSQLClusterList{})
}
