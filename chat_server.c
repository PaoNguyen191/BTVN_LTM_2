#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    int is_registered;
    char name[64];
} Client;

void get_current_time(char *time_str, size_t max_len) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(time_str, max_len, "%Y/%m/%d %I:%M:%S%p", tm_info);
}

int main() {
    int server_fd, new_socket, max_sd, activity, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    fd_set readfds;
    
    Client clients[MAX_CLIENTS];
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].is_registered = 0;
        memset(clients[i].name, 0, sizeof(clients[i].name));
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Chat Server dang chay tren port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                exit(EXIT_FAILURE);
            }

            printf("Connection moi: socket fd = %d, IP = %s, PORT = %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            char *welcome_msg = "Vui long nhap ten theo cu phap: client_id: client_name\n";
            send(new_socket, welcome_msg, strlen(welcome_msg), 0);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].is_registered = 0;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;

            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                memset(buffer, 0, BUFFER_SIZE);
                valread = read(sd, buffer, BUFFER_SIZE - 1);

                if (valread == 0) {
                    getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    printf("Client ngat ket noi: IP = %s, PORT = %d\n",
                           inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                    close(sd);
                    clients[i].fd = 0;
                    clients[i].is_registered = 0;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (clients[i].is_registered == 0) {
                        char temp_name[64];
                        
                        if (sscanf(buffer, "client_id: %63s", temp_name) == 1) {
                            clients[i].is_registered = 1;
                            strcpy(clients[i].name, temp_name);
                            
                            char success_msg[128];
                            snprintf(success_msg, sizeof(success_msg), "Dang ky thanh cong! Chao mung %s.\n", clients[i].name);
                            send(sd, success_msg, strlen(success_msg), 0);
                            printf("Client (fd: %d) da dang ky voi ten: %s\n", sd, clients[i].name);
                        } else {
                            char *err_msg = "Sai cu phap! Vui long gui lai theo dang: client_id: client_name\n";
                            send(sd, err_msg, strlen(err_msg), 0);
                        }
                    } else {
                        char time_str[64];
                        get_current_time(time_str, sizeof(time_str));

                        char send_buffer[BUFFER_SIZE + 256];
                        snprintf(send_buffer, sizeof(send_buffer), "%s %s: %s\n", 
                                 time_str, clients[i].name, buffer);

                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            int dest_sd = clients[j].fd;
                            if (dest_sd > 0 && clients[j].is_registered == 1 && j != i) {
                                send(dest_sd, send_buffer, strlen(send_buffer), 0);
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}