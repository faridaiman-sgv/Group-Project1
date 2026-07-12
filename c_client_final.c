#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define BUFFER_SIZE 1024

int main() {
    const char* server_host = getenv("SERVER_HOST") ? getenv("SERVER_HOST") : "c-server1";
    int server_port = getenv("SERVER_PORT") ? atoi(getenv("SERVER_PORT")) : 5002;
    
    printf("[C Client] Starting... Connecting to %s:%d\n", server_host, server_port);
    fflush(stdout);
    
    while (1) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("[C Client] Socket creation failed");
            sleep(10);
            continue;
        }
        
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        struct hostent *host = gethostbyname(server_host);
        if (host == NULL) {
            printf("[C Client] Failed to resolve: %s\n", server_host);
            close(sock);
            sleep(10);
            continue;
        }
        memcpy(&server_addr.sin_addr, host->h_addr, host->h_length);
        
        printf("[C Client] Connecting to %s:%d...\n", server_host, server_port);
        fflush(stdout);
        
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == 0) {
            printf("[C Client] Connected successfully!\n");
            fflush(stdout);
            
            send(sock, "GET_POINTS", 10, 0);
            
            char buffer[BUFFER_SIZE];
            memset(buffer, 0, BUFFER_SIZE);
            int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                printf("[C Client] Response: %s\n", buffer);
                fflush(stdout);
            }
            close(sock);
        } else {
            printf("[C Client] Connection failed: %s\n", strerror(errno));
            fflush(stdout);
            close(sock);
        }
        
        printf("[C Client] Waiting 10 seconds...\n");
        printf("----------------------------------------\n");
        fflush(stdout);
        sleep(10);
    }
    return 0;
}
