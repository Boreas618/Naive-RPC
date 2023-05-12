#include <stdio.h>

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