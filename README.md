# Federated Learning Parameter Server 🌐

A high-performance, multi-threaded parameter aggregation server built entirely in C. This system orchestrates edge nodes performing simulated local training, aggregating their updates into a global machine learning model using **Federated Learning**. 

This system acts as the practical culmination for the **EGC 301P / Operating Systems Lab MiniProject**, flawlessly demonstrating core OS paradigms like Concurrency, Inter-Process Communication, and low-level File/Network IO.

---

## 🚀 How to Build and Run

### 1. Prerequisites
This project requires a POSIX-compliant environment due to the reliance on Linux-specific IPC headers (`<sys/shm.h>`, `pthread`, `semaphore.h`). 
- **Windows Users:** Use **WSL** (Windows Subsystem for Linux) or MSYS2.
- **Linux/Mac:** Natively supported.

### 2. Compilation
To compile the system, open a terminal in the project directory and run:
```bash
make
```
This generates the core binaries: `server`, `client_node`, `guest_client`, `admin_client`, and our interactive `dashboard`.

### 3. Usage & Demonstration
We have provided an ANSI-colored, arrow-navigable Terminal UI Dashboard to make testing and demonstration effortless!

1. **Start the Core Server:** Open your first terminal and start the daemon. Leave this window open. 
   ```bash
   ./server
   ```
2. **Launch the Dashboard:** Open a *second* terminal in the same directory and launch the UI.
   ```bash
   ./dashboard
   ```
3. Use the arrow keys to seamlessly orchestrate simulated Edge Nodes computing gradients, Guests downloading models, or Admins wiping the memory.

*(Note: You can also run the client binaries manually, e.g., `./client_node 127.0.0.1 5` if you prefer the raw command-line experience).*

---

## 📘 Implemented OS Concepts (Requirements Breakdown)

This section details exactly where and how each mandatory project requirement is implemented in the source code.

### 1. Role-Based Authorization 🛡️
* **Concept:** Restrict operations based on permission levels.
* **Implementation:** The system defines three discrete roles `ROLE_ADMIN`, `ROLE_EDGE_NODE`, and `ROLE_GUEST` inside `common.h`.
* **Code Reference:** In `server.c` (inside the `handle_client` thread), the `payload.role` is strictly verified.
  - **Edge Nodes** are solely permitted to pass `CMD_PUSH_GRAD` instructions.
  - **Guests** have Read-Only limits (`CMD_GET_MODEL`).
  - **Admins** have absolute authority over `CMD_RESET` to wipe the global variables, and `CMD_SHUTDOWN` to terminate the process loop.

### 2. File Locking 🔒
* **Concept:** Ensure safe persistence when interacting with shared files.
* **Implementation:** Advisory locking using POSIX `fcntl` enforces safety during model check-pointing mechanisms avoiding torn-states.
* **Code Reference:** In `server.c`, the function `save_checkpoint()` acquires an exclusive **Write Lock** (`F_WRLCK`) before dumping the `global_weights` array into `model_checkpoint.bin`. During initialization, `load_checkpoint_if_exists()` acquires a shared **Read Lock** (`F_RDLCK`) so the memory array isn't corrupted during boot.

### 3. Concurrency Control 🚦
* **Concept:** Handling multiple processes/threads.
* **Implementation:** Massive multi-threading where simultaneous client connections spin off into independent parallel structures (`pthread_create`).
* **Code Reference:** The global array state requires absolute protection. In `server.c`, we declare `pthread_mutex_t chunk_locks[NUM_LOCKS]` and `count_lock` to block multiple clients from overwriting structural variables simultaneously. 

### 4. Data Consistency 🧩
* **Concept:** Prevent Race conditions, Dirty reads, and Lost updates.
* **Implementation:** The system implements a hyper-efficient paradigm: **Fine-Grained Locking**.
* **Code Reference:** Rather than bottlenecking the application with a single global mutex lock, the `global_weights` array is sliced into specific `CHUNK_SIZE` intervals. When an Edge Node pushes gradients, the thread mathematically grabs a lock *only* localized to its active chunk `pthread_mutex_lock(&chunk_locks[chunk])` (Line ~143 in `server.c`). This prevents Lost Updates precisely without sacrificing parallel performance.

### 5. Socket Programming 🔌
* **Concept:** IPC across network borders (Client-Server model).
* **Implementation:** Standard TCP stream sockets are built for structural data passage.
* **Code Reference:** 
  - **Server Setup:** Forms an `AF_INET`, `SOCK_STREAM` socket, ensures it won't crash on reconnects with `SO_REUSEADDR`, and binds to `SERVER_PORT` (8080).
  - **Client Structure:** Clients resolve connections via `inet_pton` establishing a `connect()`. Information is seamlessly encoded into a `NetworkPayload` struct and pushed via `send()` and read via `recv()`.

### 6. Inter-Process Communication (IPC) 🔀
* **Concept:** Memory and event passing between entirely separate OS processes.
* **Implementation:** 100% of minimums were exceeded here. The primary server process uses `fork()` to spawn an asynchronous `run_evaluator_process()`. 
* **Code Reference:** The system utilizes three distinct IPC layers combined:
  1. **Shared Memory (`<sys/shm.h>`):** Maps a localized memory block mapping a `SharedMemoryBlock` struct between parent and child processes. 
  2. **POSIX Semaphores (`<semaphore.h>`):** The parent and child use `sem_wait()` and `sem_post()` on named semaphores (`/fedavg_sem_write`) to govern traffic on the Shared Memory bridge.
  3. **Signals (`<signal.h>`):** The application relies on asynchronous signaling `kill(evaluator_pid, SIGUSR1)` to intentionally wake the sleeping evaluator when new data requires processing!
