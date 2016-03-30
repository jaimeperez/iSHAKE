#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ishake.h"
#include "utils.h"

#define BLOCK_SIZE 1024

int main(int argc, char *argv[]) {
    FILE *fp;
    uint8_t *buf;
    uint8_t *out;
    char *output;

    int shake = 0, blocks = 0;
    unsigned long bits = 0;
    char *filename = "";

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--128", argv[i]) == 0) {
            shake = 128;
        } else if (strcmp("--256", argv[i]) == 0) {
            shake = 256;
        } else if (strcmp("--bits", argv[i]) == 0) {
            char **invalid;
            invalid = malloc(sizeof(char **));
            bits = strtoul(argv[i + 1], invalid, 10);
            if (bits == 0 && argv[i + 1] == *invalid) {
                printf("--bits must be followed by the amount of bits "
                               "desired as output.\n");
                return -1;
            }
            if (bits % 64) {
                printf("--bits must be a multiple of 64.\n");
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

    // validate output bits and algorithm version
    switch (shake) {
        case 256:
            if (bits == 0) {
                bits = 6528;
            } else {
                if (bits < 6528 || bits > 16512) {
                    printf("--bits must be between 6528 and 16512 for "
                                   "iSHAKE128.\n");
                    return -1;
                }
            }
            break;
        case 128:
            if (bits && (bits <  2688 || bits > 4160)) {
                printf("--bits must be between 2688 and 4160 for iSHAKE128.\n");
                return -1;
            }
        default:
            if (bits == 0) {
                bits = 2688;
            } else {
                if (bits <  2688 || bits > 4160) {
                    printf("--bits must be between 2688 and 4160 for "
                                   "iSHAKE128.\n");
                    return -1;
                }
            }
    }

    // read input in chunk, until finished or memory exhausted
    buf = malloc(BLOCK_SIZE);
    unsigned long b_read = fread(buf + blocks * BLOCK_SIZE, 1, BLOCK_SIZE, fp);
    while (b_read == (unsigned long) BLOCK_SIZE) {
        blocks++;
        buf = realloc(buf, (size_t)blocks * BLOCK_SIZE + BLOCK_SIZE);
        if (buf == NULL) {
            printf("Input too large.\n");
            return -1;
        }
        b_read = fread(buf + blocks * BLOCK_SIZE, 1, BLOCK_SIZE, fp);
    }

    out = malloc(bits / 8);
    ishake_hash(buf, blocks * BLOCK_SIZE + b_read, out, (uint16_t) bits);

    // convert to hex and print
    bin2hex(&output, out, (int) bits / 8);
    printf("%s - %s\n", output, filename);

    fclose(fp);
    free(buf);
    free(out);
    free(output);
    return 0;
}