#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    int state;
} Client;

int check_auth(char *input) {
    FILE *fp = fopen("database.txt", "r");
    if (!fp) {
        return 0;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = 0;
        if (strcmp(input, line) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
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
        clients[i].state = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        exit(EXIT_FAILURE);
    }

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
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                exit(EXIT_FAILURE);
            }

            char *welcome_msg = "Please enter user and pass:\n";
            send(new_socket, welcome_msg, strlen(welcome_msg), 0);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].state = 0;
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
                    close(sd);
                    clients[i].fd = 0;
                    clients[i].state = 0;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (clients[i].state == 0) {
                        if (check_auth(buffer)) {
                            clients[i].state = 1;
                            char *success_msg = "Login successful. Enter command:\n";
                            send(sd, success_msg, strlen(success_msg), 0);
                        } else {
                            char *err_msg = "Login failed. Try again:\n";
                            send(sd, err_msg, strlen(err_msg), 0);
                        }
                    } else {
                        char cmd[BUFFER_SIZE + 128];
                        char *filename = "out.txt";
                        
                        snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buffer, filename);
                        system(cmd);

                        FILE *out_fp = fopen(filename, "r");
                        if (out_fp) {
                            char file_buf[BUFFER_SIZE];
                            size_t bytes_read;
                            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), out_fp)) > 0) {
                                send(sd, file_buf, bytes_read, 0);
                            }
                            fclose(out_fp);
                        } else {
                            char *err_exec = "Error retrieving command output.\n";
                            send(sd, err_exec, strlen(err_exec), 0);
                        }
                    }
                }
            }
        }
    }
    return 0;
}