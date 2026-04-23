#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "common.h"

int main(int argc, char const *argv[]) {
    if (argc < 3) {
        printf("Usage: %s <server_ip> <reset|shutdown>\n", argv[0]);
        return -1;
    }
    
    const char *server_ip = argv[1];
    Command admin_cmd;
    if (strcmp(argv[2], "reset") == 0) {
        admin_cmd = CMD_RESET;
    } else if (strcmp(argv[2], "shutdown") == 0) {
        admin_cmd = CMD_SHUTDOWN;
    } else {
        printf("Invalid command. Use 'reset' or 'shutdown'\n");
        return -1;
    }

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
    payload.role = ROLE_ADMIN;
    payload.cmd  = admin_cmd;
    
    send(sock, &payload, sizeof(NetworkPayload), 0);
    
    ResponsePayload response;
    recv(sock, &response, sizeof(ResponsePayload), MSG_WAITALL);
    
    if (response.status == 0) {
        printf("[Admin] Command %s executed successfully.\n", argv[2]);
    } else {
        printf("[Admin] Unauthorized or failed to execute command.\n");
    }
    
    close(sock);
    return 0;
}
