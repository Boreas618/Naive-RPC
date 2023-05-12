#include <stdio.h>
#include "./rpc.h"

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
void encode_data_find(char *name, char *buf) {
    // The first byte of the buffer is the size of the buffer
    // The seond byte of the buffer is 0, which indicates that this is a find request
    // The remaining bytes are the name of the function
    int count_bytes = 0;
    buf[1] = 0;
    count_bytes += 2;
    memcpy(buf + count_bytes, name, strlen(name));
    count_bytes += strlen(name);
    buf[0] = count_bytes;
}

// Encode the call request into a sendable buffer
void encode_data_call(int index, rpc_data *data, char *buf) {
    // The first byte of the buffer is the size of the buffer
    // The second byte of the buffer is 1, which indicates that this is a call request
    // The third byte of the buffer is the index of the function
    // The next four bytes are the data1
    // The next four bytes are the length of the data2
    // The remaining bytes are the data2
    int count_bytes = 0;
    buf[1] = 1;
    buf[2] = index;
    count_bytes += 3;
    memcpy(buf + count_bytes, &data->data1, sizeof(int));
    count_bytes += sizeof(int);
    memcpy(buf + count_bytes, &data->data2_len, 4);
    count_bytes += 4;
    memcpy(buf + count_bytes, data->data2, data->data2_len);
    count_bytes += data->data2_len;
    buf[0] = count_bytes;
}