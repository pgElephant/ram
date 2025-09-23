package controllers

import (
	"context"
	"fmt"
	"time"

	"k8s.io/apimachinery/pkg/api/errors"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/types"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"
	"sigs.k8s.io/controller-runtime/pkg/controller/controllerutil"
	"sigs.k8s.io/controller-runtime/pkg/log"

	appsv1 "k8s.io/api/apps/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	ramv1 "github.com/pgelephant/pgraft/k8s/operator/api/v1"
)

// PostgreSQLClusterReconciler reconciles a PostgreSQLCluster object
type PostgreSQLClusterReconciler struct {
	client.Client
	Scheme *runtime.Scheme
}

//+kubebuilder:rbac:groups=ram.pgelephant.com,resources=postgresqlclusters,verbs=get;list;watch;create;update;patch;delete
//+kubebuilder:rbac:groups=ram.pgelephant.com,resources=postgresqlclusters/status,verbs=get;update;patch
//+kubebuilder:rbac:groups=ram.pgelephant.com,resources=postgresqlclusters/finalizers,verbs=update
//+kubebuilder:rbac:groups=apps,resources=deployments,verbs=get;list;watch;create;update;patch;delete
//+kubebuilder:rbac:groups=core,resources=services,verbs=get;list;watch;create;update;patch;delete
//+kubebuilder:rbac:groups=core,resources=configmaps,verbs=get;list;watch;create;update;patch;delete
//+kubebuilder:rbac:groups=core,resources=persistentvolumeclaims,verbs=get;list;watch;create;update;patch;delete
//+kubebuilder:rbac:groups=core,resources=secrets,verbs=get;list;watch;create;update;patch;delete

// Reconcile is part of the main kubernetes reconciliation loop
func (r *PostgreSQLClusterReconciler) Reconcile(ctx context.Context, req ctrl.Request) (ctrl.Result, error) {
	log := log.FromContext(ctx)

	// Fetch the PostgreSQLCluster instance
	cluster := &ramv1.PostgreSQLCluster{}
	err := r.Get(ctx, req.NamespacedName, cluster)
	if err != nil {
		if errors.IsNotFound(err) {
			log.Info("PostgreSQLCluster resource not found. Ignoring since object must be deleted.")
			return ctrl.Result{}, nil
		}
		log.Error(err, "Failed to get PostgreSQLCluster")
		return ctrl.Result{}, err
	}

	// Set default values
	r.setDefaults(cluster)

	// Update status
	if err := r.updateStatus(ctx, cluster); err != nil {
		log.Error(err, "Failed to update status")
		return ctrl.Result{}, err
	}

	// Create or update ConfigMap
	if err := r.reconcileConfigMap(ctx, cluster); err != nil {
		log.Error(err, "Failed to reconcile ConfigMap")
		return ctrl.Result{}, err
	}

	// Create or update Secret
	if err := r.reconcileSecret(ctx, cluster); err != nil {
		log.Error(err, "Failed to reconcile Secret")
		return ctrl.Result{}, err
	}

	// Create or update StatefulSet for PostgreSQL
	if err := r.reconcileStatefulSet(ctx, cluster); err != nil {
		log.Error(err, "Failed to reconcile StatefulSet")
		return ctrl.Result{}, err
	}

	// Create or update Service
	if err := r.reconcileService(ctx, cluster); err != nil {
		log.Error(err, "Failed to reconcile Service")
		return ctrl.Result{}, err
	}

	// Create or update RAMD Deployment
	if err := r.reconcileRAMDDeployment(ctx, cluster); err != nil {
		log.Error(err, "Failed to reconcile RAMD Deployment")
		return ctrl.Result{}, err
	}

	// Create or update RAMD Service
	if err := r.reconcileRAMDService(ctx, cluster); err != nil {
		log.Error(err, "Failed to reconcile RAMD Service")
		return ctrl.Result{}, err
	}

	// Create or update Monitoring resources
	if cluster.Spec.Monitoring.Enabled {
		if err := r.reconcileMonitoring(ctx, cluster); err != nil {
			log.Error(err, "Failed to reconcile Monitoring")
			return ctrl.Result{}, err
		}
	}

	return ctrl.Result{RequeueAfter: 30 * time.Second}, nil
}

