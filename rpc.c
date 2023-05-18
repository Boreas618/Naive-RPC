#include "rpc.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int verify_name(char *name);
void encode_data_find(char *name, int8_t *buf);
void encode_data_call(int index, rpc_data *data, int8_t *buf);
void encode_data_handle(int8_t index, int8_t *buf);
void encode_data_call_response(rpc_data *data, int8_t *buf);
int create_listening_socket(int port_num);
int open_clientfd(char *hostname, char *port);

struct rpc_handle {
    /* Add variable(s) for handle */
    int8_t index;
};

struct rpc_server {
    int socket_fd;
    int client_fd;
    int port;
    char *names[10];
    rpc_handler handlers[10];
    int count_registered;
};

rpc_server *rpc_init_server(int port) {
    struct rpc_server *srv = malloc(sizeof(rpc_server));

    if (srv == NULL) {
        perror("malloc");
        return NULL;
    }

    // Create listening socket
    int socket_fd = -1;
    socket_fd = create_listening_socket(port);
    if (socket_fd == -1) {
        perror("create_listening_socket");
        exit(EXIT_FAILURE);
    }

    srv->socket_fd = socket_fd;
    srv->client_fd = -1;
    srv->port = port;
    srv->count_registered = 0;

    return srv;
}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    // Verify the name is not already registered
    if (verify_name(name) == -1) {
        return -1;
    }

    // Allocate memory for name_copy
    char *name_copy = malloc(strlen(name) + 1);
    if (name_copy == NULL) {
        // Failed to allocate memory
        return -1;
    }

    // Copy the name to name_copy
    strcpy(name_copy, name);

    // register the name and handler
    srv->names[srv->count_registered] = name_copy;
    srv->handlers[srv->count_registered] = handler;
    srv->count_registered++;

    // return 0 on success, -1 on failure
    return 0;
}

