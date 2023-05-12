# Project Overview

Remote Procedure Call (RPC) is a crucial technology in distributed computing that enables software applications to communicate with each other seamlessly over a network. It provides a way for a client to call a function on a remote server as if it were a local function call. This abstraction allows developers to build distributed systems and applications that span multiple machines and platforms.

This project is a custom RPC system that allows computations to be split seamlessly between multiple computers. This system may differ from standard RPC systems, but the underlying principles of RPC will still apply.

# RPC System Architecture

The RPC system is implemented in two files, called rpc.c and rpc.h. The resulting system can be linked to either a client or a server.

# API

The basic API implemented by rpc.c consists of the following data structures and functions.

## Data structures

The API will send and receive data structures of the form:

```c
typedef struct {
int data1;
size_t data2_len;
void *data2;
} rpc_data;
```

where `data1` is an integer to be passed to the other side, and `data2` is a block of bytes (of length data2_len) to be sent. The purpose of `data1` is to allow simple functions that only pass an integer to avoid memory management issues, by setting `data2_len=0` and `data2=NULL`. Your protocol can limit `data1` to being no more than 64 bits.

Note that `size_t` depends on the architecture, and the sender and receiver can have different architectures.

The handler that implements the actual remote procedure will have the signature:

```c
rpc_data *procedure (rpc_data *d);
```

That is, it takes a pointer to an `rpc_data` object and returns a pointer to another `rpc_data` object. This function will dynamically allocate memory with malloc for both the `rpc_data` structure and its `data2` field. It is the responsibility of the RPC system to free those after use.

The state of the client and server are declared in rpc.h as:

```c
typedef struct rpc_client rpc_client;
typedef struct rpc_server rpc_server;
```

The actual struct definitions are in rpc.c. These are returned by initialization functions `rpc_init_client` and `rpc_init_server`, and passed to all other functions.

## Server-side API

```c
rpc_server *rpc_init_server (int port)
```

Called before `rpc_register`. It returns a pointer to a struct containing server state information on success and `NULL` on failure.

```c
int rpc_register (rpc_server *srv, const char *name, rpc_data* (*handler)(rpc_data*))
```

At the server, let the subsystem know what function to call when an incoming request is received.

It should return a non-negative number on success, and -1 on failure. If any of the arguments is NULL then -1 should be returned.

If there is already a function registered with name name, then the old function should be forgotten and the new
one should take its place.

```c
void rpc_serve_all (rpc_server *srv)
```

This function will wait for incoming requests for any of the registered functions, or `rpc_find`, on the port specified in `rpc_init_server` of any interface. If it is a function call request, it will call the requested function, send a reply to the caller, and resume waiting for new requests. If it is `rpc_find`, it will reply to the caller saying whether the name was found or not, or possibly an error code.

This function will not usually return.

It should only return if `srv` is `NULL` or you’re handling SIGINT.

## Client-side API

```c
rpc_client *rpc_init_client (const char *addr, int port)
```

Called before `rpc_find` or `rpc_call`. The string addr and integer port are the text-based IP address and numeric port number passed in on the command line.

The function should return a non-NULL pointer to a struct (that you define) containing client state information on success and `NULL` on failure.

```c
void rpc_close_client (rpc_client *cl)
```

Called after the final `rpc_call` or `rpc_find` (i.e., any use of the RPC system by the client).

If it is (mistakenly) called on a client that has already been closed, or `cl == NULL`, it should return without error.

```c
rpc_handle *rpc_find (rpc_client *cl, const char *name)
```

At the client, tell the subsystem what details are required to place a call. The return value is a handle (not handler) for the remote procedure, which is passed to the following function.

If name is not registered, it should return `NULL`. If any of the arguments are `NULL` then `NULL` should be returned. If the find operation fails, it returns `NULL`.

```c
rpc_data *rpc_call (rpc_client *cl, rpc_handle *h, const rpc_data *data)
```

This function causes the subsystem to run the remote procedure, and returns the value.

If the call fails, it returns `NULL`. `NULL` should be returned if any of the arguments are `NULL`. If this returns a non-NULL value, then it should dynamically allocate (by malloc) both the `rpc_data` structure and its `data2` field. The client will free these by `rpc_data_free` (defined below).

## Shared API

```c
void *rpc_data_free (rpc_data* data)
```

Frees the memory allocated for a dynamically allocated rpc_data struct.
