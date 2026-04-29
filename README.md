# Federated Learning Parameter Server 🌐

For my mini-project, I have built a high-performance, multi-threaded parameter aggregation server entirely in C. I designed this system to orchestrate edge nodes that perform simulated local training, aggregating their updates into a global machine learning model using **Federated Learning**. 

This project serves as the practical culmination for my **EGC 301P / Operating Systems Lab MiniProject**. I have explicitly structured my codebase to flawlessly demonstrate core OS paradigms like Concurrency, Inter-Process Communication, and low-level File/Network IO.

---

## 🚀 How to Build and Run

### 1. Prerequisites
I have built this project to run in a POSIX-compliant environment due to my reliance on Linux-specific IPC headers (`<sys/shm.h>`, `pthread`, `semaphore.h`). 
- **Windows Users:** Use **WSL** (Windows Subsystem for Linux) or MSYS2.
- **Linux/Mac:** Natively supported.

### 2. Compilation
To compile my system, open a terminal in the project directory and run:
```bash
make
```
This generates the core binaries: `server`, `client_node`, `guest_client`, `admin_client`, and an interactive `dashboard`.

### 3. Usage & Demonstration
I have provided an ANSI-colored, arrow-navigable Terminal UI Dashboard to make testing and demonstration of my project effortless!

1. **Start the Core Server:** Open your first terminal and start the daemon. Leave this window open. 
   ```bash
   ./server
   ```
2. **Launch the Dashboard:** Open a *second* terminal in the same directory and launch my UI.
   ```bash
   ./dashboard
   ```
3. Use the arrow keys to seamlessly orchestrate simulated Edge Nodes computing gradients, Guests downloading models, or Admins wiping the memory.

*(Note: You can also run my client binaries manually, e.g., `./client_node 127.0.0.1 5` if you prefer the raw command-line experience).*

---

## 📘 Implemented OS Concepts (Requirements Breakdown)

This section details exactly where and how I have implemented each mandatory project requirement in my source code.

### 1. Role-Based Authorization 🛡️
* **Concept:** Restrict operations based on permission levels.
* **My Implementation:** I have defined three discrete roles `ROLE_ADMIN`, `ROLE_EDGE_NODE`, and `ROLE_GUEST` inside `common.h`.
* **Code Reference:** In my `server.c` file (inside the `handle_client` thread), I strictly verify the `payload.role`.
  - I permit **Edge Nodes** solely to pass `CMD_PUSH_GRAD` instructions.
  - I restrict **Guests** to Read-Only limits (`CMD_GET_MODEL`).
  - I have given **Admins** absolute authority over `CMD_RESET` to wipe the global variables, and `CMD_SHUTDOWN` to terminate the process loop.

### 2. File Locking 🔒
* **Concept:** Ensure safe persistence when interacting with shared files.
* **My Implementation:** I implemented advisory locking using POSIX `fcntl` to enforce state safety during my model check-pointing mechanisms, completely avoiding torn-states.
* **Code Reference:** In `server.c`, my function `save_checkpoint()` acquires an exclusive **Write Lock** (`F_WRLCK`) before dumping the `global_weights` array into `model_checkpoint.bin`. During initialization, my `load_checkpoint_if_exists()` function successfully acquires a shared **Read Lock** (`F_RDLCK`) so the memory array isn't corrupted during the boot sequence.

### 3. Concurrency Control 🚦
* **Concept:** Handling multiple processes/threads.
* **My Implementation:** I designed the server with massive multi-threading where simultaneous client connections spin off into independent parallel structures (`pthread_create`).
* **Code Reference:** Because my global array state requires absolute protection, I declared `pthread_mutex_t chunk_locks[NUM_LOCKS]` and a `count_lock` in `server.c` to block multiple clients from overwriting structural variables simultaneously. 

