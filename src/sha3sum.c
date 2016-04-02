#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../lib/libkeccak-tiny/keccak-tiny.h"
#include "utils.h"

#define BLOCK_SIZE 1024

int main(int argc, char *argv[]) {
    FILE *fp;
    uint8_t *buf;
    uint8_t *out;
    char *output;

    int shake = 0, sha = 0, blocks = 0, hex_input = 0;
    unsigned long bytes = 0;
    char *filename = "";

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--shake128", argv[i]) == 0) {
            shake = 128;
            sha = 0;
        } else if (strcmp("--shake256", argv[i]) == 0) {
            shake = 256;
            sha = 0;
        } else if (strcmp("--sha3-224", argv[i]) == 0) {
            sha = 224;
            shake = 0;
        } else if (strcmp("--sha3-256", argv[i]) == 0) {
            sha = 256;
            shake = 0;
        } else if (strcmp("--sha3-384", argv[i]) == 0) {
            sha = 384;
            shake = 0;
        } else if (strcmp("--sha3-512", argv[i]) == 0) {
            sha = 512;
            shake = 0;
        } else if (strcmp("--hex", argv[i]) == 0) {
            hex_input = 1;
        } else if (strcmp("--bytes", argv[i]) == 0) {
            char** invalid;
            invalid = malloc(sizeof(char**));
            bytes = strtoul(argv[i + 1], invalid, 10);
            if (bytes == 0 && argv[i + 1] == *invalid) {
                printf("--bytes must be followed by the amount of bytes "
                               "desired as output.");
                return -1;
            }
            i++;
            continue;
        } else {
            if (strlen(argv[i]) > 2) {
                if ((argv[i][0] == '-') && (argv[i][1] == '-')) {
                    printf("Unknown option '%s'\n", argv[i]);
                    return -1;
                }
            }
            if (strlen(filename) > 1) {
                printf("Cannot specify more than one file.\n");
                return -1;
            }
            filename = argv[i];
        }
    }

    // open appropriate input source
    if (strlen(filename) == 0) {
        fp = stdin;
    } else {
        if (access(filename, R_OK) == -1) {
            printf("Cannot find file '%s' or read access denied.\n", filename);
            return -1;
        }
        fp = fopen(filename, "r");
    }

    // read input in chunk, until finished or memory exhausted
    buf = malloc(BLOCK_SIZE);
    unsigned long b_read = fread(buf + blocks * BLOCK_SIZE, 1, BLOCK_SIZE, fp);
    while (b_read == BLOCK_SIZE) {
        blocks++;
        buf = realloc(buf, blocks * BLOCK_SIZE + BLOCK_SIZE);
        if (buf == NULL) {
            printf("Input too large.\n");
            return -1;
        }
        b_read = fread(buf + blocks * BLOCK_SIZE, 1, BLOCK_SIZE, fp);
    }

    if (hex_input) {
        uint8_t *input;
        // convert the input to bytes
        hex2bin((char **)&input, buf, blocks * BLOCK_SIZE + b_read);

        // make sure the algorithm uses half of the length read
        unsigned long l = (blocks * BLOCK_SIZE + b_read) / 2;
        b_read = l - blocks * BLOCK_SIZE;

        free(buf);
        buf = input;
    }

    if (shake != 0) { // we are asked for a shake hash (extensible output)
        switch (shake) {
            case 128:
                if (bytes == 0) {
                    bytes = 32;
                }
                out = malloc(bytes);
                shake128(out, bytes, buf, blocks * BLOCK_SIZE + b_read);
                break;
            default:
                if (bytes == 0) {
                    bytes = 64;
                }
                out = malloc(bytes);
                shake256(out, bytes, buf, blocks * BLOCK_SIZE + b_read);
        }
    } else { // we are asked for a SHA3 hash (fixed-length output)
        if (bytes != 0) {
            printf("Cannot specify output bytes with fixed-size SHA3 "
                           "functions.\n");
            return -1;
        }
        switch (sha) {
            case 224:
                bytes = 28;
                out = malloc(bytes);
                sha3_224(out, 28, buf, blocks * BLOCK_SIZE + b_read);
                break;
            case 384:
                bytes = 48;
                out = malloc(bytes);
                sha3_384(out, 48, buf, blocks * BLOCK_SIZE + b_read);
                break;
            case 512:
                bytes = 64;
                out = malloc(bytes);
                sha3_512(out, 64, buf, blocks * BLOCK_SIZE + b_read);
                break;
            default:
                bytes = 32;
                out = malloc(bytes);
                sha3_256(out, 32, buf, blocks * BLOCK_SIZE + b_read);
        }
    }

    // convert to hex and print
    bin2hex(&output, out, (int)bytes);
    printf("%s - %s\n", output, filename);

    fclose(fp);
    free(buf);
    free(out);
    free(output);
    return 0;
}
