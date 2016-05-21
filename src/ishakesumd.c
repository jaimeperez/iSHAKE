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
    printf("\t--mode\t\tThe mode of operation, one of FULL or APPEND_ONLY. "
                   "Defaults to APPEND_ONLY.\n");
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
    uint8_t mode = ISHAKE_APPEND_ONLY_MODE;
    char **files = NULL;
    uint32_t filesno = 0;

    int shake = 0, blocks = 0, quiet = 0, rehash = 0;
    unsigned long bits = 0;
    char *dirname = "", *oldhash;
    char *oldext = ".old";

    // parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp("--128", argv[i]) == 0) {
            shake = 128;
        } else if (strcmp("--256", argv[i]) == 0) {
            shake = 256;
        } else if (strcmp("--quiet", argv[i]) == 0) {
            quiet = 1;
        } else if (strcmp("--mode", argv[i]) == 0) {
            if (i == argc - 1) {
                panic(argv[0],
                      "--mode must be followed by FULL or APPEND_ONLY.", 0);
            }
            if (strcmp("FULL", argv[i + 1]) == 0) {
                mode = ISHAKE_FULL_MODE;
            }
            i++; // two arguments consumed, advance the pointer!
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
        if (bits != strlen(oldhash) * 4) {
            panic(argv[0], "the length of the old hash does not match with "
                    "the requested amount of bits.", 0);
        }
    }

    // initialize ishake
    ishake *is;
    is = malloc(sizeof(ishake));
    if (ishake_init(is, block_size, (uint16_t) bits, mode)) {
        panic(argv[0], "cannot initialize iSHAKE.", 0);
    }
    if (rehash) {
        // initialize hash
        uint8_t *bin = malloc(bits / 8);
        hex2bin((char **)&bin, (uint8_t *)oldhash, strlen(oldhash));
        uint8_t2uint64_t(is->hash, bin, bits / 8);
    }

    // open directory
    if ((dfd = opendir(dirname)) == NULL) {
        panic(argv[0], "cannot find directory '%s' or read access denied.",
              1, dirname);
    }

    // iterate over list of files in directory
    while ((dp = readdir(dfd)) != NULL) {
        char *file;
        if (dp->d_name[0] == '.' && rehash) { // dot file and we need to rehash
            size_t file_l = strlen(dp->d_name);
            size_t olde_l = strlen(oldext);

            if (olde_l >= file_l) {
                // this can't be a legitimate file, ignore
                continue;
            }

            // we are asked to rehash, see if this is an old file
            if (strcmp(dp->d_name + file_l - olde_l, oldext) == 0) {
                // it is, get the corresponding file(s)
                unsigned long b_read;
                int i;

                // dirname + '/' + dp->d_name + '\0'
                char *old = malloc(strlen(dirname) + 1 +
                                   strlen(dp->d_name) + 1);
                snprintf(old, strlen(dirname) + 1 + strlen(dp->d_name) + 1,
                         "%s/%s", dirname, dp->d_name);
                // dirname + '/' + dp->d_name - '.' - oldext + '\0'
                char *new = malloc(strlen(dirname) + file_l - olde_l + 1);
                snprintf(new, strlen(dirname) + file_l - olde_l + 1,
                         "%s/%s", dirname, dp->d_name + 1);

                // verify both new and old files
                if (access(old, R_OK) == -1) {
                    panic(argv[0], "cannot read file '%s'.", 1, old);
                }
                if (access(new, R_OK) == -1) {
                    panic(argv[0], "cannot read file '%s'.", 1, new);
                }

                // parse the name of the file into the index of the block
                char *ptr;
                uint64_t idx = strtoull(dp->d_name + 1, &ptr, 10);

                // initialize old block
                ishake_block oldblock;
                oldblock.block_size = 0;
                if (mode == ISHAKE_APPEND_ONLY_MODE) {
                    oldblock.header.length = 8;
                    oldblock.header.value.idx = idx;
                } else {
                    oldblock.header.length = 16;
                    oldblock.header.value.nonce.nonce = idx;
                }

                // read the contents of the old file
                FILE *oldfp = fopen(old, "r");
                i = 0;
                do {
                    oldblock.data = realloc(oldblock.data,
                                            block_size + block_size * i);
                    b_read = fread(oldblock.data + block_size * i, 1,
                                   block_size, oldfp);
                    oldblock.block_size += b_read;
                    i++;
                } while (b_read == (unsigned long)block_size);
                fclose(oldfp);

                // initialize new block
                ishake_block newblock;
                newblock.block_size = 0;
                newblock.header.length = 8;
                newblock.header.value.idx = idx;

                // read the contents of the new block
                FILE *newfp = fopen(new, "r");
                i = 0;
                do {
                    newblock.data = realloc(newblock.data,
                                            block_size + block_size * i);
                    b_read = fread(newblock.data + block_size * i, 1,
                                   block_size, newfp);
                    newblock.block_size += b_read;
                    i++;
                } while (b_read == (unsigned long)block_size);
                fclose(newfp);

                if (ishake_update(is, oldblock, newblock)) {
                    panic(argv[0], "iSHAKE failed to update block %s.", 1, new);
                }

                free(oldblock.data);
                free(newblock.data);
                free(new);
                continue;
            }
        } else if (rehash) { // rehashing, but not a dot file
            continue;
        } else if (dp->d_name[0] == '.') { // not rehashing and a dot file
            continue;
        }

        if (mode == ISHAKE_FULL_MODE) {
            // safe the file name to insert it in reverse order afterwards
            files = realloc(files, sizeof(char*) * (filesno + 1));
            int namlen = strlen(dp->d_name);
            char *filename = malloc(namlen + 1);
            strncpy(filename, dp->d_name, namlen + 1);
            files[filesno] = filename;
            filesno++;

            // we're done with this file, will process later
            continue;
        }

        // regular hash, no dot file
        file = malloc(strlen(dirname) + strlen(dp->d_name) + 1);
        sprintf(file, "%s/%s", dirname, dp->d_name);

        if (access(file, R_OK) == -1) {
            panic(argv[0], "cannot read file '%s'.", 1, file);
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

    if (mode == ISHAKE_FULL_MODE && !rehash) {
        int64_t i = filesno - 1;
        char *file;
        uint64_t next = 0;

        // iterate over the list of files in reverse order
        while (i >= 0) {
            file = malloc(strlen(dirname) + strlen(files[i]) + 1);
            sprintf(file, "%s/%s", dirname, files[i]);

            if (access(file, R_OK) == -1) {
                panic(argv[0], "cannot read file '%s'.", 1, file);
            }

            // parse the name of the file into the nonce of the block
            char *ptr;
            uint64_t nonce = strtoull(files[i], &ptr, 10);

            // read file and process it on the go
            FILE *fp = fopen(file, "r");
            size_t b_read;

            // in FULL R&W mode, files size must be less or equal to block size
            ishake_block block;
            block.data = malloc(block_size);
            b_read = fread(block.data, 1, block_size, fp);
            block.block_size = (uint32_t) b_read;
            block.header.length = 16;
            block.header.value.nonce.next = next;
            block.header.value.nonce.nonce = nonce;

            // insert here
            ishake_insert(is, NULL, block);

            // record the last block processed to set the "next" pointer
            next = nonce;

            fclose(fp);
            free(file);
            free(files[i]);
            i--;
        }
        free(files);
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
