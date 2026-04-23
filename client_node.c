#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

// Simulate local training on artificial stock fraud data
void simulate_local_training(float *gradients) {
    // We are simulating gradients for a simple 1D output or flat neural net
    // Fraud data points could generate spikes in certain parameters
    for (int i = 0; i < MODEL_SIZE; i++) {
        // Random noise gradient -0.5 to +0.5
        float delta = ((float)rand() / (float)(RAND_MAX)) - 0.5f;
        
        // Let's say index 500-510 represent tracking features of fraud (e.g. transaction speed)
        // Ensure consistent positive push to simulate learning of a fraud pattern
        if (i >= 500 && i <= 510) {
            delta = 0.5f + ((float)rand() / (float)(RAND_MAX)) * 0.5f; // +0.5 to +1.0
        }
        
        // Scale down the gradient with a mock learning rate (e.g., 0.01)
        gradients[i] = delta * 0.01f;
    }
}

int connect_and_push(const char *server_ip) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("\n Socket creation error \n");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    // Convert IPv4 and IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("\nConnection Failed \n");
        return -1;
    }
    
    NetworkPayload payload;
    payload.role = ROLE_EDGE_NODE;
    payload.cmd  = CMD_PUSH_GRAD;
    
    simulate_local_training(payload.gradients);
    
    send(sock, &payload, sizeof(NetworkPayload), 0);
    
    ResponsePayload response;
    recv(sock, &response, sizeof(ResponsePayload), MSG_WAITALL);
    
    if (response.status == 0) {
        printf("[Edge Node] Successfully pushed gradients. Server model update round: %d\n", response.update_count);
    } else {
        printf("[Edge Node] Error from server. Unauthorized or malformed payload.\n");
    }
    
    close(sock);
    return 0;
}

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip> [rounds]\n", argv[0]);
        return -1;
    }
    
    const char *server_ip = argv[1];
    int rounds = 1;
    if (argc >= 3) {
        rounds = atoi(argv[2]);
    }
    
    srand(time(NULL) ^ getpid()); // Seed pseudo random
    
    for (int i = 0; i < rounds; i++) {
        printf("\n--- Local Training Round %d ---\n", i + 1);
        connect_and_push(server_ip);
        sleep(1); // Small delay to simulate training time locally
    }
    
    return 0;
}
