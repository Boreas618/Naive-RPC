#include "rpc.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    int exit_code = 0;

    rpc_client *state = rpc_init_client("::1", 3000);
    if (state == NULL) {
        exit(EXIT_FAILURE);
    }

    printf("finished init\n");

    rpc_handle *handle_echo2 = rpc_find(state, "add2");
    if (handle_echo2 == NULL) {
        fprintf(stderr, "ERROR: Function add2 does not exist\n");
        exit_code = 1;
        goto cleanup;
    }

    /* Prepare request */
    char left_operand = 127;
    rpc_data request_data = {
        .data1 = 127, .data2_len = 1, .data2 = &left_operand};

    rpc_data * response_data = rpc_call(state, handle_echo2, &request_data);
    
    if (response_data == NULL) {
        fprintf(stderr, "Function call of add2 failed\n");
        exit_code = 1;
        goto cleanup;
    }
    
    printf("%d\n", response_data->data1);
    //printf("%d", ((char *)response_data->data2)[0]);

    left_operand = -127;
    request_data.data1 = -127;
    response_data = rpc_call(state, handle_echo2, &request_data);

    printf("%d", response_data->data1);
    //printf("%d", ((char *)response_data->data2)[0]);

    rpc_data_free(response_data);

cleanup:

    rpc_close_client(state);
    state = NULL;

    return exit_code;
}
