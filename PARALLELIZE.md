# Final Plan for Parallelizing rsync (Linux-Optimized)

This document outlines the final, comprehensive plan to refactor `rsync` for high-performance, parallel file transfers, specifically optimized for Linux environments.

## 1. Project Goals

- Implement a new command-line option `--parallel, -j {n}` to specify the number of parallel threads.
- Automatically determine the optimal number of threads based on network performance and system resources.
- Refactor the `rsync` core to enable parallel file transfers, leveraging modern Linux APIs for maximum performance.
- Add advanced features for resource management, resilience, and observability.

## 2. Final Implementation Plan

### 2.1. Foundational Fixes and Refinements

- **Fix `threadpool.c` Bug & Add Assertions:**
    - Remove the redundant `pthread_mutex_unlock` call.
    - Wrap all mutex lock/unlock pairs with a macro that includes error checking and assertions.
- **Centralize Command-Line Parsing:**
    - Refactor argument parsing to rely exclusively on `popt`.
- **Implement a Ring Buffer for the Task Pool:**
    - Replace `malloc`/`free` with a lock-free ring buffer for task management.

### 2.2. Asynchronous I/O & Multiplexing

- **`io_uring` for Zero-Copy I/O:**
    - Replace blocking `read()`/`write()` calls with `io_uring` for asynchronous, zero-copy I/O.
    - Use `splice(2)`/`sendfile(2)` as fallbacks for older kernels.
- **`epoll`-Driven Control Plane:**
    - Use `epoll` to manage the main control socket, and `eventfd` to signal task readiness to worker threads.

### 2.3. Chunked Parallel I/O for Large Files

- **Dynamic Chunk Size Selection:**
    - Make chunk size configurable via `--chunk-size`.
    - The adaptive tuner will adjust the chunk size based on network conditions.
- **Chunk-Oriented Delta Algorithm:**
    - Refactor the delta-generation logic to be chunk-oriented.

### 2.4. Adaptive Tuning and Network Monitoring

- **Refined EWMA Smoothing:**
    - Use a decay factor (α ≈ 0.15) for the EWMA.
- **Network Metrics via `TCP_INFO` & Netlink:**
    - Use `getsockopt()` with `TCP_INFO` and Netlink to get kernel-measured RTT, `cwnd`, and per-interface stats.
- **Clear Thread Scaling Policy:**
    - Implement a clear and well-defined thread scaling policy based on throughput.

### 2.5. Advanced Features

- **Pluggable Compression Backends & Hardware Offload:**
    - Create a pluggable compression interface to support LZ4, Zstd, and hardware offload (e.g., Intel QuickAssist).
- **File-Type Heuristics:**
    - Automatically skip compression for incompressible files.
- **File Prioritization & Scheduling:**
    - Implement a two-tier scheduler to prioritize small files.
    - Allow users to specify file priority with a `--priority-pattern=<glob>` option.

### 2.6. Resource Management & QoS

- **CPU Affinity:**
    - Use `sched_setaffinity()` to allow pinning worker threads to specific cores.
- **Resource Capping:**
    - Add options for `--max-memory`, `--cpu-affinity`, and `--nice`.
- **Rate Limiting:**
    - Implement a token-bucket rate limiter to enforce bandwidth caps.

### 2.7. Resilience & Resume Support

- **Per-Chunk Checksums & Session Manifest:**
    - Introduce per-chunk checksums and a session manifest (e.g., JSON) to allow for resuming transfers.

### 2.8. Observability & Metrics

- **Prometheus-Style Metrics:**
    - Emit Prometheus-style counters for throughput, error rates, and queue depths.
- **Status Interval:**
    - Add a `--status-interval` flag for polling transfer progress.

### 2.9. Documentation and Testing

- **Incremental Development and Testing:**
    - Break the work into small, incremental pull requests.
    - Use `netem` and `perf` to create a robust, Linux-only test harness.
- **Update `PARALLELIZE.md`:**
    - This document will be kept up-to-date.
- **User Documentation:**
    - Create a user-facing "Parallel rsync HOWTO".

---

## 3. Original Plan (Archive)

The original plan and accomplishments are archived below for historical reference.

### 3.1. Accomplishments

- **Initial Research:** Analyzed the existing `rsync` architecture by reviewing the following files:
    - `INSTALL.md`
    - `rsync.c`
    - `main.c`
    - `options.c`
    - `sender.c`
    - `receiver.c`
    - `io.c`
