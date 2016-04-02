#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ishake.h"
#include "utils.h"

#define BLOCK_SIZE 32768

int main(int argc, char *argv[]) {
    FILE *fp;
    uint8_t *buf;
    uint8_t *bo;
    char *ho;

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

    // initialize ishake
    struct IShakeHash *is;
    is = malloc(sizeof(struct IShakeHash));
    if (ishake_init(is, BLOCK_SIZE, (uint16_t) bits)) {
        return -1;
    }

    // read input and process it on the go
    buf = malloc(BLOCK_SIZE);
    unsigned long b_read = fread(buf, 1, BLOCK_SIZE, fp);
    while (b_read == (unsigned long) BLOCK_SIZE) {
        blocks++;
        if (ishake_append(is, buf, BLOCK_SIZE)) return -1;
        b_read = fread(buf, 1, BLOCK_SIZE, fp);
    }

    // add the last leftovers of input
    if (ishake_append(is, buf, b_read)) return -1;

    // finish computations and get the hash
    bo = malloc(bits / 8);
    ishake_final(is, bo);

    // convert to hex and print
    bin2hex(&ho, bo, bits / 8);
    printf("%s - %s\n", ho, filename);

    // clean
    ishake_cleanup(is);
    fclose(fp);
    free(buf);
    free(bo);
    free(ho);
    return 0;
}
