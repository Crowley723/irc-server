#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <regex.h>

#define RED "\e[0;31m"
#define NC "\e[0m"

#define MAX_CLIENTS 10
#define MAX_BUFFER 1024
#define PORT 2000
#define MAX_USERNAME 20
#define SERVER_FD 0

volatile sig_atomic_t running = 1;

void handle_signal(int sig) {
    running = 0;
}

enum ConnectState {
    SERVER = -1, // the SERVER state is reserved for the server
    IDLE = 0, // IDLE means normal operation, server will handle incoming messages as commands or messages.
    WAITING_FOR_USERNAME = 1 // WAITING_FOR_USERNAME means the server is still waiting for a valid username from the client, will interpret incoming messages as the user's desired username
};

struct client {
    struct pollfd pfd;
    struct sockaddr_in address;
    enum ConnectState state;
    char* username;
};

bool validateUsername(const char* username) {
    regex_t regex;
    const int value = regcomp(&regex, "^[[:alnum:]]{1,10}$", REG_EXTENDED);

    if (value != 0) {
        return false;
    }

    const int match_result = regexec(&regex, username, 0, NULL, 0);
    regfree(&regex);

    return (match_result == 0);

}

void broadcast(const int num_fds, const struct client* clients, const int sender_index, const char* buffer) {
    char message[MAX_BUFFER];
    snprintf(message, sizeof(message), "%s: %s\n",clients[sender_index].username, buffer);

    for (int i = 1; i < num_fds; i++) {  // Start from 1 to skip server
        if (clients[i].pfd.fd != clients[sender_index].pfd.fd || sender_index == -1) {  // Don't send back to sender
            send(clients[i].pfd.fd, message, strlen(message), 0);
        }
    }
}

void directSend(const struct client* clients, const int sender_index, const int recipient_index, const char* buffer) {
    char message[MAX_BUFFER];
    snprintf(message, sizeof(message), "%s to you: %s\n", clients[sender_index].username, buffer);

    send(clients[recipient_index].pfd.fd, message, strlen(message), 0);
}

void serverSend(const struct client* clients, const int recipient_index, const char* buffer) {
    char message[MAX_BUFFER];
    snprintf(message, sizeof(message), "Server to you: %s", buffer);

    send(clients[recipient_index].pfd.fd, message, strlen(message), 0);
}

char** parseCommand(const char* input, int expected_parts) {
    char** result = malloc(sizeof(char*) * expected_parts);
    if (!result) return NULL;

    char* str = strdup(input);
    if (!str) {
        free(result);
        return NULL;
    }

    char* token = strtok(str, " ");
    for (int i = 0; i < expected_parts - 1; i++) {
        if (token) {
            result[i] = strdup(token);
            token = strtok(NULL, " ");
        } else {
            for (int j = 0; j < i; j++) {
                free(result[j]);
            }
            free(result);
            free(str);
            return NULL;
        }

    }
    result[expected_parts - 1] = token ? strdup(token) : strdup("");

    free(str);
    return result;
}