- **Command-Line Option:** Added the `--parallel, -j` option to `options.c` to accept the number of threads.
- **Thread Pool:**
    - Created `threadpool.h` and `threadpool.c` to implement a reusable thread pool.
    - Integrated the thread pool into `main.c`, initializing it when the `--parallel` option is used.
- **Sender-Side Parallelization:**
    - Modified `sender.c` to dispatch file transfer tasks to the thread pool.
    - Created a task structure to encapsulate the necessary information for each file transfer.
    - Implemented a mutex to synchronize access to the network socket from multiple threads.
- **Receiver-Side Modifications:**
    - Modified `receiver.c` to handle the parallel data stream by reading the file index from the stream.
- **Protocol Extension:**
    - Modified the protocol to include the file index in the data stream, allowing the receiver to identify which file the data belongs to.
- **Network Monitor:**
    - Implemented a comprehensive network monitoring system in `network_monitor.c` and `network_monitor.h`.
    - The system tracks bandwidth, latency, packet loss, and transfer speeds.
    - It uses a time-series based analysis with a rolling history of network performance metrics.
    - It includes an intelligent thread calculation using the Bandwidth-Delay Product (BDP) formula.
    - The system continuously monitors network conditions and adjusts the thread count accordingly.
    - The sender code in `sender.c` has been updated to track file transfer time and update network statistics.
    - The main code in `main.c` now automatically starts the network monitor when `-j` or `--parallel` is used without an argument.

### 3.2. Remaining Work (Original)

#### 3.2.1. C-Optimization Principles

1.  **Tight, Predictable Inner Loops**
    -   Use pointer arithmetic or `memcpy`/`write(2)` for bulk I/O instead of per-byte operations.
    -   Minimize branching inside hot loops; hoist tests or use lookup tables where possible.
2.  **In-Place Data Structures & Buffering**
    -   Favor contiguous buffers (arrays) for checksum and delta computations to maximize cache locality.
    -   Pre-allocate working buffers rather than repeatedly `malloc`/`free`.
3.  **Amortized Analysis & Appropriate ADTs**
    -   Use hash tables for per-file metadata lookups (O(1) average) rather than trees, unless you need ordering.
    -   For chunk-based transfers, size your queues so that per-chunk enqueue/dequeue is O(1) amortized.
4.  **Avoid Excessive Recursion**
    -   Implement directory traversal and file-list recursion as explicit loops or stack-based traversals to avoid call-stack overhead.

#### 3.2.2. Concurrency & Parallelism Foundations (Original)

1.  **Thread Pool + Work Queue**
    -   Spawn a fixed pool of N worker threads on startup.
    -   Push per-file or per-chunk “transfer tasks” onto a lock-protected or lock-free queue.
    -   Workers pop tasks and drive the existing sender/receiver logic in parallel.
2.  **Minimal Synchronization**
    -   Use a mutex only for brief shared‐state updates (e.g., bandwidth metrics, queue head/tail).
    -   Consider a single producer (main thread) / multiple consumer (workers) pattern to simplify locking.
3.  **Adaptive Back-off & Tuning**
    -   Maintain a small EWMA over recent per-thread throughput.
    -   Increase or decrease active threads when throughput per thread falls below/above thresholds.
    -   Probe network RTT and bandwidth at startup (via a small “ping” exchange) to pick initial chunk size and thread count.
4.  **Chunked Parallel I/O**
    -   For large files, split into sub-ranges so multiple threads can stream distinct ranges simultaneously.
    -   Use `lseek` + `read`/`write` on each thread to avoid interfering with one another’s file descriptors.
5.  **Compression & Heuristics**
    -   Plug in fast backends (LZ4/Zstd) alongside zlib.
    -   Skip compression for known incompressible types (`.jpg`, `.mp4`).
    -   Allow per-thread `--compress-level` overrides to balance CPU vs. bandwidth.

#### 3.2.3. Next Steps (Original)

1.  **Build the Thread Pool** & work-queue abstraction.
2.  **Wire that into `main.c`** so that when `-j N` is used, you don’t just call a stub—you push tasks to N threads.
3.  **Implement chunked reads/writes** so each worker can independently stream different segments of big files.
4.  **Complete the adaptive tuning loop**: periodically consult `network_metrics_t`, then spawn or retire threads.
5.  **Hook in alternate compressors** and file-type skip logic.
6.  **Bolster the test suite** with multi-threaded scenarios and performance benchmarks.