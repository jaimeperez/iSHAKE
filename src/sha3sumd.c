#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../lib/libkeccak-tiny/keccak-tiny.h"
#include "utils.h"

#define BLOCK_SIZE 1024

/*
 * Print help on how to use this program and exit.
 */
void usage(char *program) {
    printf("Usage:\t%s [--128|--256] [--bits N] [--block-size N] "
                   "[--quiet] [--help] [dir]\n\n",
           program);
    printf("\t--shake128\t=2Use 128 bit SHAKE.\n");
    printf("\t--shake256\tUse 256 bit SHAKE.\n");
    printf("\t--sha3-224\tUse 224 bit SHA3.\n");
    printf("\t--sha3-256\tUse 256 bit SHA3. Default\n");
    printf("\t--sha3-384\tUse 384 bit SHA3.\n");
    printf("\t--sha3-512\tUse 512 bit SHA3.\n");
    printf("\t--bytes\t\tThe number of bytes desired in the output. Must be a "
                   "multiple of 8.\n");
    printf("\t--quiet\t\tOutput only the resulting hash string.\n");
    printf("\t--help\t\tPrint this help.\n");
    printf("\tdir\t\tThe path to a directory whose contents will be hashed in "
                   "order. Every file in the directory will be read and "
                   "incorporated into the input, in the same order as they "
                   "appear in the directory.\n");
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

int main(int argc, char *argv[]) {
    DIR *dfd;
    FILE **files;
    struct dirent *dp;
    uint8_t fno = 0;

    uint8_t *out;
    char *output;

    int shake = 0, sha = 0, quiet = 0;
    unsigned long bytes = 0;
    char *dirname = "";

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
        } else if (strcmp("--quiet", argv[i]) == 0) {
            quiet = 1;
        } else if (strcmp("--bytes", argv[i]) == 0) {
            char **invalid;
            invalid = malloc(sizeof(char **));
            bytes = strtoul(argv[i + 1], invalid, 10);
            if (bytes == 0 && argv[i + 1] == *invalid) {
                printf("--bytes must be followed by the amount of bytes "
                               "desired as output.");
                free(invalid);
                return -1;
            }
            i++;
            free(invalid);
            continue;
        } else if (strcmp("--help", argv[i]) == 0) {
            usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else {
            if (strlen(argv[i]) > 2) {
                if ((argv[i][0] == '-') && (argv[i][1] == '-')) {
                    printf("Unknown option '%s'\n", argv[i]);
                    return -1;
                }
            }
            if (strlen(dirname) > 1) {
                printf("Cannot specify more than one directory.\n");
                return -1;
            }
            dirname = argv[i];
        }
    }

    // open directory
    if ((dfd = opendir(dirname)) == NULL) {
        panic(argv[0], "cannot find directory '%s' or read access denied.",
              1, dirname);
    }


    // build array of files
    while ((dp = readdir(dfd)) != NULL) {
        if (dp->d_name[0] == '.') {
            continue;
        }
        // resolve path: dirname + '/' + dp->d_name + '\0'
        char *file = malloc(strlen(dirname) + 1 + strlen(dp->d_name) + 1);
        snprintf(file, strlen(dirname) + 1 + strlen(dp->d_name) + 1,
                 "%s/%s", dirname, dp->d_name);

        // add file to list
        files = realloc(files, fno + 1);
        files[fno] = fopen(file, "r");
        fno++;
    }

    if (shake != 0) { // we are asked for a shake hash (extensible output)
        switch (shake) {
            case 128:
                if (bytes == 0) {
                    bytes = 32;
                }
                out = malloc(bytes);
                shakef128(out, bytes, files, fno);
                break;
            default:
                if (bytes == 0) {
                    bytes = 64;
                }
                out = malloc(bytes);
                shakef256(out, bytes, files, fno);
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
                sha3f_224(out, 28, files, fno);
                break;
            case 384:
                bytes = 48;
                out = malloc(bytes);
                sha3f_384(out, 48, files, fno);
                break;
            case 512:
                bytes = 64;
                out = malloc(bytes);
                sha3f_512(out, 64, files, fno);
                break;
            default:
                bytes = 32;
                out = malloc(bytes);
                sha3f_256(out, 32, files, fno);
        }
    }

    // convert to hex and print
    bin2hex(&output, out, (int)bytes);
    if (quiet) {
        printf("%s\n", output);
    } else {
        printf("%s - %s\n", output, dirname);
    }

    // close files and free memory
    for (int i = 0; i < fno; i++) {
        fclose(files[i]);
    }
    closedir(dfd);
    free(out);
    free(output);
    return EXIT_SUCCESS;
}
