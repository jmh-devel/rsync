#include "network_monitor.h"
#include "rsync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

// Global network metrics
static network_metrics_t metrics = {
    .bandwidth_mbps = 10.0,       // Default to conservative 10 Mbps
    .latency_ms = 100.0,          // Default to 100ms latency
    .packet_loss = 0.01,          // Default to 1% packet loss
    .last_transfer_speed = 0.0,   // No transfer yet
    .current_threads = 1,         // Start with 1 thread
    .optimal_threads = 4,         // Default optimal is 4 threads
    .last_update = 0              // Not updated yet
};

static pthread_t monitor_thread;
static int monitor_running = 0;
static pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;

// History of measurements for smoothing
#define HISTORY_SIZE 10
static double speed_history[HISTORY_SIZE] = {0};
static int history_index = 0;

// Function to calculate the bandwidth-delay product
static double calculate_bdp(double bandwidth_mbps, double latency_ms) {
    // BDP = bandwidth * RTT
    // Convert bandwidth from Mbps to bytes/ms
    double bandwidth_bytes_per_ms = (bandwidth_mbps * 1000000) / 8000;
    return bandwidth_bytes_per_ms * latency_ms;
}

// Function to calculate optimal thread count based on network metrics
static int calculate_optimal_threads(network_metrics_t *metrics) {
    // Calculate BDP in bytes
    double bdp = calculate_bdp(metrics->bandwidth_mbps, metrics->latency_ms);
    
    // Default socket buffer size (can be tuned in actual implementation)
    const int socket_buffer_size = 64 * 1024; // 64KB
    
    // Base calculation: BDP / socket_buffer_size
    int optimal = ceil(bdp / socket_buffer_size);
    
    // Adjust for packet loss - higher loss may benefit from more threads
    // Up to 50% more threads for high packet loss
    if (metrics->packet_loss > 0.05) {
        optimal = ceil(optimal * (1 + metrics->packet_loss * 5));
    }
    
    // Use at least 2 threads for parallelism, at most 32 for resource conservation
    optimal = optimal < 2 ? 2 : optimal;
    optimal = optimal > 32 ? 32 : optimal;
    
    // If we have transfer speed history, use it to fine-tune
    if (metrics->last_transfer_speed > 0) {
        // Calculate average speed from history
        double avg_speed = 0;
        int count = 0;
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (speed_history[i] > 0) {
                avg_speed += speed_history[i];
                count++;
            }
        }
        if (count > 0) {
            avg_speed /= count;
            
            // If current speed is significantly less than the average,
            // reduce the thread count to avoid contention
            if (metrics->last_transfer_speed < avg_speed * 0.7 && 
                metrics->current_threads > 2) {
                optimal = metrics->current_threads - 1;
            } 
            // If current speed is significantly better than the average,
            // and we're not at our calculated optimal, try more threads
            else if (metrics->last_transfer_speed > avg_speed * 1.1 && 
                     metrics->current_threads < optimal) {
                optimal = metrics->current_threads + 1;
            }
        }
    }
    
    return optimal;
}

// Active monitoring function that runs in a separate thread
static void *monitor_function(void *arg) {
    struct timeval last_check, current_time;
    gettimeofday(&last_check, NULL);
    
    while (monitor_running) {
        sleep(2); // Check every 2 seconds
        
        gettimeofday(&current_time, NULL);
        double elapsed = (current_time.tv_sec - last_check.tv_sec) + 
                        (current_time.tv_usec - last_check.tv_usec) / 1000000.0;
        
        // In a real implementation, we would perform actual network tests here
        // For now, we use synthetic measurements based on transfer stats
        
        pthread_mutex_lock(&metrics.lock);
        
        // Calculate optimal thread count
        metrics.optimal_threads = calculate_optimal_threads(&metrics);
        metrics.last_update = time(NULL);
        
        pthread_mutex_unlock(&metrics.lock);
        
        last_check = current_time;
    }
    
    return NULL;
}

void start_network_monitor(void) {
    pthread_mutex_lock(&monitor_mutex);
    
    if (!monitor_running) {
        // Initialize mutex in metrics
        pthread_mutex_init(&metrics.lock, NULL);
        
        monitor_running = 1;
        if (pthread_create(&monitor_thread, NULL, monitor_function, NULL) != 0) {
            monitor_running = 0;
            rsyserr(FERROR, errno, "failed to create network monitor thread");
        }
    }
    
    pthread_mutex_unlock(&monitor_mutex);
}

void stop_network_monitor(void) {
    pthread_mutex_lock(&monitor_mutex);
    
    if (monitor_running) {
        monitor_running = 0;
        pthread_join(monitor_thread, NULL);
        pthread_mutex_destroy(&metrics.lock);
    }
    
    pthread_mutex_unlock(&monitor_mutex);
}

int get_optimal_threads(void) {
    int optimal;
    
    pthread_mutex_lock(&metrics.lock);
    optimal = metrics.optimal_threads;
    pthread_mutex_unlock(&metrics.lock);
    
    return optimal;
}

void update_transfer_stats(size_t bytes_transferred, double time_taken) {
    if (time_taken <= 0) return;
    
    // Calculate speed in bytes per second
    double speed = bytes_transferred / time_taken;
    
    pthread_mutex_lock(&metrics.lock);
    
    // Update speed history
    speed_history[history_index] = speed;
    history_index = (history_index + 1) % HISTORY_SIZE;
    
    // Update metrics
    metrics.last_transfer_speed = speed;
    
    // Estimate bandwidth (conservatively) from transfer speed
    // We use a smoothing factor to avoid radical changes
    double estimated_bandwidth_mbps = (speed * 8) / 1000000; // Convert bytes/sec to Mbps
    metrics.bandwidth_mbps = metrics.bandwidth_mbps * 0.7 + estimated_bandwidth_mbps * 0.3;
    
    // We don't have a good way to directly measure latency and packet loss in this context
    // In a real implementation, we would use ping or similar tools
    
    // Update optimal threads based on new measurements
    metrics.optimal_threads = calculate_optimal_threads(&metrics);
    metrics.last_update = time(NULL);
    
    pthread_mutex_unlock(&metrics.lock);
}

network_metrics_t get_network_metrics(void) {
    network_metrics_t result;
    
    pthread_mutex_lock(&metrics.lock);
    memcpy(&result, &metrics, sizeof(network_metrics_t));
    pthread_mutex_unlock(&metrics.lock);
    
    return result;
}