int findUserByUsername(const struct client* clients, const char* username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        printf("Username Check: %s == %s\n", username, clients[i].username);
        if (clients[i].username != NULL && strcmp(clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

char* listUsers(const struct client* clients) {
    char* users = malloc(MAX_CLIENTS * (MAX_USERNAME + 2));  // +2 for ", " between names
    users[0] = '\0';  // Initialize empty string

    int first = 1;
    for (int i = 1; i < MAX_CLIENTS + 1; i++) {  // Start from 1 to skip server
        if (clients[i].username != NULL) {
            if (!first) {
                strcat(users, ", ");
            }
            strcat(users, clients[i].username);
            first = 0;
        }
    }

    return users;
}

int main(void) {
    struct sigaction sa = {
        .sa_handler = handle_signal,
        .sa_flags = 0
    };
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Failed to create socket");
        return 1;
    }

    // Set SO_REUSEADDR option
    const int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Failed to set socket options");
        close(server_fd);
        return 1;
    }


    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };

    if (bind(server_fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("Failed to bind socket");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) == -1) {
        perror("Failed to listen on socket");
        close(server_fd);
        return 1;
    }

    struct client clients[MAX_CLIENTS + 1] = {
        [0] = {
            .pfd = {
                .fd = server_fd,
                .events = POLLIN,
                .revents = -1
            },
            .username = "Server",
            .state = SERVER,
        }
    };

    for (int i = 1; i <= MAX_CLIENTS; i++) {
        clients[i].username = NULL;
    }

    char buffer[MAX_BUFFER];
    int num_fds = 1;

    printf("Server listening on port %d...\n", PORT);

    while (running) {
        struct pollfd fds[MAX_CLIENTS + 1];
        for (int i = 0; i < num_fds; i++) {
            fds[i] = clients[i].pfd;
        }
        poll(fds, num_fds, -1);

        for (int i = 0; i < num_fds; i++) {
            clients[i].pfd.revents = fds[i].revents;
        }

        if (clients[0].pfd.revents & POLLIN) {
            if (num_fds <= MAX_CLIENTS) {
                socklen_t addr_len = sizeof(clients[num_fds].address);
                int new_fd = accept(server_fd, (struct sockaddr *) &clients[num_fds].address, &addr_len);

                if (new_fd == -1) {
                    perror("Failed to accept connection");
                    continue;
                }

                clients[num_fds].pfd.fd = new_fd;
                clients[num_fds].pfd.events = POLLIN;
                clients[num_fds].pfd.revents = 0;

                printf("New client connected at %s. Total clients: %d\n", inet_ntoa(clients[num_fds].address.sin_addr), num_fds);
                clients[num_fds].state = WAITING_FOR_USERNAME;
                serverSend(clients, num_fds, "Enter your username: ");
                num_fds++;
            } else {
                printf("Maximum clients reached. Connection rejected.\n");
                // Accept and immediately close the connection
                struct sockaddr_in temp_addr;
                socklen_t temp_len = sizeof(temp_addr);
                int temp_fd = accept(server_fd, (struct sockaddr *) &temp_addr, &temp_len);
                if (temp_fd != -1) {
                    close(temp_fd);
                }
            }
        }

        // Check for data on client sockets
        for (int i = 1; i < num_fds; i++) {
            // Start from 1 to skip server socket
            if (clients[i].pfd.revents & POLLIN) {
                ssize_t bytes_read = recv(clients[i].pfd.fd, buffer, sizeof(buffer) - 1, 0);


                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';

                    if (buffer[bytes_read-1] == '\n') {
                        buffer[bytes_read-1] = '\0';
                        bytes_read--;
                    }

                    printf("Received from client %d: %s\n", i, buffer);

                    if (buffer[0] == '\n' || buffer[0] == '\0') {

                    } else if (buffer[0] == '/') {
                        if (strcmp(buffer, "/list") == 0) {
                            printf("List command detected...\n");
                            char* users = listUsers(clients);
                            serverSend(clients, i, users);
                            free(users);
                        } else {
                            char* space = strchr(buffer, ' ');
                            if (space) {
                                size_t cmd_len = space - buffer;
                                if (strncmp(buffer, "/w", cmd_len - 1) == 0) {
                                    printf("Whisper command detected...\n");
                                    char** cmd_parts = parseCommand(buffer, 3);
                                    if (cmd_parts) {
                                        printf("Command Parts...");
                                        int recipient = findUserByUsername(clients, cmd_parts[1]);
                                        if (recipient != -1) {
                                            directSend(clients, i, recipient, cmd_parts[2]);
                                        }

                                        for (int j = 0; j < 3; j++) {
                                            free(cmd_parts[j]);
                                        }
                                        free(cmd_parts);
                                    }
                                }
                            }
                        }
                    }else if (clients[i].state == IDLE) {
                        broadcast(num_fds, clients, i, buffer);
                    } else if (clients[i].state == WAITING_FOR_USERNAME) {
                        if (validateUsername(buffer)) {
                            clients[i].state = IDLE;
                            clients[i].username = strdup(buffer);
                            char* message;
                            if (clients[i].username == NULL) {
                                fprintf(stderr, RED "[ERROR]" NC ": Error formatting string for user join message.\n");
                                shutdown(clients[i].pfd.fd, SHUT_RDWR);
                                close(clients[i].pfd.fd);
                                continue;
                            }

                            char join_message[MAX_BUFFER];
                            snprintf(join_message, sizeof(join_message), "%s has joined.",clients[i].username);

                            broadcast(num_fds, clients, 0, join_message);

                        } else {
                            serverSend(clients, i, "Usernames can only contain letters and numbers. \nEnter your username: ");
                        }
                    }
                } else if (bytes_read == 0 || (bytes_read == -1 && errno != EAGAIN)) {
                    // Client disconnected or error
                    printf("Client %d disconnected\n", i);
                    char message[MAX_BUFFER];
                    snprintf(message, sizeof(message), "%s has disconnected.", clients[i].username);

                    broadcast(num_fds, clients, 0, message);
                    close(clients[i].pfd.fd);

                    if (clients[i].username != NULL) {
                        free(clients[i].username);
                    }
                    memmove(&clients[i], &clients[i + 1],
                            (num_fds - i - 1) * sizeof(struct client));
                    num_fds--;
                    i--;
                }
            }
        }
    }
    broadcast(num_fds, clients, 0, "Server shutting down...");
    printf("Exiting...");
    for (int i = 0; i < num_fds; i++) {
        shutdown(clients[i].pfd.fd, SHUT_RDWR);
        close(clients[i].pfd.fd);
    }

    return 0;
}
