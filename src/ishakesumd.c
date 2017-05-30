#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "ishake.h"
#include "utils.h"

// default block size
#define BLOCK_SIZE 32768


/*
 * Print help on how to use this program and exit.
 */
void usage(char *program) {
    printf("Usage:\t%s [--128|--256] [--bits N] [--block-size N] [--mode M] "
                   "[--rehash H] [--threads N] [--quiet] [--help] [dir]\n\n",
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
    printf("\t--rehash\tThe hash to use as base, computing only those "
                   "blocks that have changed.\n");
    printf("\t--threads\tThe number of threads to use. No threads are used "
                   "by default.\n");
    printf("\t--profile\tMeasure the performance of the operation(s) to run"
                   ".\n");
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

    char *ho;
    char *dirname = "", *oldhash = NULL;

    uint64_t prev_nonce_f = 0;

    // file extensions with special meaning, should always be '.' + 3 bytes
    char *delext = ".del";
    char *oldext = ".old";
    char *newext = ".new";

    int shake = 0, blocks = 0, quiet = 0, rehash = 0, thrno = 0, profile = 0;
    unsigned long bits = 0;

    uint8_t *buf;
    uint8_t *bo;
    uint8_t mode = ISHAKE_APPEND_ONLY_MODE;
    uint32_t block_size = BLOCK_SIZE;
    uint32_t datalen;
    uint8_t headerlen = 8;

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
                headerlen = 16;
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
        } else if (strcmp("--threads", argv[i]) == 0) {
            if (i == argc - 1) {
                panic(argv[0], "--threads must be followed by the amount of "
                        "threads to use.", 0);
            }
            thrno = atoi(argv[i + 1]);
            i++; // two arguments consumed, advance the pointer!
        } else if (strcmp("--profile", argv[i]) == 0) {
            profile = 1;
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

    // set the max amount of data per block
    datalen = block_size - headerlen;

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
    ishake_t *is;
    is = malloc(sizeof(ishake_t));
    if (ishake_init(is, block_size, (uint16_t) bits, mode, (uint16_t)thrno)) {
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

    // start measuring performance
    clock_t start_cpu = 0, end_cpu = 0;
    struct timespec start_wall, end_wall;
    double elapsed_cpu = 0, elapsed_wall = 0;
    if (profile) {
        start_cpu = clock();
        clock_gettime(CLOCK_MONOTONIC, &start_wall);

    }

    // iterate over list of files in directory
    while ((dp = readdir(dfd)) != NULL) {
        char *file;
        if (dp->d_name[0] == '.' && rehash) { // dot file and we need to rehash
            size_t file_l = strlen(dp->d_name);
            size_t ext_l = strlen(oldext);

            if (ext_l >= file_l) {
                // this can't be a legitimate file, ignore
                continue;
            }

            if (mode == ISHAKE_FULL_MODE) {
                // FULL R&W mode, insert and delete available

                if (strcmp(dp->d_name + file_l - ext_l, delext) == 0) {
                    // we are asked to delete

                    // get the full path to the file we are deleting
                    char *delfile;
                    resolve_file_path(&delfile, dirname, dp->d_name);

                    // see if there are previous or next blocks assigned
                    char *firstdot, *lastdot, *nonce;
                    nonce = malloc(file_l - ext_l);
                    snprintf(nonce, file_l - ext_l,
                             "%s", dp->d_name + 1);
                    firstdot = strchr(nonce, '.');
                    lastdot = strrchr(nonce, '.');

                    // get the nonces for all blocks involved
                    char *prevnonce, *delnonce, *nextnonce;
                    prevnonce = malloc(firstdot - nonce + 1);
                    delnonce  = malloc(lastdot - firstdot);
                    nextnonce = malloc(strlen(nonce) - (lastdot - nonce));
                    snprintf(prevnonce, firstdot - nonce + 1, "%s", nonce);
                    snprintf(delnonce, lastdot - firstdot, "%s", firstdot + 1);
                    snprintf(nextnonce, strlen(nonce) - (lastdot - nonce),
                             "%s", lastdot + 1);

                    // convert the nonces to numeric
                    uint64_t prev_n = str2uint64_t(prevnonce, 10);
                    uint64_t del_n  = str2uint64_t(delnonce,  10);
                    uint64_t next_n = str2uint64_t(nextnonce, 10);

                    // check deleted file
                    if (access(delfile, R_OK) == -1) {
                        panic(argv[0], "cannot read file '%s'.", 1, delfile);
                    }

                    // see if we have a next block and build it
                    ishake_block_t *next_ptr = NULL;
                    if (next_n) {
                        // get the file name of the next block
                        char *nextfile;
                        resolve_file_path(&nextfile, dirname, nextnonce);

                        // now check previous file
                        if (access(nextfile, R_OK) == -1) {
                            panic(argv[0], "cannot read file '%s'.", 1,
                                  nextfile);
                        }

                        // recreate the next block
                        ishake_block_t *next_b = malloc(sizeof(ishake_block_t));
                        next_b->data_len = datalen;
                        next_b->header.length = headerlen;
                        next_b->header.value.nonce.nonce = prev_n;
                        next_b->header.value.nonce.prev = del_n;

                        // read its contents
                        FILE *next_fd = fopen(nextfile, "r");
                        next_b->data = malloc(datalen);
                        fread(next_b->data, 1, datalen, next_fd);
                        fclose(next_fd);
                        next_ptr = next_b;
                        free(nextfile);
                    }

                    // recreate deleted block
                    ishake_block_t *del_b = malloc(sizeof(ishake_block_t));
                    del_b->data_len = datalen;
                    del_b->header.length = headerlen;
                    del_b->header.value.nonce.nonce = del_n;
                    del_b->header.value.nonce.prev = prev_n;

                    // read its contents
                    FILE *del_fd = fopen(delfile, "r");
                    del_b->data = malloc(datalen);
                    fread(del_b->data, 1, datalen, del_fd);
                    fclose(del_fd);

                    ishake_delete(is, del_b, next_ptr);

                    free(prevnonce);
                    free(delnonce);
                    free(nextnonce);
                    free(nonce);
                    free(delfile);

                    continue;
                }

                if (strcmp(dp->d_name + file_l - ext_l, newext) == 0) {
                    // we are asked to insert

                    // get the full path to the file we are inserting
                    char *newfile;
                    resolve_file_path(&newfile, dirname, dp->d_name);

                    // see if there are previous or next blocks assigned
                    char *firstdot, *lastdot, *nonce;
                    nonce = malloc(file_l - ext_l);
                    snprintf(nonce, file_l - ext_l, "%s", dp->d_name + 1);
                    firstdot = strchr(nonce, '.');
                    lastdot = strrchr(nonce, '.');

                    // get the nonces for all blocks involved
                    char *prevnonce, *newnonce, *nextnonce;
                    prevnonce = malloc(firstdot - nonce + 1);
                    newnonce  = malloc(lastdot - firstdot);
                    nextnonce = malloc(strlen(nonce) - (lastdot - nonce));
                    snprintf(prevnonce, firstdot - nonce + 1, "%s", nonce);
                    snprintf(newnonce, lastdot - firstdot, "%s", firstdot + 1);
                    snprintf(nextnonce, strlen(nonce) - (lastdot - nonce), "%s",
                             lastdot + 1);

                    // convert the nonces to numeric
                    uint64_t prev_n = str2uint64_t(prevnonce, 10);
                    uint64_t new_n  = str2uint64_t(newnonce,  10);
                    uint64_t next_n = str2uint64_t(nextnonce, 10);

                    // check inserted file
                    if (access(newfile, R_OK) == -1) {
                        panic(argv[0], "cannot read file '%s'.", 1, newfile);
                    }

                    // see if we have a next block and build it
                    ishake_block_t *next_ptr = NULL;
                    if (next_n) {
                        // get the file name of the next block
                        char *nextfile;
                        resolve_file_path(&nextfile, dirname, nextnonce);

                        //  now check next file
                        if (access(nextfile, R_OK) == -1) {
                            panic(argv[0], "cannot read file '%s'.", 1,
                                  nextfile);
                        }

                        // modify next block
                        ishake_block_t *next_b = malloc(sizeof(ishake_block_t));
                        next_b->data_len = datalen;
                        next_b->header.length = headerlen;
                        next_b->header.value.nonce.nonce = next_n;
                        next_b->header.value.nonce.prev  = new_n;

                        // read its contents
                        FILE *next_fd = fopen(nextfile, "r");
                        next_b->data = malloc(datalen);
                        fread(next_b->data, 1, datalen, next_fd);
                        fclose(next_fd);
                        next_ptr = next_b;
                        free(nextfile);
                    }

                    // build new block
                    ishake_block_t *new_b = malloc(sizeof(ishake_block_t));
                    new_b->data_len = datalen;
                    new_b->header.length = headerlen;
                    new_b->header.value.nonce.nonce = new_n;
                    new_b->header.value.nonce.prev = prev_n;

                    // read its contents
                    FILE *new_fd = fopen(newfile, "r");
                    new_b->data = malloc(datalen);
                    fread(new_b->data, 1, datalen, new_fd);
                    fclose(new_fd);

                    ishake_insert(is, new_b, next_ptr);

                    free(nextnonce);
                    free(newnonce);
                    free(prevnonce);
                    free(nonce);
                    free(newfile);

                    continue;
                }
            }

            // we are asked to rehash, see if this is an old file
            if (strcmp(dp->d_name + file_l - ext_l, oldext) == 0) {
                // it is, get the corresponding file(s)
                unsigned long b_read;
                int i;

                char *old, *new;
                resolve_file_path(&old, dirname, dp->d_name);


                // verify the old file
                if (access(old, R_OK) == -1) {
                    panic(argv[0], "cannot read file '%s'.", 1, old);
                }


                // parse the name of the file into the index of the block
                uint64_t idx = str2uint64_t(dp->d_name + 1, 10);

                // verify the new file
                char *updated;
                char *ptr;
                ptr = strchr(dp->d_name + 1, '.');
                updated = malloc(ptr - dp->d_name);
                snprintf(updated, ptr - dp->d_name, "%s", dp->d_name + 1);

                resolve_file_path(&new, dirname, updated);
                free(updated);
                if (access(new, R_OK) == -1) {
                    panic(argv[0], "cannot read file '%s'.", 1, new);
                }

                // initialize old block
                ishake_block_t *oldblock = malloc(sizeof(ishake_block_t));
                uint64_t prev = 0;
                oldblock->data_len = 0;
                oldblock->data = NULL;
                if (mode == ISHAKE_APPEND_ONLY_MODE) {
                    oldblock->header.length = 8;
                    oldblock->header.value.idx = idx;
                } else { // FULL R&W, we need the next block
                    // parse the name of the file again
                    prev = str2uint64_t(dp->d_name + 1, 10);
                    oldblock->header.length = 16;
                    oldblock->header.value.nonce.nonce = idx;
                    oldblock->header.value.nonce.prev = prev;
                }

                // read the contents of the old file
                FILE *oldfp = fopen(old, "r");
                i = 0;
                do {
                    oldblock->data = realloc(oldblock->data,
                                             datalen + datalen * i);
                    b_read = fread(oldblock->data + datalen * i, 1,
                                   datalen, oldfp);
                    oldblock->data_len += b_read;
                    i++;
                } while (b_read == datalen);
                fclose(oldfp);

                // initialize new block
                ishake_block_t *newblock = malloc(sizeof(ishake_block_t));
                newblock->data_len = 0;
                newblock->data = NULL;
                if (mode == ISHAKE_APPEND_ONLY_MODE) {
                    newblock->header.length = 8;
                    newblock->header.value.idx = idx;
                } else { // FULL R&W, we need the next block
                    newblock->header.length = 16;
                    newblock->header.value.nonce.nonce = idx;
                    newblock->header.value.nonce.prev = prev;
                }

                // read the contents of the new block
                FILE *newfp = fopen(new, "r");
                i = 0;
                do {
                    newblock->data = realloc(newblock->data,
                                            datalen + datalen * i);
                    b_read = fread(newblock->data + datalen * i, 1,
                                   datalen, newfp);
                    newblock->data_len += b_read;
                    i++;
                } while (b_read == datalen);
                fclose(newfp);

                if (ishake_update(is, oldblock, newblock)) {
                    panic(argv[0], "iSHAKE failed to update block %s.", 1, new);
                }

                free(new);
                continue;
            }


            // we are asked to rehash, see if this is a new file
            if (strcmp(dp->d_name + file_l - ext_l, newext) == 0) {
                // it is, get the corresponding file(s)
                unsigned long b_read;

                char *new;
                resolve_file_path(&new, dirname, dp->d_name);

                // verify the new file
                if (access(new, R_OK) == -1) {
                    panic(argv[0], "cannot read file '%s'.", 1, new);
                }

                // parse the name of the file into the index of the block
                uint64_t idx = str2uint64_t(dp->d_name + 1, 10);

                // since we are appending, we need to tell iSHAKE that we
                // already have idx-1 blocks
                is->block_no = idx-1;

                // read the contents of the new file
                FILE *newfp = fopen(new, "r");
                buf = malloc(datalen);
                b_read = fread(buf, 1, datalen, newfp);
                if (ishake_append(is, buf, b_read)) {
                    panic(argv[0], "iSHAKE failed to process data.", 0);
                }
                free(buf);
                fclose(newfp);

                continue;
            }
        } else if (rehash) { // rehashing, but not a dot file
            continue;
        } else if (dp->d_name[0] == '.') { // not rehashing and a dot file
            continue;
        }

        // regular hash, no dot file

        if (mode == ISHAKE_FULL_MODE) { // full mode, new block, append it
            size_t namlen = strlen(dp->d_name);
            char *filename = malloc(namlen + 1);
            strncpy(filename, dp->d_name, namlen + 1);

            resolve_file_path(&file, dirname, filename);

            if (access(file, R_OK) == -1) {
                panic(argv[0], "cannot read file '%s'.", 1, file);
            }

            // parse the name of the file into the nonce of the block
            uint64_t nonce = str2uint64_t(filename, 10);

            // read file and process it on the go
            FILE *fp = fopen(file, "r");
            size_t b_read;

            // in FULL R&W mode, files size must be less or equal to block size
            ishake_block_t *block = malloc(sizeof(ishake_block_t));
            block->data = malloc(datalen);
            b_read = fread(block->data, 1, datalen, fp);
            block->data_len = (uint32_t) b_read;
            block->header.length = 16;
            block->header.value.nonce.prev = prev_nonce_f;
            block->header.value.nonce.nonce = nonce;

            // insert here
            ishake_insert(is, block, NULL);

            // record the last block processed to set the "prev" pointer of the
            // next block appended, if any
            prev_nonce_f = nonce;

            fclose(fp);
            free(file);
            free(filename);

            // we're done with this file
            continue;
        }

        // append-only mode

        resolve_file_path(&file, dirname, dp->d_name);

        if (access(file, R_OK) == -1) {
            panic(argv[0], "cannot read file '%s'.", 1, file);
        }

        // read file and process it on the go
        FILE *fp = fopen(file, "r");
        unsigned long b_read;

        do {
            blocks++;
            buf = malloc(datalen);
            b_read = fread(buf, 1, datalen, fp);
            if (b_read == 0) {
                // the file size is a multiple of the block size, we are done
                break;
            }

            if (ishake_append(is, buf, b_read)) {
                panic(argv[0], "iSHAKE failed to process data.", 0);
            }
        } while (b_read == datalen);

        free(buf);
        fclose(fp);
        free(file);
    }

    // finish computations and get the hash
    bo = malloc(bits / 8);
    if (ishake_final(is, bo)) {
        panic(argv[0], "cannot compute hash after processing data.", 0);
    }

    if (profile) {
        end_cpu = clock();
        clock_gettime(CLOCK_MONOTONIC, &end_wall);
        elapsed_cpu = ((double) (end_cpu - start_cpu)) / CLOCKS_PER_SEC;
        elapsed_wall = (end_wall.tv_sec - start_wall.tv_sec);
        elapsed_wall += (end_wall.tv_nsec - start_wall.tv_nsec) / 1000000000.0;
    }

    // convert to hex and print
    bin2hex(&ho, bo, bits / 8);
    if (quiet) {
        printf("%s\n", ho);
    } else {
        printf("%s - %s\n", ho, dirname);
    }

    if (profile) {
        printf("CPU time: %f\n", elapsed_cpu);
        printf("Wall time: %f\n", elapsed_wall);
    }

    // clean
    ishake_cleanup(is);
    closedir(dfd);
    free(bo);
    free(ho);
    return EXIT_SUCCESS;
}
