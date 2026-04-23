#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <sys/wait.h>
#include "common.h"

float global_weights[MODEL_SIZE];
pthread_mutex_t chunk_locks[NUM_LOCKS];
pthread_mutex_t count_lock;
int global_update_count = 0;
volatile sig_atomic_t running = 1;

int server_fd;
pid_t evaluator_pid = -1;
SharedMemoryBlock *shm_ptr = (SharedMemoryBlock *)-1;
sem_t *sem_write;
sem_t *sem_read;

// Evaluator Signal Handler
volatile sig_atomic_t eval_trigger = 0;
void handle_eval_signal(int sig) {
    if (sig == SIGUSR1) {
        eval_trigger = 1;
    }
}

// Server Shutdown Handlers
void handle_shutdown_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        close(server_fd);
    }
}

// File check-pointing with fcntl
void save_checkpoint() {
    int fd = open("model_checkpoint.bin", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Failed to open checkpoint file");
        return;
    }

    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl failed to acquire write lock");
        close(fd);
        return;
    }

    // Write array to file
    write(fd, global_weights, sizeof(float) * MODEL_SIZE);

    fl.l_type = F_UNLCK;
    if (fcntl(fd, F_SETLK, &fl) == -1) {
        perror("fcntl failed to release lock");
    }
    
    close(fd);
    printf("[Server] Checkpoint saved securely with fcntl.\n");
}

void load_checkpoint_if_exists() {
    int fd = open("model_checkpoint.bin", O_RDONLY);
    if (fd >= 0) {
        struct flock fl;
        memset(&fl, 0, sizeof(fl));
        fl.l_type = F_RDLCK;
        fl.l_whence = SEEK_SET;
        fcntl(fd, F_SETLKW, &fl);
        
        read(fd, global_weights, sizeof(float) * MODEL_SIZE);
        
        fl.l_type = F_UNLCK;
        fcntl(fd, F_SETLK, &fl);
        close(fd);
        printf("[Server] Loaded existing checkpoint.\n");
    } else {
        printf("[Server] No existing checkpoint, initializing weights to 0.\n");
        for (int i = 0; i < MODEL_SIZE; ++i) {
            global_weights[i] = 0.0f;
        }
    }
}

// Thread routine for handling client connections
void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    free(arg);

    NetworkPayload payload;
    ResponsePayload response;
    memset(&response, 0, sizeof(ResponsePayload));

    ssize_t bytes_received = recv(client_socket, &payload, sizeof(NetworkPayload), MSG_WAITALL);
    if (bytes_received != sizeof(NetworkPayload)) {
        response.status = -1;
        send(client_socket, &response, sizeof(ResponsePayload), 0);
        close(client_socket);
        return NULL;
    }

    // Role-based Authorization
    response.status = 0;
    if (payload.role == ROLE_ADMIN) {
        if (payload.cmd == CMD_RESET) {
            printf("[Admin] Resetting global model...\n");
            for (int i = 0; i < NUM_LOCKS; i++) pthread_mutex_lock(&chunk_locks[i]);
            pthread_mutex_lock(&count_lock);
            
            for (int i = 0; i < MODEL_SIZE; i++) global_weights[i] = 0.0f;
            global_update_count = 0;
            save_checkpoint();
            
            pthread_mutex_unlock(&count_lock);
            for (int i = 0; i < NUM_LOCKS; i++) pthread_mutex_unlock(&chunk_locks[i]);
            printf("[Admin] Model reset complete.\n");
        } else if (payload.cmd == CMD_SHUTDOWN) {
            printf("[Admin] Shutdown command received.\n");
            running = 0;
            close(server_fd); // Break the accept loop
        } else {
            response.status = -1; // Unauthorized cmd
        }
    } 
    else if (payload.role == ROLE_EDGE_NODE) {
        if (payload.cmd == CMD_PUSH_GRAD) {
            printf("[Edge Node] Received PUSH_GRAD. Aggregating...\n");
            // Concurrency Control: Apply chunk-wise locking for aggregation
            for (int chunk = 0; chunk < NUM_LOCKS; chunk++) {
                pthread_mutex_lock(&chunk_locks[chunk]);
                int start = chunk * CHUNK_SIZE;
                int end = start + CHUNK_SIZE;
                for (int i = start; i < end; i++) {
                    // FedAvg sum, assume each client provides delta W / n beforehand, or we just accumulate deltas
                    global_weights[i] += payload.gradients[i];
                }
                pthread_mutex_unlock(&chunk_locks[chunk]);
            }
            
            pthread_mutex_lock(&count_lock);
            global_update_count++;
            
            // Check if we should trigger the evaluator
            if (global_update_count % EVAL_INTERVAL == 0) {
                // IPC: Copy to Shared Memory
                sem_wait(sem_read); // Wait until child is done reading
                for (int i = 0; i < MODEL_SIZE; i++) {
                    shm_ptr->model_copy[i] = global_weights[i];
                }
                shm_ptr->update_count = global_update_count;
                sem_post(sem_write); // Signal child that new data is written
                
                // Signal child
                kill(evaluator_pid, SIGUSR1);
                
                // Also checkpoint to disk
                save_checkpoint();
            }
            response.update_count = global_update_count;
            pthread_mutex_unlock(&count_lock);
        } else {
            response.status = -1; // Unauthorized
        }
    } 
    else if (payload.role == ROLE_GUEST) {
        if (payload.cmd == CMD_GET_MODEL) {
            printf("[Guest] Serving GET_MODEL...\n");
            for (int chunk = 0; chunk < NUM_LOCKS; chunk++) {
                pthread_mutex_lock(&chunk_locks[chunk]);
                int start = chunk * CHUNK_SIZE;
                int end = start + CHUNK_SIZE;
                for (int i = start; i < end; i++) {
                    response.model_weights[i] = global_weights[i];
                }
                pthread_mutex_unlock(&chunk_locks[chunk]);
            }
            pthread_mutex_lock(&count_lock);
            response.update_count = global_update_count;
            pthread_mutex_unlock(&count_lock);
        } else {
            response.status = -1; // Unauthorized
        }
    } else {
        response.status = -1; // Unknown role
    }

    send(client_socket, &response, sizeof(ResponsePayload), 0);
    close(client_socket);
    return NULL;
}

