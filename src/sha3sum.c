#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "KeccakCodePackage.h"
#include "utils.h"

#define BLOCK_SIZE 1024
#define algorithm_Keccak 0

typedef struct {
    unsigned int algorithm;
    unsigned int rate;
    unsigned int capacity;
    unsigned int hashbitlen;
    unsigned char delimitedSuffix;
} Specifications;


int main(int argc, char *argv[]) {
    FILE *fp;
    uint8_t *buf;
    uint8_t *out;
    char *output;

    int shake = 0, sha = 0, hex_input = 0, quiet = 0;
    unsigned long bytes = 0;
    char *filename = "";

    Keccak_HashInstance keccak;
    Specifications specs;
    specs.algorithm = algorithm_Keccak;
    specs.rate = 1344;
    specs.capacity = 256;
    specs.hashbitlen = 264;
    specs.delimitedSuffix = 0x1F;

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--shake128", argv[i]) == 0) {
            specs.rate = 1344;
            specs.capacity = 256;
            specs.hashbitlen = 264;
            shake = 128;
            sha = 0;
        } else if (strcmp("--shake256", argv[i]) == 0) {
            specs.rate = 1088;
            specs.capacity = 512;
            specs.hashbitlen = 528;
            shake = 256;
            sha = 0;
        } else if (strcmp("--sha3-224", argv[i]) == 0) {
            specs.rate = 1152;
            specs.capacity = 448;
            specs.hashbitlen = 224;
            sha = 224;
            shake = 0;
        } else if (strcmp("--sha3-256", argv[i]) == 0) {
            specs.rate = 1088;
            specs.capacity = 512;
            specs.hashbitlen = 256;
            sha = 256;
            shake = 0;
        } else if (strcmp("--sha3-384", argv[i]) == 0) {
            specs.rate = 832;
            specs.capacity = 768;
            specs.hashbitlen = 384;
            sha = 384;
            shake = 0;
        } else if (strcmp("--sha3-512", argv[i]) == 0) {
            specs.rate = 576;
            specs.capacity = 1024;
            specs.hashbitlen = 512;
            sha = 512;
            shake = 0;
        } else if (strcmp("--hex", argv[i]) == 0) {
            hex_input = 1;
        } else if (strcmp("--quiet", argv[i]) == 0) {
            quiet = 1;
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
    if (bytes) {
        specs.hashbitlen = (unsigned int)bytes * 8;
    } else {
        bytes = specs.hashbitlen / 8;
    }

    if (shake != 0) { // we are asked for a shake hash (extensible output)
        switch (shake) {
            case 128:
                if (bytes == 0) {
                    bytes = 32;
                }
                out = malloc(bytes);
                break;
            default:
                if (bytes == 0) {
                    bytes = 64;
                }
                out = malloc(bytes);
        }
    } else { // we are asked for a SHA3 hash (fixed-length output)
        specs.delimitedSuffix = 0x06;
        if (bytes != 0) {
            printf("Cannot specify output bytes with fixed-size SHA3 "
                           "functions.\n");
            return -1;
        }
        switch (sha) {
            case 224:
                bytes = 28;
                out = malloc(bytes);
                break;
            case 384:
                bytes = 48;
                out = malloc(bytes);
                break;
            case 512:
                bytes = 64;
                out = malloc(bytes);
                break;
            default:
                bytes = 32;
                out = malloc(bytes);
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

    if (Keccak_HashInitialize(&keccak, specs.rate, specs.capacity,
                              specs.hashbitlen, specs.delimitedSuffix)) {
        return -1;
    }

    // read input in chunk, until finished or memory exhausted
    buf = malloc(BLOCK_SIZE);
    unsigned long b_read;

    do {
        b_read = fread(buf, 1, BLOCK_SIZE, fp);
        if (b_read > 0) {
            uint8_t *input = buf;
            if (hex_input) { // convert the input to bytes
                hex2bin((char **)&input, buf, b_read);
                b_read = b_read / 2;
            }
            Keccak_HashUpdate(&keccak, input, b_read * 8);
            if (hex_input) {
                free(input);
            }
        }
    } while(b_read > 0);

    Keccak_HashFinal(&keccak, out);

    // convert to hex and print
    bin2hex(&output, out, (int)bytes);
    if (quiet) {
        printf("%s\n", output);
    } else {
        printf("%s - %s\n", output, filename);
    }

    fclose(fp);
    free(buf);
    free(out);
    free(output);
    return 0;
}