// setDefaults sets default values for the PostgreSQLCluster
func (r *PostgreSQLClusterReconciler) setDefaults(cluster *ramv1.PostgreSQLCluster) {
	if cluster.Spec.PostgreSQL.Version == "" {
		cluster.Spec.PostgreSQL.Version = "17"
	}
	if cluster.Spec.PostgreSQL.Image == "" {
		cluster.Spec.PostgreSQL.Image = "postgres:17"
	}
	if cluster.Spec.RAMD.Image == "" {
		cluster.Spec.RAMD.Image = "pgraft/ramd:latest"
	}
	if cluster.Spec.Networking.ServiceType == "" {
		cluster.Spec.Networking.ServiceType = corev1.ServiceTypeClusterIP
	}
	if cluster.Spec.Networking.Ports.PostgreSQL == 0 {
		cluster.Spec.Networking.Ports.PostgreSQL = 5432
	}
	if cluster.Spec.Networking.Ports.RAMD == 0 {
		cluster.Spec.Networking.Ports.RAMD = 8080
	}
	if cluster.Spec.Networking.Ports.Prometheus == 0 {
		cluster.Spec.Networking.Ports.Prometheus = 9090
	}
}

// updateStatus updates the status of the PostgreSQLCluster
func (r *PostgreSQLClusterReconciler) updateStatus(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	// Get StatefulSet status
	statefulSet := &appsv1.StatefulSet{}
	err := r.Get(ctx, types.NamespacedName{
		Name:      cluster.Name + "-postgresql",
		Namespace: cluster.Namespace,
	}, statefulSet)

	if err != nil {
		if errors.IsNotFound(err) {
			cluster.Status.Phase = "Pending"
			cluster.Status.ReadyReplicas = 0
			cluster.Status.TotalReplicas = cluster.Spec.Replicas
		} else {
			return err
		}
	} else {
		cluster.Status.ReadyReplicas = statefulSet.Status.ReadyReplicas
		cluster.Status.TotalReplicas = *statefulSet.Spec.Replicas

		if statefulSet.Status.ReadyReplicas == *statefulSet.Spec.Replicas {
			cluster.Status.Phase = "Running"
		} else {
			cluster.Status.Phase = "Updating"
		}
	}

	// Update leader (simplified - in real implementation, query RAMD)
	if cluster.Status.ReadyReplicas > 0 {
		cluster.Status.Leader = fmt.Sprintf("%s-postgresql-0", cluster.Name)
	}

	// Update endpoints
	cluster.Status.Endpoints.Primary = fmt.Sprintf("%s-postgresql.%s.svc.cluster.local:%d",
		cluster.Name, cluster.Namespace, cluster.Spec.Networking.Ports.PostgreSQL)

	cluster.Status.Endpoints.Replicas = []string{}
	for i := int32(1); i < cluster.Spec.Replicas; i++ {
		replicaEndpoint := fmt.Sprintf("%s-postgresql-%d.%s-postgresql.%s.svc.cluster.local:%d",
			cluster.Name, i, cluster.Name, cluster.Namespace, cluster.Spec.Networking.Ports.PostgreSQL)
		cluster.Status.Endpoints.Replicas = append(cluster.Status.Endpoints.Replicas, replicaEndpoint)
	}

	return r.Status().Update(ctx, cluster)
}

