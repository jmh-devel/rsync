# Plan for Parallelizing rsync

This document outlines the plan to refactor `rsync` to support parallel file transfers, improving its efficiency, especially over WAN connections.

## 1. Project Goals

- Implement a new command-line option `--parallel, -j {n}` to specify the number of parallel threads.
- If `n` is not provided, automatically determine the optimal number of threads based on network performance and system resources.
- Refactor the `rsync` core to enable parallel file transfers.
- Ensure the implementation is robust and efficient for WAN environments.

## 2. Accomplishments

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

## 3. Remaining Work

- **Fix Parallel Writes:**
    - The `write_file` function in `fileio.c` has been modified to use `pwrite` for thread-safe writes.
- **Auto-Detection of Thread Count:**
    - Created `network_monitor.h` and `network_monitor.c` to house the network monitoring logic.
    - Integrated the network monitor into `main.c` to be called when the `--parallel` option is used without an argument.
    - The current implementation of `get_optimal_threads` is a placeholder and needs to be replaced with actual network monitoring and dynamic thread adjustment logic.
- **Testing:**
    - Conduct thorough testing of the parallel implementation under various network conditions (e.g., high latency, low bandwidth, packet loss) to ensure its stability and performance.
- **Documentation:**
    - Update `INSTALL.md` and other relevant documentation to explain the new `--parallel` option and its usage.