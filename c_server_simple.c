#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <mysql/mysql.h>
#include <json-c/json.h>
#include <time.h>

#define BUFFER_SIZE 1024

volatile int running = 1;
MYSQL *db_conn = NULL;
const char *user_name = "c_user";

void sigint_handler(int sig) {
    running = 0;
}

int connect_db() {
    db_conn = mysql_init(NULL);
    if (db_conn == NULL) {
        fprintf(stderr, "[C Server] mysql_init() failed\n");
        fflush(stderr);
        return 0;
    }
    
    const char *host = getenv("DB_HOST") ? getenv("DB_HOST") : "mysql-db";
    const char *user = getenv("DB_USER") ? getenv("DB_USER") : "itt440_user";
    const char *password = getenv("DB_PASSWORD") ? getenv("DB_PASSWORD") : "itt440_pass";
    const char *database = getenv("DB_NAME") ? getenv("DB_NAME") : "itt440_db";
    
    if (mysql_real_connect(db_conn, host, user, password, database, 0, NULL, 0) == NULL) {
        fprintf(stderr, "[C Server] MySQL connection error: %s\n", mysql_error(db_conn));
        fflush(stderr);
        return 0;
    }
    
    printf("[C Server] Connected to database: %s\n", host);
    fflush(stdout);
    return 1;
}

void* update_points_thread(void *arg) {
    while (running) {
        if (db_conn == NULL || mysql_ping(db_conn) != 0) {
            connect_db();
        }
        
        char update_query[256];
        snprintf(update_query, sizeof(update_query),
                "UPDATE user_points SET points = points + 1, datetime_stamp = CURRENT_TIMESTAMP WHERE user = '%s'",
                user_name);
        
        if (mysql_query(db_conn, update_query) == 0) {
            printf("[C Server %s] Updated points\n", user_name);
            fflush(stdout);
        } else {
            fprintf(stderr, "[C Server %s] Update error: %s\n", user_name, mysql_error(db_conn));
            fflush(stderr);
        }
        sleep(30);
    }
    return NULL;
}

void* handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("[C Server %s] Received: %s\n", user_name, buffer);
        fflush(stdout);
        
        if (strcmp(buffer, "GET_POINTS") == 0) {
            if (db_conn == NULL || mysql_ping(db_conn) != 0) {
                connect_db();
            }
            
            char query[256];
            snprintf(query, sizeof(query),
                    "SELECT user, points, datetime_stamp FROM user_points WHERE user = '%s'",
                    user_name);
            
            if (mysql_query(db_conn, query) == 0) {
                MYSQL_RES *result = mysql_store_result(db_conn);
                MYSQL_ROW row = mysql_fetch_row(result);
                
                if (row) {
                    struct json_object *json_obj = json_object_new_object();
                    json_object_object_add(json_obj, "user", json_object_new_string(row[0]));
                    json_object_object_add(json_obj, "points", json_object_new_int(atoi(row[1])));
                    json_object_object_add(json_obj, "datetime_stamp", json_object_new_string(row[2]));
                    
                    const char *json_str = json_object_to_json_string(json_obj);
                    send(client_socket, json_str, strlen(json_str), 0);
                    printf("[C Server %s] Sent response\n", user_name);
                    fflush(stdout);
                    json_object_put(json_obj);
                } else {
                    send(client_socket, "{\"error\":\"User not found\"}", 26, 0);
                }
                mysql_free_result(result);
            }
        } else {
            send(client_socket, "Invalid command", 15, 0);
        }
    }
    close(client_socket);
    return NULL;
}

int main() {
    signal(SIGINT, sigint_handler);
    
    user_name = getenv("USER_NAME") ? getenv("USER_NAME") : "c_user";
    int server_port = getenv("SERVER_PORT") ? atoi(getenv("SERVER_PORT")) : 5002;
    
    printf("[C Server %s] Starting on port %d\n", user_name, server_port);
    fflush(stdout);
    
    if (!connect_db()) {
        return 1;
    }
    
    pthread_t update_thread;
    pthread_create(&update_thread, NULL, update_points_thread, NULL);
    pthread_detach(update_thread);
    
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }
    
    if (listen(server_socket, 5) < 0) {
        perror("listen");
        return 1;
    }
    
    printf("[C Server %s] Listening on port %d\n", user_name, server_port);
    fflush(stdout);
    
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int *client_socket = malloc(sizeof(int));
        *client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        
        if (*client_socket < 0) {
            free(client_socket);
            continue;
        }
        
        printf("[C Server %s] Client connected\n", user_name);
        fflush(stdout);
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, client_socket);
        pthread_detach(client_thread);
    }
    
    close(server_socket);
    mysql_close(db_conn);
    return 0;
}
