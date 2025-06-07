#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <pthread.h>
#include <time.h>

// Network metrics structure
typedef struct {
    double bandwidth_mbps;    // Measured bandwidth in Mbps
    double latency_ms;        // Measured latency in milliseconds
    double packet_loss;       // Packet loss ratio (0.0 - 1.0)
    double last_transfer_speed; // Last measured transfer speed in bytes/sec
    int current_threads;      // Current number of threads being used
    int optimal_threads;      // Calculated optimal number of threads
    time_t last_update;       // Timestamp of the last update
    pthread_mutex_t lock;     // Mutex to protect concurrent access
} network_metrics_t;

// Start the network monitor thread
void start_network_monitor(void);

// Stop the network monitor thread
void stop_network_monitor(void);

// Get the current optimal number of threads based on network conditions
int get_optimal_threads(void);

// Update transfer statistics to help inform thread optimization
void update_transfer_stats(size_t bytes_transferred, double time_taken);

// Get a copy of the current network metrics
network_metrics_t get_network_metrics(void);

#endif /* NETWORK_MONITOR_H */