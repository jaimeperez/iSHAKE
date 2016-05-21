#include "modulo_arithmetics.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

/*
 * Print help on how to use this program and exit.
 */
void usage(char *program) {
    printf("Usage:\t%s [--add|--sub] hash1 hash2\n\n",
           program);
    printf("\t--add\t\tApply the addition operation. Default.\n");
    printf("\t--sub\t\tApply the subtraction operation.\n");
    printf("\t--help\t\tPrint this help.\n");
    printf("\thash1\t\tThe first operand to the operation requested.\n");
    printf("\thash2\t\tThe second operand to the operation requested.\n");
    exit(EXIT_SUCCESS);
}


/*
 * Write a message to stderr and exit.
 */
void panic(char *program, char *format, int argc, ...) {
    va_list valist;
    va_start(valist, argc);

    fprintf(stderr, "%s: ", program);
    if (argc > 0) {
        vfprintf(stderr, format, valist);
    } else {
        fprintf(stderr, "%s\n", format);
    }

    usage(program);
    exit(EXIT_FAILURE);
}

/*
 * Combine a and b using a group operation op, and store the result in a.
 */
void _combine(uint64_t *out, uint64_t *in, uint16_t len, group_op op) {
    for (int i = 0; i < len; i++) {
        out[i] = op(out[i], in[i], UINT64_MAX);
    }
}

int main(int argc, char **argv) {
    group_op op = add_mod;
    char *hash1 = NULL;
    char *hash2 = NULL;

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--sub", argv[i]) == 0) {
            op = sub_mod;
        } else if (strcmp("--help", argv[i]) == 0) {
            usage(argv[0]);
        } else {
            if (hash1 != NULL && hash2 != NULL) {
                panic(argv[0], "cannot combine more than two hashes.", 0);
            }
            if (hash1 != NULL) {
                hash2 = argv[i];
                continue;
            }
            hash1 = argv[i];
        }
    }

    if (strlen(hash1) != strlen(hash2)) {
        panic(argv[0], "both hashes must have the same length.", 0);
    }

    if (strlen(hash1) % 16 != 0) {
        panic(argv[0], "the length of the hashes must be multiple of 16.", 0);
    }

    // convert both hashes to binary data
    char *bin1 = malloc(strlen(hash1) / 2);
    char *bin2 = malloc(strlen(hash2) / 2);
    hex2bin(&bin1, (uint8_t *)hash1, strlen(hash1));
    hex2bin(&bin2, (uint8_t *)hash2, strlen(hash2));

    // cast both strings of binary data to arrays of 64-bit unsigned integers
    uint64_t *arr1 = malloc(sizeof(uint64_t) * (strlen(hash1) / 16));
    uint64_t *arr2 = malloc(sizeof(uint64_t) * (strlen(hash2) / 16));
    uint8_t2uint64_t(arr1, (uint8_t *)bin1, strlen(hash1) / 2);
    uint8_t2uint64_t(arr2, (uint8_t *)bin2, strlen(hash2) / 2);

    // combine both hashes
    _combine(arr1, arr2, (uint16_t )(strlen(hash1) / 16), op);

    // cast back to string of bytes
    uint8_t *bin = malloc(strlen(hash1) / 2);
    uint64_t2uint8_t(bin, arr1, strlen(hash1) / 16);

    // convert to hex and print
    char *out;
    bin2hex(&out, bin, strlen(hash1) / 2);
    printf("%s\n", out);

    // cleanup
    free(bin1);
    free(bin2);
    free(arr1);
    free(arr2);
    free(bin);
    free(out);

    return EXIT_SUCCESS;
}