void rpc_serve_all(rpc_server *srv) {
    // If the server is NULL, return
    if (srv == NULL) {
        return;
    }

    // Listen on socket
    if (listen(srv->socket_fd, 1) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept connection
    srv->client_fd = accept(srv->socket_fd, NULL, NULL);
    if (srv->client_fd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    int n = 0;
    int8_t buf[100050];
    // Keep processing the find and call requests
    while ((n = read(srv->client_fd, buf, sizeof(buf))) >= 0) {
        // Receive the buffer from the client
        // The size is the maximum size of a possible buffer

        // Decode the buffer
        int8_t type = buf[1];
        if (type == 0) {
            // Find request
            char name[1000];
            memcpy(name, buf + 2, buf[0] - 2);
            name[buf[0] - 2] = '\0';

            int index_of_handler = -1;
            for (int i = 0; i < srv->count_registered; i++) {
                if (strcmp(srv->names[i], name) == 0) {
                    index_of_handler = i;
                    break;
                }
            }

            if (index_of_handler == -1) {
                // If the handle is invalid, send -1 to the client
                encode_data_handle(-1, buf);
                if ((n = write(srv->client_fd, buf, 3)) < 0) {
                    perror("send");
                    return;
                }
            } else {
                // Else, send the encoded handle to the client
                encode_data_handle(index_of_handler, buf);
                if ((n = write(srv->client_fd, buf, 3)) < 0) {
                    perror("send");
                    return;
                }
            }

        } else if (type == 1) {
            // Call request
            int8_t index = buf[3];
            // Decode the data1
            uint8_t *data1_ptr = buf + 4;
            int data1 = (data1_ptr[3] << 24) | (data1_ptr[2] << 16) |
                        (data1_ptr[1] << 8) | (data1_ptr[0]);

            // Decode the length of data2
            int size_of_size_t = buf[2];
            size_t *data2_len_ptr = (size_t *)(buf + 4 + sizeof(int));
            size_t data2_len = *data2_len_ptr;

            // Decode data2
            void *data2;
            data2 = malloc(data2_len);
            memcpy(data2, buf + 4 + sizeof(int) + size_of_size_t, data2_len);

            rpc_data *data = malloc(sizeof(rpc_data));
            data->data1 = data1;
            data->data2_len = data2_len;
            data->data2 = data2;

            // Call the handler
            rpc_data *outcome = srv->handlers[index](data);
            if (outcome == NULL) {
                perror("rpc_call");
            } else {
                // Send the response to the client
                encode_data_call_response(outcome, buf);
                if ((n = write(srv->client_fd, buf, buf[0])) < 0) {
                    perror("send");
                    return;
                }
            }
        } else {
            // Invalid request
            return;
        }
    }
}

struct rpc_client {
    int socket_fd;
    struct sockaddr_in addr;
    socklen_t addr_len;
};

rpc_client *rpc_init_client(char *addr, int port) {
    struct rpc_client *cl = malloc(sizeof(rpc_client));
    if (cl == NULL) {
        perror("malloc");
        return NULL;
    }

    // Convert the port to a string
    char port_str[100];
    sprintf(port_str, "%d", port);

    // Create client socket
    int socket_fd = -1;
    socket_fd = open_clientfd(addr, port_str);
    if (socket_fd == -1) {
        perror("create_client_socket");
        exit(EXIT_FAILURE);
    }

    // Convert the addr into a sockaddr_in
    struct sockaddr_in addr_in;
    if (inet_pton(AF_INET6, addr, &addr_in.sin_addr) <= 0) {
        perror("inet_pton");
        exit(EXIT_FAILURE);
    }
    addr_in.sin_family = AF_INET6;
    addr_in.sin_port = htons(port);

    cl->socket_fd = socket_fd;
    cl->addr = addr_in;
    cl->addr_len = sizeof(addr_in);

    return cl;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    // If either argument is NULL, return NULL
    if (cl == NULL || name == NULL) {
        return NULL;
    }

    // Encode the find request into a sendable buffer
    int8_t buf[100050];
    encode_data_find(name, buf);

    int n = 0;

    if ((n = write(cl->socket_fd, buf, sizeof(name) + 2)) < 0) {
        perror("send");
        return NULL;
    }

    // Receive the encoded handle from the server and derive the index
    if ((n = read(cl->socket_fd, buf, sizeof(buf))) < 0) {
        perror("recv");
        return NULL;
    }

    int8_t index = buf[2];

    // If the handle is invalid, return NULL
    if (index == -1) {
        return NULL;
    }

    // Else, return the handle
    rpc_handle *handle = malloc(sizeof(rpc_handle));
    if (handle == NULL) {
        perror("malloc");
        return NULL;
    }
    handle->index = index;

    return handle;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    // If any argument is NULL, return NULL
    if (cl == NULL || h == NULL || payload == NULL) {
        return NULL;
    }

    // Encode the call request into a sendable buffer
    int8_t buf[100050];
    encode_data_call(h->index, payload, buf);

    int n = 0;

    // Send the request to the server
    if ((n = write(cl->socket_fd, buf,
                   1 + 1 + 1 + 1 + 4 + sizeof(size_t) + payload->data2_len)) <
        0) {
        perror("send");
        return NULL;
    }

    // Receive the response from the server
    if ((n = read(cl->socket_fd, buf, sizeof(buf))) < 0) {
        perror("recv");
        return NULL;
    }

    // Decode the response
    int8_t type = buf[1];

    if (type == 3) {
        // Decode the data1
        uint8_t *data1_ptr = (int *)(buf + 3);
        int data1 = (data1_ptr[3] << 24) | (data1_ptr[2] << 16) |
                    (data1_ptr[1] << 8) | (data1_ptr[0]);

        int size_of_size_t = buf[2];
        size_t *data2_len_ptr = (size_t *)(buf + 3 + sizeof(int));
        size_t data2_len = *data2_len_ptr;

        // Decode data2
        void *data2;
        data2 = malloc(data2_len);
        memcpy(data2, buf + 3 + sizeof(int) + size_of_size_t, data2_len);

        rpc_data *data = malloc(sizeof(rpc_data));

        data->data1 = data1;
        data->data2_len = data2_len;
        data->data2 = data2;

        return data;
    }

    return NULL;
}

void rpc_close_client(rpc_client *cl) {}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}

// Verify the name
int verify_name(char *name) {
    if (name == NULL) {
        return -1;
    }

    if (strlen(name) > 1000) {
        return -1;
    }

    for (int i = 0; i < strlen(name); i++) {
        if (name[i] < 32 || name[i] > 126) {
            return -1;
        }
    }

    return 0;
}

// Encode the find request into a sendable buffer
void encode_data_find(char *name, int8_t *buf) {
    // The first byte of the buffer is the size of the buffer
    // The seond byte of the buffer is 0, which indicates that this is a find
    // request The remaining bytes are the name of the function
    int count_bytes = 0;
    buf[1] = 0;
    count_bytes += 2;
    memcpy(buf + count_bytes, name, strlen(name));
    count_bytes += strlen(name);
    buf[0] = count_bytes;
}

// Encode the call request into a sendable buffer
void encode_data_call(int index, rpc_data *data, int8_t *buf) {
    // The first byte of the buffer is the size of the buffer
    // The second byte of the buffer is 1, which indicates that this is a call
    // request The third byte of the buffer is the size of the size_t The forth
    // byte of the buffer is the index of the function The next four bytes are
    // the data1 The next sizeof(size_t) bytes are the length of the data2 The
    // remaining bytes are the data2
    int count_bytes = 0;
    buf[1] = 1;
    buf[2] = sizeof(size_t);
    buf[3] = index;
    count_bytes += 4;
    memcpy(buf + count_bytes, &(data->data1), sizeof(int));
    count_bytes += sizeof(int);
    memcpy(buf + count_bytes, &data->data2_len, sizeof(size_t));
    count_bytes += sizeof(size_t);
    memcpy(buf + count_bytes, data->data2, data->data2_len);
    count_bytes += data->data2_len;
    buf[0] = count_bytes;
}

// Encode the response the handle into a sendable buffer
void encode_data_handle(int8_t index, int8_t *buf) {
    // The first byte of the buffer is the size of the buffer
    // The second byte of the buffer is 2, which indicates that this is a handle
    // response The third byte of the buffer is the index of the function
    int count_bytes = 0;
    buf[1] = 2;
    buf[2] = index;
    count_bytes += 3;
    buf[0] = count_bytes;
}

// Encode the response the call into a sendable buffer
void encode_data_call_response(rpc_data *data, int8_t *buf) {
    // The first byte of the buffer is the size of the buffer
    // The second byte of the buffer is the size of the size_t
    // The third byte of the buffer is 3, which indicates that this is a call
    // response The forth byte of the buffer is the data1 The next
    // sizeof(size_t) bytes are the length of the data2 The remaining bytes are
    // the data2
    int count_bytes = 0;
    buf[1] = 3;
    buf[2] = sizeof(size_t);
    count_bytes += 3;
    memcpy(buf + count_bytes, &(data->data1), sizeof(int));
    count_bytes += sizeof(int);
    memcpy(buf + count_bytes, &data->data2_len, sizeof(size_t));
    count_bytes += sizeof(size_t);
    memcpy(buf + count_bytes, data->data2, data->data2_len);
    count_bytes += data->data2_len;
    buf[0] = count_bytes;
}

int create_listening_socket(int port_num) {
    int re, s, sockfd;
    struct addrinfo hints, *res;

    // Create address we're going to listen on (with given port number)
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;      // IPv4
    hints.ai_socktype = SOCK_STREAM; // Connection-mode byte streams
    hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept

    // convert port number to string
    char service[100];
    sprintf(service, "%d", port_num);

    // node (NULL means any interface), service (port), hints, res
    s = getaddrinfo(NULL, service, &hints, &res);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    // Create socket
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Reuse port if possible
    re = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Bind address to the socket
    if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);

    return sockfd;
}

int open_clientfd(char *hostname, char *port) {
    int clientfd;
    struct addrinfo hints, *listp, *p;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_flags |= AI_ADDRCONFIG;

    getaddrinfo(hostname, port, &hints, &listp);

    for (p = listp; p; p = p->ai_next) {
        if ((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) <
            0)
            continue;
        if (connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
            break;
        close(clientfd);
    }

    freeaddrinfo(listp);
    if (!p)
        return -1;
    else
        return clientfd;
}