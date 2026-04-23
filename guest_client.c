#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

int main(int argc, char const *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return -1;
    }
    
    const char *server_ip = argv[1];

    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("\n Socket creation error \n");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    
    if(inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        perror("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("\nConnection Failed \n");
        return -1;
    }
    
    NetworkPayload payload;
    memset(&payload, 0, sizeof(NetworkPayload));
    payload.role = ROLE_GUEST;
    payload.cmd  = CMD_GET_MODEL;
    
    send(sock, &payload, sizeof(NetworkPayload), 0);
    
    ResponsePayload response;
    recv(sock, &response, sizeof(ResponsePayload), MSG_WAITALL);
    
    if (response.status == 0) {
        printf("[Guest] Downloaded Model Weights (Round: %d)\n", response.update_count);
        // Print Summary stats
        float min = response.model_weights[0];
        float max = response.model_weights[0];
        float sum = 0.0f;
        for (int i = 0; i < MODEL_SIZE; i++) {
            if (response.model_weights[i] < min) min = response.model_weights[i];
            if (response.model_weights[i] > max) max = response.model_weights[i];
            sum += response.model_weights[i];
        }
        
        printf("--- Summary Statistics ---\n");
        printf("Model Size: %d weights\n", MODEL_SIZE);
        printf("Min value: %f\n", min);
        printf("Max value: %f\n", max);
        printf("Average weight: %f\n", sum / MODEL_SIZE);
        
        printf("\nDetailed look at fraud indicator features (Params 500-510):\n");
        for(int i = 500; i <= 510; i++) {
            printf("Weight[%d] = %f\n", i, response.model_weights[i]);
        }
    } else {
        printf("[Guest] Failed to retrieve model.\n");
    }
    
    close(sock);
    return 0;
}