// The Evaluator Process Code
void run_evaluator_process() {
    signal(SIGUSR1, handle_eval_signal);
    printf("[Evaluator] Process started. PID: %d\n", getpid());
    
    while (1) {
        pause(); // Sleep until signal arrives
        
        if (shm_ptr->shutdown_flag) {
            printf("[Evaluator] Shutdown flag detected. Exiting...\n");
            break;
        }
        
        if (eval_trigger) {
            eval_trigger = 0;
            sem_wait(sem_write);
            
            // Simulated validation calculation (L2 norm)
            float sum_sq = 0.0f;
            for (int i = 0; i < MODEL_SIZE; i++) {
                sum_sq += shm_ptr->model_copy[i] * shm_ptr->model_copy[i];
            }
            float l2_norm = sqrt(sum_sq);
            
            printf("=========================================\n");
            printf("[Evaluator] Round %d Evaluation\n", shm_ptr->update_count);
            printf("[Evaluator] Model L2 Norm score: %f\n", l2_norm);
            printf("=========================================\n");
            
            sem_post(sem_read);
        }
    }
    
    exit(0);
}

int main() {
    // 1. Initialize Locks
    for (int i = 0; i < NUM_LOCKS; i++) {
        pthread_mutex_init(&chunk_locks[i], NULL);
    }
    pthread_mutex_init(&count_lock, NULL);

    load_checkpoint_if_exists();

    // 2. Set up Shared Memory
    key_t shm_key = ftok("server.c", IPC_PROJ_ID);
    int shm_id = shmget(shm_key, sizeof(SharedMemoryBlock), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("shmget failed");
        exit(1);
    }
    shm_ptr = (SharedMemoryBlock *)shmat(shm_id, NULL, 0);
    shm_ptr->shutdown_flag = 0;

    // 3. Set up Semaphores
    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ);
    sem_write = sem_open(SEM_WRITE, O_CREAT, 0666, 0); // Initially locked
    sem_read = sem_open(SEM_READ, O_CREAT, 0666, 1);  // Initially free to write

    // 4. Fork the Evaluator Process
    evaluator_pid = fork();
    if (evaluator_pid < 0) {
        perror("fork failed");
        exit(1);
    } else if (evaluator_pid == 0) {
        // Child process
        run_evaluator_process();
    }

    // 5. Parent Network Setup
    signal(SIGINT, handle_shutdown_signal);
    signal(SIGTERM, handle_shutdown_signal);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(1);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        exit(1);
    }

    printf("[Server] Listening on port %d...\n", SERVER_PORT);

    // 6. Main Accept Loop
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        
        if (client_socket < 0) {
            if (!running) break;
            perror("accept failed");
            continue;
        }

        int *arg = malloc(sizeof(int));
        *arg = client_socket;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, arg) != 0) {
            perror("pthread_create failed");
            close(client_socket);
            free(arg);
        } else {
            pthread_detach(thread_id);
        }
    }

    printf("\n[Server] Shutting down cleanly...\n");
    
    // Checkpoint one last time
    save_checkpoint();

    // Signal evaluator to exit
    shm_ptr->shutdown_flag = 1;
    kill(evaluator_pid, SIGUSR1);
    waitpid(evaluator_pid, NULL, 0);

    // Cleanup IPC
    shmdt(shm_ptr);
    shmctl(shm_id, IPC_RMID, NULL);
    sem_close(sem_write);
    sem_close(sem_read);
    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ);

    // Cleanup Locks
    for (int i = 0; i < NUM_LOCKS; i++) pthread_mutex_destroy(&chunk_locks[i]);
    pthread_mutex_destroy(&count_lock);

    printf("[Server] Cleanup complete.\n");
    return 0;
}
