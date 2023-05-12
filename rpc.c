#include "rpc.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include "./utils.h"

struct rpc_server {
    int socket_fd;
    int client_fd;
    int port;
    struct sockaddr_in addr;
    socklen_t addr_len;
    char **names;
    rpc_handler *handlers;
    int count_registered;
};

rpc_server *rpc_init_server(int port) {
    struct rpc_server *srv = malloc(sizeof(rpc_server));

    if (srv == NULL) {
        perror("malloc");
        return NULL;
    }

    // Create socket
    int socket_fd = -1;
    socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    // Bind socket
    struct sockaddr_in addr = {
        .sin_family = AF_INET6,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen on socket
    if (listen(socket_fd, 1) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Accept connection
    int client_fd = -1;
    client_fd = accept(socket_fd, NULL, NULL);
    if (client_fd == -1) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    // Initialize the name adn handler arrays
    srv->names = malloc(sizeof(char *) * 64);
    if (srv->names == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    srv->handlers = malloc(sizeof(rpc_handler) * 64);
    if (srv->handlers == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    srv->socket_fd = socket_fd;
    srv->client_fd = client_fd;
    srv->port = port;
    srv->addr = addr;
    srv->addr_len = sizeof(addr);
    srv->count_registered = 0;

    return srv;
}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    // Verify the name is not already registered
    if(verify_name(name) == -1) {
        return -1;
    }

    // register the name and handler
    srv->names[srv->count_registered] = name;
    srv->handlers[srv->count_registered] = handler;
    srv->count_registered++;

    // return 0 on success, -1 on failure
    return 0;
}

void rpc_serve_all(rpc_server *srv) {

}

struct rpc_client {
    int socket_fd;
    int port;
    struct sockaddr_in addr;
    socklen_t addr_len;
};

struct rpc_handle {
    /* Add variable(s) for handle */
};

rpc_client *rpc_init_client(char *addr, int port) {
    struct rpc_client *cl = malloc(sizeof(rpc_client));
    if (cl == NULL) {
        perror("malloc");
        return NULL;
    }

    // Create socket
    int socket_fd = -1;
    socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Connect to server
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET6,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(addr)
    };

    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    cl->socket_fd = socket_fd;
    cl->port = port;
    cl->addr = server_addr;
    cl->addr_len = sizeof(server_addr);
    
    return cl;
}

rpc_handle *rpc_find(rpc_client *cl, char *name) {
    return NULL;
}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {
    return NULL;
}

void rpc_close_client(rpc_client *cl) {

}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
