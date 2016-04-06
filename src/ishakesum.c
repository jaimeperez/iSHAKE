#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include "ishake.h"
#include "utils.h"

// default block size
#define BLOCK_SIZE 32768

int main(int argc, char *argv[]) {
    FILE *fp;
    uint8_t *buf;
    uint8_t *bo;
    char *ho;
    uint32_t block_size = BLOCK_SIZE;

    int shake = 0, blocks = 0, hex_input = 0, quiet = 0;
    unsigned long bits = 0;
    char *filename = "";

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--128", argv[i]) == 0) {
            shake = 128;
        } else if (strcmp("--256", argv[i]) == 0) {
            shake = 256;
        } else if (strcmp("--hex", argv[i]) == 0) {
            hex_input = 1;
        } else if (strcmp("--quiet", argv[i]) == 0) {
            quiet = 1;
        } else if (strcmp("--bits", argv[i]) == 0) {
            char *bits_str;
            bits = strtoul(argv[i + 1], &bits_str, 10);
            if (argv[i + 1] == bits_str) {
                printf("--bits must be followed by the amount of bits "
                               "desired as output.\n");
                return -1;
            }
            if (bits == 0) {
                printf("--bits can't be zero.\n");
                return -1;
            }
            if (bits % 64) {
                printf("--bits must be a multiple of 64.\n");
                return -1;
            }
            i++; // two arguments consumed, advance the pointer!
            continue;
        } else if (strcmp("--block-size", argv[i]) == 0) {
            char *block_str;
            block_size = (uint32_t)strtoul(argv[i + 1], &block_str, 10);
            if (argv[i + 1] == block_str) {
                printf("--block-size must be followed by the amount of bytes "
                               "desired as block size.\n");
                return -1;
            }
            if (block_size == 0) {
                printf("--block-size can't be zero.\n");
                return -1;
            }
            i++; // two arguments consumed, advance the pointer!
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

    /*
     * Obtain the input block size we should read. If input is hex,
     * hex_input == 1 and therefore the input block size will double. If not,
     * hex_input == 0 and the input will have the same size as hash blocks.
     */
    uint32_t input_block_size = block_size + (block_size * hex_input);

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
    if (ishake_init(is, block_size, (uint16_t) bits)) {
        return -1;
    }

    // read input and process it on the go
    buf = malloc(input_block_size);
    unsigned long b_read;

    do {
        blocks++;
        b_read = fread(buf, 1, input_block_size, fp);
        if (hex_input) {
            uint8_t *raw_data;
            hex2bin((char **)&raw_data, buf, b_read);
            if (ishake_append(is, raw_data, b_read / 2)) return -1;
            free(raw_data);
        } else {
            if (ishake_append(is, buf, b_read)) return -1;
        }
    } while (b_read == (unsigned long) input_block_size);

    // finish computations and get the hash
    bo = malloc(bits / 8);
    ishake_final(is, bo);

    // convert to hex and print
    bin2hex(&ho, bo, bits / 8);
    if (quiet) {
        printf("%s\n", ho);
    } else {
        printf("%s - %s\n", ho, filename);
    }

    // clean
    ishake_cleanup(is);
    fclose(fp);
    free(buf);
    free(bo);
    free(ho);
    return 0;
}