// reconcileConfigMap creates or updates the ConfigMap
func (r *PostgreSQLClusterReconciler) reconcileConfigMap(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	configMap := &corev1.ConfigMap{
		ObjectMeta: metav1.ObjectMeta{
			Name:      cluster.Name + "-config",
			Namespace: cluster.Namespace,
		},
	}

	_, err := controllerutil.CreateOrUpdate(ctx, r.Client, configMap, func() error {
		configMap.Labels = map[string]string{
			"app":       "postgresql-cluster",
			"cluster":   cluster.Name,
			"component": "config",
		}

		// PostgreSQL configuration
		postgresqlConf := ""
		for key, value := range cluster.Spec.PostgreSQL.Parameters {
			postgresqlConf += fmt.Sprintf("%s = %s\n", key, value)
		}

		// RAMD configuration
		ramdConf := fmt.Sprintf(`{
  "cluster": {
    "name": "%s",
    "nodes": []
  },
  "postgresql": {
    "host": "localhost",
    "port": %d,
    "database": "postgres",
    "user": "postgres"
  },
  "monitoring": {
    "prometheus_port": %d,
    "metrics_interval": "%s"
  },
  "security": {
    "enable_ssl": %t,
    "rate_limiting": %t,
    "audit_logging": %t
  }
}`, cluster.Name, cluster.Spec.Networking.Ports.PostgreSQL,
			cluster.Spec.RAMD.Config.Monitoring.PrometheusPort,
			cluster.Spec.RAMD.Config.Monitoring.MetricsInterval,
			cluster.Spec.RAMD.Config.Security.EnableSSL,
			cluster.Spec.RAMD.Config.Security.RateLimiting,
			cluster.Spec.RAMD.Config.Security.AuditLogging)

		configMap.Data = map[string]string{
			"postgresql.conf": postgresqlConf,
			"ramd.json":       ramdConf,
		}

		return controllerutil.SetControllerReference(cluster, configMap, r.Scheme)
	})

	return err
}

// reconcileSecret creates or updates the Secret
func (r *PostgreSQLClusterReconciler) reconcileSecret(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	secret := &corev1.Secret{
		ObjectMeta: metav1.ObjectMeta{
			Name:      cluster.Name + "-secret",
			Namespace: cluster.Namespace,
		},
	}

	_, err := controllerutil.CreateOrUpdate(ctx, r.Client, secret, func() error {
		secret.Labels = map[string]string{
			"app":       "postgresql-cluster",
			"cluster":   cluster.Name,
			"component": "secret",
		}

		secret.Type = corev1.SecretTypeOpaque
		secret.Data = map[string][]byte{
			"postgres-password":    []byte("postgres"),
			"replication-password": []byte("replication"),
		}

		return controllerutil.SetControllerReference(cluster, secret, r.Scheme)
	})

	return err
}