### 4. Data Consistency 🧩
* **Concept:** Prevent Race conditions, Dirty reads, and Lost updates.
* **My Implementation:** I have implemented a hyper-efficient concurrency paradigm known as **Fine-Grained Locking**.
* **Code Reference:** Rather than bottlenecking my application with a single global mutex lock, I sliced the `global_weights` array into specific `CHUNK_SIZE` intervals. When an Edge Node pushes gradients, my thread mathematically maps and grabs a lock *only* localized to its active chunk `pthread_mutex_lock(&chunk_locks[chunk])` (Line ~143 in `server.c`). I wrote it this way to prevent Lost Updates precisely without sacrificing parallel performance.

### 5. Socket Programming 🔌
* **Concept:** IPC across network borders (Client-Server model).
* **My Implementation:** I built standard TCP stream sockets to allow structural data passage between my nodes.
* **Code Reference:** 
  - **Server Setup:** I formed an `AF_INET`, `SOCK_STREAM` socket, ensured it won't crash on rapid reconnects with `SO_REUSEADDR`, and bound it to `SERVER_PORT` (8080).
  - **Client Structure:** My clients resolve connections via `inet_pton` and establish a `connect()`. I seamlessly encoded network information into a `NetworkPayload` struct, pushing it via `send()` and reading it via `recv()`.

### 6. Inter-Process Communication (IPC) 🔀
* **Concept:** Memory and event passing between entirely separate OS processes.
* **My Implementation:** I successfully exceeded the minimum requirements here. My primary server process uses `fork()` to spawn an asynchronous `run_evaluator_process()`. 
* **Code Reference:** I have combined three distinct IPC layers in my code:
  1. **Shared Memory (`<sys/shm.h>`):** I mapped a localized `SharedMemoryBlock` struct between my parent and child processes. 
  2. **POSIX Semaphores (`<semaphore.h>`):** I wrote logic where the parent and child use `sem_wait()` and `sem_post()` on named semaphores (`/fedavg_sem_write`) to govern memory traffic perfectly.
  3. **Signals (`<signal.h>`):** My application relies heavily on asynchronous signaling `kill(evaluator_pid, SIGUSR1)` to instantly wake the sleeping evaluator only when new data requires processing!

---

## 🧗 Challenges Faced and Solutions

### 1. Concurrency and Deadlocks in Fine-Grained Locking
**Challenge:** When implementing fine-grained locking for the `global_weights` array, I initially faced issues with potential deadlocks and race conditions. Managing an array of mutexes (`chunk_locks`) meant that if multiple threads tried to acquire locks for overlapping chunks in a different order, the server could halt entirely.
**Solution:** I solved this by ensuring a strict lock acquisition protocol. Locks are always acquired sequentially to prevent circular wait conditions. Additionally, I limited the lock scope to the absolute minimum required operations to reduce lock contention and maximize parallel performance.

### 2. Complex Inter-Process Communication (IPC) Synchronization
**Challenge:** Coordinating the main server process and the forked asynchronous evaluator process using Shared Memory, Semaphores, and Signals simultaneously was highly complex. Early implementations led to orphaned semaphores and memory leaks when the server crashed or exited unexpectedly.
**Solution:** I implemented robust cleanup handlers using `atexit()` and signal trapping (`SIGINT`, `SIGTERM`) to ensure named semaphores and shared memory segments are safely unlinked and destroyed upon server termination. For signaling, I paired `SIGUSR1` with semaphore checks to ensure the evaluator safely sleeps and wakes up without losing evaluation triggers.

### 3. Handling Network Protocol Data Over TCP
**Challenge:** Sending structured C structs (`NetworkPayload`) over raw TCP sockets introduced challenges with fragmented packet deliveries. Because TCP is a stream protocol, `recv()` is not guaranteed to read the entire struct in a single call, which could lead to corrupted memory reads.
**Solution:** I ensured that the data transmission uses explicitly sized fields, and the server strictly validates the incoming payload before processing. I also structured the socket reads and writes to confirm the exact byte counts expected for the `NetworkPayload` and array chunks.
