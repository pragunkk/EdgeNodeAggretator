#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#define MODEL_SIZE 1000
#define NUM_LOCKS 10
#define CHUNK_SIZE (MODEL_SIZE / NUM_LOCKS)

#define SERVER_PORT 8080
#define EVAL_INTERVAL 3 // Evaluate after 3 gradient updates

#define SHM_PATH "/fedavg_shm"
#define SEM_WRITE "/fedavg_sem_write"
#define SEM_READ "/fedavg_sem_read"
#define IPC_PROJ_ID 'F'

// Role Enums
typedef enum {
    ROLE_ADMIN = 1,
    ROLE_EDGE_NODE = 2,
    ROLE_GUEST = 3
} Role;

// Command Enums
typedef enum {
    CMD_PUSH_GRAD = 10,
    CMD_GET_MODEL = 11,
    CMD_RESET = 12,
    CMD_SHUTDOWN = 13 // Additional command explicitly for admin
} Command;

// The standard network payload sent by clients
typedef struct {
    Role role;
    Command cmd;
    float gradients[MODEL_SIZE]; // Only populated if cmd == CMD_PUSH_GRAD
} NetworkPayload;

// Payload sent back by server
typedef struct {
    int status; // 0 for OK, -1 for error
    int update_count; // Number of training updates so far
    float model_weights[MODEL_SIZE]; // Populated for CMD_GET_MODEL
} ResponsePayload;

// Shared Memory Structure for IPC
typedef struct {
    float model_copy[MODEL_SIZE];
    int update_count;
    int shutdown_flag;
} SharedMemoryBlock;

#endif // COMMON_H
