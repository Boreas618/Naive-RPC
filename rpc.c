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
    // Receive the buffer from the client
    // The size is the maximum size of a possible buffer
    char buf[1+1+1+4+4+100000];

    // Read the buffer from the client
    int n = 0;
    if (n = read(srv->client_fd, buf, sizeof(buf)) < 0) {
        perror("recv");
        return;
    }

    // Decode the buffer
    int8_t type = buf[1];
    if (type == 0) {
        // Find request
        char name[1000];
        memcpy(name, buf+2, buf[0]-2);
        name[buf[0]-2] = '\0';
        rpc_handle *handle = rpc_find(srv, name);
        if (handle == NULL) {
            // If the handle is invalid, send -1 to the client
            int8_t index = -1;
            if (n = write(srv->client_fd, &index, sizeof(int8_t)) < 0) {
                perror("send");
                return;
            }
        } else {
            // Else, send the index to the client
            if (n = write(srv->client_fd, &handle->index, sizeof(int8_t)) < 0) {
                perror("send");
                return;
            }
        }
    } else if (type == 1) {
        // Call request
        int8_t index = buf[2];
        int data1 = buf[3];
        int data2_len = buf[4];
        char data2[data2_len];
        memcpy(data2, buf+5, data2_len);
        rpc_data *data = malloc(sizeof(rpc_data));
        data->data1 = data1;
        data->data2_len = data2_len;
        data->data2 = data2;
        rpc_data *outcome = rpc_call(srv, index, data);
        if (outcome == NULL) {
            perror("rpc_call");
        } else {
            // ToDo
        }
    } else {
        // Invalid request
        return;
    }
}

struct rpc_client {
    int socket_fd;
    int port;
    struct sockaddr_in addr;
    socklen_t addr_len;
};

struct rpc_handle {
    /* Add variable(s) for handle */
    int8_t index;
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
    // If either argument is NULL, return NULL
    if (cl == NULL || name == NULL) {
        return NULL;
    }

    // Encode the find request into a sendable buffer
    char buf[sizeof(name)+2];
    encode_data_find(name, buf);

    // Send the name to the server
    int n = 0;
    if (n = write(cl->socket_fd, buf, strlen(buf)) < 0) {
        perror("send");
        return NULL;
    }

    // Receive the uint8_t index from the server
    int8_t index = 0;
    if (n = read(cl->socket_fd, &index, sizeof(int8_t)) < 0) {
        perror("recv");
        return NULL;
    }
    
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
    char buf[1+1+1+4+4+payload->data2_len];
    encode_data_call(h->index, payload, buf);

    // Send the request to the server
    if (send(cl->socket_fd, buf, sizeof(buf), 0) == -1) {
        perror("send");
        return NULL;
    }

    // Receive the response from the server
    // ToDo

    // If the response is invalid, return NULL
    // Else, return the response
    if (response->data1 == -1) {
        return NULL;
    } else {
        return response;
    }
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