// reconcileStatefulSet creates or updates the StatefulSet
func (r *PostgreSQLClusterReconciler) reconcileStatefulSet(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	statefulSet := &appsv1.StatefulSet{
		ObjectMeta: metav1.ObjectMeta{
			Name:      cluster.Name + "-postgresql",
			Namespace: cluster.Namespace,
		},
	}

	_, err := controllerutil.CreateOrUpdate(ctx, r.Client, statefulSet, func() error {
		statefulSet.Labels = map[string]string{
			"app":       "postgresql-cluster",
			"cluster":   cluster.Name,
			"component": "postgresql",
		}

		statefulSet.Spec = appsv1.StatefulSetSpec{
			Replicas: &cluster.Spec.Replicas,
			Selector: &metav1.LabelSelector{
				MatchLabels: map[string]string{
					"app":       "postgresql-cluster",
					"cluster":   cluster.Name,
					"component": "postgresql",
				},
			},
			Template: corev1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						"app":       "postgresql-cluster",
						"cluster":   cluster.Name,
						"component": "postgresql",
					},
				},
				Spec: corev1.PodSpec{
					Containers: []corev1.Container{
						{
							Name:  "postgresql",
							Image: cluster.Spec.PostgreSQL.Image,
							Ports: []corev1.ContainerPort{
								{
									ContainerPort: cluster.Spec.Networking.Ports.PostgreSQL,
									Name:          "postgresql",
								},
							},
							Env: []corev1.EnvVar{
								{
									Name: "POSTGRES_PASSWORD",
									ValueFrom: &corev1.EnvVarSource{
										SecretKeyRef: &corev1.SecretKeySelector{
											LocalObjectReference: corev1.LocalObjectReference{
												Name: cluster.Name + "-secret",
											},
											Key: "postgres-password",
										},
									},
								},
								{
									Name:  "POSTGRES_DB",
									Value: "postgres",
								},
							},
							VolumeMounts: []corev1.VolumeMount{
								{
									Name:      "postgresql-data",
									MountPath: "/var/lib/postgresql/data",
								},
								{
									Name:      "postgresql-config",
									MountPath: "/etc/postgresql/postgresql.conf",
									SubPath:   "postgresql.conf",
								},
							},
							Resources: cluster.Spec.PostgreSQL.Resources,
						},
					},
					Volumes: []corev1.Volume{
						{
							Name: "postgresql-config",
							VolumeSource: corev1.VolumeSource{
								ConfigMap: &corev1.ConfigMapVolumeSource{
									LocalObjectReference: corev1.LocalObjectReference{
										Name: cluster.Name + "-config",
									},
								},
							},
						},
					},
				},
			},
			VolumeClaimTemplates: []corev1.PersistentVolumeClaim{
				{
					ObjectMeta: metav1.ObjectMeta{
						Name: "postgresql-data",
					},
					Spec: corev1.PersistentVolumeClaimSpec{
						AccessModes: []corev1.PersistentVolumeAccessMode{
							corev1.ReadWriteOnce,
						},
						Resources: corev1.ResourceRequirements{
							Requests: corev1.ResourceList{
								corev1.ResourceStorage: cluster.Spec.PostgreSQL.Storage.Size,
							},
						},
						StorageClassName: &cluster.Spec.PostgreSQL.Storage.StorageClass,
					},
				},
			},
		}

		return controllerutil.SetControllerReference(cluster, statefulSet, r.Scheme)
	})

	return err
}

// reconcileService creates or updates the Service
func (r *PostgreSQLClusterReconciler) reconcileService(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	service := &corev1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      cluster.Name + "-postgresql",
			Namespace: cluster.Namespace,
		},
	}

	_, err := controllerutil.CreateOrUpdate(ctx, r.Client, service, func() error {
		service.Labels = map[string]string{
			"app":       "postgresql-cluster",
			"cluster":   cluster.Name,
			"component": "postgresql",
		}

		service.Spec = corev1.ServiceSpec{
			Type: cluster.Spec.Networking.ServiceType,
			Ports: []corev1.ServicePort{
				{
					Name:       "postgresql",
					Port:       cluster.Spec.Networking.Ports.PostgreSQL,
					TargetPort: intstr.FromInt(int(cluster.Spec.Networking.Ports.PostgreSQL)),
					Protocol:   corev1.ProtocolTCP,
				},
			},
			Selector: map[string]string{
				"app":       "postgresql-cluster",
				"cluster":   cluster.Name,
				"component": "postgresql",
			},
		}

		return controllerutil.SetControllerReference(cluster, service, r.Scheme)
	})

	return err
}

