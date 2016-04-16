#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ishake.h"
#include "utils.h"

// default block size
#define BLOCK_SIZE 32768


/*
 * Print help on how to use this program and exit.
 */
void usage(char *program) {
    printf("Usage:\t%s [--128|--256] [--bits N] [--block-size N] "
                   "[--quiet] [--help] [dir]\n\n",
           program);
    printf("\t--128\t\tUse 128 bit equivalent iSHAKE. Default.\n");
    printf("\t--256\t\tUse 256 bit equivalent iSHAKE.\n");
    printf("\t--bits\t\tThe number of bits desired in the output. Must be a "
                   "multiple of 64. Between 2688 and 4160 for iSHAKE 128, and "
                   "between 6528 and 16512 for iSHAKE 256. The lowest "
                   "number for each version is the default.\n");
    printf("\t--block-size\tThe size in bytes of the iSHAKE internal blocks."
                   "\n");
    printf("\t--quiet\t\tOutput only the resulting hash string.\n");
    printf("\t--help\t\tPrint this help.\n");
    printf("\tdir\t\tThe path to a directory whose contents will be hashed in "
                   "order. Every file in the directory will be read and "
                   "incorporated into the input, in the same order as they "
                   "appear in the directory.\n");

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


int main(int argc, char **argv) {
    struct dirent *dp;
    DIR *dfd;

    uint8_t *buf;
    uint8_t *bo;
    char *ho;
    uint32_t block_size = BLOCK_SIZE;

    int shake = 0, blocks = 0, quiet = 0, rehash = 0;
    unsigned long bits = 0;
    char *dirname = "", *oldhash;

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--128", argv[i]) == 0) {
            shake = 128;
        } else if (strcmp("--256", argv[i]) == 0) {
            shake = 256;
        } else if (strcmp("--quiet", argv[i]) == 0) {
            quiet = 1;
        } else if (strcmp("--bits", argv[i]) == 0) {
            char *bits_str;
            if (i == argc - 1) {
                panic(argv[0], "--bits must be followed by the amount of bits "
                        "desired as output.", 0);
            }
            bits = strtoul(argv[i + 1], &bits_str, 10);
            if (argv[i + 1] == bits_str) {
                panic(argv[0], "--bits must be followed by the amount of bits "
                        "desired as output.", 0);
            }
            if (bits == 0) {
                panic(argv[0], "--bits option can't be zero.", 0);
            }
            if (bits % 64) {
                panic(argv[0], "--bits must be a multiple of 64.", 0);
            }
            i++; // two arguments consumed, advance the pointer!
        } else if (strcmp("--block-size", argv[i]) == 0) {
            char *block_str;
            if (i == argc - 1) {
                panic(argv[0], "--block_size must be followed by the amount of "
                        "bytes desired as block size.", 0);
            }
            block_size = (uint32_t)strtoul(argv[i + 1], &block_str, 10);
            if (argv[i + 1] == block_str) {
                panic(argv[0], "--block-size must be followed by the amount of "
                        "bytes desired as block size.", 0);
            }
            if (block_size == 0) {
                panic(argv[0], "--block-size can't be zero.", 0);
            }
            i++; // two arguments consumed, advance the pointer!
        } else if (strcmp("--rehash", argv[i]) == 0) {
            rehash = 1;
            if (i == argc - 1) {
                panic(argv[0], "--rehash must be followed by the old hash to "
                        "use as base for the computation, hex-encoded.", 0);
            }
            oldhash = argv[i + 1];
            i++; // two arguments consumed, advance the pointer!
        } else if (strcmp("--help", argv[i]) == 0) {
            usage(argv[0]);
        } else {
            if (strlen(argv[i]) > 2) {
                if ((argv[i][0] == '-') && (argv[i][1] == '-')) {
                    panic(argv[0], "unknown option '%s'\n", 1, argv[i]);
                }
            }
            if (strlen(dirname) > 1) {
                panic(argv[0], "cannot specify more than one directory.", 0);
            }
            dirname = argv[i];
        }
    }

    // validate output bits and algorithm version
    switch (shake) {
        case 256:
            if (bits == 0) {
                bits = 6528;
            } else {
                if (bits < 6528 || bits > 16512) {
                    panic(argv[0], "--bits must be between 6528 and 16512 for "
                            "iSHAKE128.", 0);
                }
            }
            break;
        case 128:
            if (bits && (bits <  2688 || bits > 4160)) {
                panic(argv[0], "--bits must be between 2688 and 4160 for "
                        "iSHAKE128.", 0);
            }
        default:
            if (bits == 0) {
                bits = 2688;
            } else {
                if (bits <  2688 || bits > 4160) {
                    panic(argv[0], "--bits must be between 2688 and 4160 for "
                            "iSHAKE128.", 0);
                }
            }
    }

    // verify the length of the old hash if we are rehashing
    if (rehash) {
        if (bits / 8 != strlen(oldhash)) {
            panic(argv[0], "the length of the old hash does not match with "
                    "the requested amount of bits.", 0);
        }
    }

    // initialize ishake
    ishake *is;
    is = malloc(sizeof(ishake));
    if (ishake_init(is, block_size, (uint16_t) bits, ISHAKE_APPEND_ONLY_MODE)) {
        panic(argv[0], "cannot initialize iSHAKE.", 0);
    }

    // open directory
    if ((dfd = opendir(dirname)) == NULL) {
        panic(argv[0], "cannot find directory '%s' or read access denied.",
              1, dirname);
    }

    // iterate over list of files in directory
    char *file;
    while ((dp = readdir(dfd)) != NULL) {
        if (dp->d_name[0] == '.') {
            continue;
        }
        file = malloc(strlen(dirname) + strlen(dp->d_name) + 1);
        struct stat stbuf;
        sprintf(file, "%s/%s", dirname, dp->d_name);
        if (stat(file, &stbuf) == -1) {
            printf("Unable to stat file: %s\n", file);
            continue;
        }

        if (access(file, R_OK) == -1) {
            panic(argv[0], "cannot find file '%s' or read access denied.",
                  1, file);
        }

        // read file and process it on the go
        FILE *fp = fopen(file, "r");
        buf = malloc(block_size);
        unsigned long b_read;

        do {
            blocks++;
            b_read = fread(buf, 1, block_size, fp);
            if (ishake_append(is, buf, b_read)) {
                panic(argv[0], "iSHAKE failed to process data.", 0);
            }
        } while (b_read == (unsigned long) block_size);

        free(buf);
        fclose(fp);
        free(file);
    }

    // finish computations and get the hash
    bo = malloc(bits / 8);
    if (ishake_final(is, bo)) {
        panic(argv[0], "cannot compute hash after processing data.", 0);
    }

    // convert to hex and print
    bin2hex(&ho, bo, bits / 8);
    if (quiet) {
        printf("%s\n", ho);
    } else {
        printf("%s - %s\n", ho, dirname);
    }

    // clean
    ishake_cleanup(is);
    closedir(dfd);
    free(bo);
    free(ho);
    return EXIT_SUCCESS;
}