// reconcileRAMDDeployment creates or updates the RAMD Deployment
func (r *PostgreSQLClusterReconciler) reconcileRAMDDeployment(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	deployment := &appsv1.Deployment{
		ObjectMeta: metav1.ObjectMeta{
			Name:      cluster.Name + "-ramd",
			Namespace: cluster.Namespace,
		},
	}

	_, err := controllerutil.CreateOrUpdate(ctx, r.Client, deployment, func() error {
		deployment.Labels = map[string]string{
			"app":       "postgresql-cluster",
			"cluster":   cluster.Name,
			"component": "ramd",
		}

		replicas := int32(1)
		deployment.Spec = appsv1.DeploymentSpec{
			Replicas: &replicas,
			Selector: &metav1.LabelSelector{
				MatchLabels: map[string]string{
					"app":       "postgresql-cluster",
					"cluster":   cluster.Name,
					"component": "ramd",
				},
			},
			Template: corev1.PodTemplateSpec{
				ObjectMeta: metav1.ObjectMeta{
					Labels: map[string]string{
						"app":       "postgresql-cluster",
						"cluster":   cluster.Name,
						"component": "ramd",
					},
				},
				Spec: corev1.PodSpec{
					Containers: []corev1.Container{
						{
							Name:  "ramd",
							Image: cluster.Spec.RAMD.Image,
							Ports: []corev1.ContainerPort{
								{
									ContainerPort: cluster.Spec.Networking.Ports.RAMD,
									Name:          "ramd",
								},
								{
									ContainerPort: cluster.Spec.Networking.Ports.Prometheus,
									Name:          "prometheus",
								},
							},
							VolumeMounts: []corev1.VolumeMount{
								{
									Name:      "ramd-config",
									MountPath: "/etc/ramd/ramd.json",
									SubPath:   "ramd.json",
								},
							},
							Resources: cluster.Spec.RAMD.Resources,
						},
					},
					Volumes: []corev1.Volume{
						{
							Name: "ramd-config",
							VolumeSource: corev1.VolumeSource{
								ConfigMap: &corev1.ConfigMapVolumeSource{
									LocalObjectReference: corev1.LocalObjectReference{
										Name: cluster.Name + "-config",
									},
								},
							},
						},
					},
				},
			},
		}

		return controllerutil.SetControllerReference(cluster, deployment, r.Scheme)
	})

	return err
}

// reconcileRAMDService creates or updates the RAMD Service
func (r *PostgreSQLClusterReconciler) reconcileRAMDService(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	service := &corev1.Service{
		ObjectMeta: metav1.ObjectMeta{
			Name:      cluster.Name + "-ramd",
			Namespace: cluster.Namespace,
		},
	}

	_, err := controllerutil.CreateOrUpdate(ctx, r.Client, service, func() error {
		service.Labels = map[string]string{
			"app":       "postgresql-cluster",
			"cluster":   cluster.Name,
			"component": "ramd",
		}

		service.Spec = corev1.ServiceSpec{
			Type: cluster.Spec.Networking.ServiceType,
			Ports: []corev1.ServicePort{
				{
					Name:       "ramd",
					Port:       cluster.Spec.Networking.Ports.RAMD,
					TargetPort: intstr.FromInt(int(cluster.Spec.Networking.Ports.RAMD)),
					Protocol:   corev1.ProtocolTCP,
				},
				{
					Name:       "prometheus",
					Port:       cluster.Spec.Networking.Ports.Prometheus,
					TargetPort: intstr.FromInt(int(cluster.Spec.Networking.Ports.Prometheus)),
					Protocol:   corev1.ProtocolTCP,
				},
			},
			Selector: map[string]string{
				"app":       "postgresql-cluster",
				"cluster":   cluster.Name,
				"component": "ramd",
			},
		}

		return controllerutil.SetControllerReference(cluster, service, r.Scheme)
	})

	return err
}

// reconcileMonitoring creates or updates monitoring resources
func (r *PostgreSQLClusterReconciler) reconcileMonitoring(ctx context.Context, cluster *ramv1.PostgreSQLCluster) error {
	// This is a simplified implementation
	// In a real implementation, you would create ServiceMonitor, GrafanaDashboard, etc.
	return nil
}

// SetupWithManager sets up the controller with the Manager.
func (r *PostgreSQLClusterReconciler) SetupWithManager(mgr ctrl.Manager) error {
	return ctrl.NewControllerManagedBy(mgr).
		For(&ramv1.PostgreSQLCluster{}).
		Owns(&appsv1.StatefulSet{}).
		Owns(&appsv1.Deployment{}).
		Owns(&corev1.Service{}).
		Owns(&corev1.ConfigMap{}).
		Owns(&corev1.Secret{}).
		Complete(r)
}
