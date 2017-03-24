/*
 * Copyright (c) 2016 Jaime Pérez <jaimep@stud.ntnu.no>, NTNU
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of NTNU nor the names of its contributors may be used
 *       to endorse or promote products derived from this software without
 *       specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL JAIME PEREZ OR NTNU BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#include <unistd.h>
#endif

#include "timing.h"
#include "../src/ishake.h"
#include "../src/utils.h"


/*
 * Try to get the number of available cores.
 */
long getNumberOfCores(void) {
    long nprocs = -1;
#ifdef _WIN32
#ifndef _SC_NPROCESSORS_ONLN
    SYSTEM_INFO info;
    GetSystemInfo(&info);
#define sysconf(a) info.dwNumberOfProcessors
#define _SC_NPROCESSORS_ONLN
#endif
#endif
#ifdef _SC_NPROCESSORS_ONLN
    nprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (nprocs < 1) {
        fprintf(stderr, "Could not determine number of CPUs online:\n%s\n",
                strerror (errno));
    }
    return nprocs;
#else
    fprintf(stderr, "Could not determine number of CPUs");
    return nprocs;
#endif
}


/*
 * Hash some data n times, while measuring the time it takes each time, and
 * return the minimum time observed.
 */
uint64_t measureHash(
        ishake_t *is,
        uint64_t dtMin,
        unsigned char *data,
        unsigned char *hash,
        uint16_t hashbitlen,
        uint16_t blkno,
        uint8_t thrno
) {
    measureTimingBegin
    if (ishake_init(
            is,
            (uint32_t) ISHAKE_BLOCK_SIZE,
            hashbitlen,
            ISHAKE_APPEND_ONLY_MODE,
            thrno
    )) {
        ishake_cleanup(is);
        fprintf(stderr, "ishake_init() failed\n");
        return 0;
    }

    for (int i = 0; i < blkno; i++) {
        if (ishake_append(is, data, ISHAKE_BLOCK_SIZE)) {
            ishake_cleanup(is);
            fprintf(stderr, "ishake_append() failed\n");
            return 0;
        }
    }

    if (ishake_final(is, hash)) {
        ishake_cleanup(is);
        fprintf(stderr, "ishake_final() failed\n");
        return 0;
    }
    measureTimingEnd
    ishake_cleanup(is);
    return tMin;
}


/*
 * Print help on how to use this program and exit.
 */
void usage(char *program) {
    printf("Usage:\t%s [--threads N]\n\n", program);
    printf("\t--threads\tThe number of threads to use. The amount of threads "
                   "used by default depends on the number of logical cores.\n");
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


int main(int argc, char *argv[]) {
    unsigned char *data, *hash;
    char *hex;
    uint64_t time;
    uint16_t hashbitlen = 2688;
    uint64_t calibration = calibrate();
    ishake_t *is;

    long thrno = getNumberOfCores();
    thrno--;
    if (thrno < 0) {
        thrno = 0;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp("--threads", argv[i]) == 0) {
            if (i == argc - 1) {
                panic(argv[0], "--threads must be followed by the amount of "
                        "threads to use.", 0);
            }
            thrno = atoi(argv[i + 1]);
            i++; // two arguments consumed, advance the pointer!
        }
    }

    data = calloc(ISHAKE_BLOCK_SIZE, sizeof(uint8_t));
    hash = malloc(hashbitlen/sizeof(uint8_t));

    printf("Block size: %d bytes, using %lu threads.\n", ISHAKE_BLOCK_SIZE,
           thrno);
    for (int i = 0; i < 10; i++) {
        is = calloc(1, sizeof(ishake_t));
        time = measureHash(is, calibration, data, hash, hashbitlen, 1024, thrno);
        printf("%10d bytes, %10llu cycles, %6.3f cycles/byte\n",
               ISHAKE_BLOCK_SIZE * 1024, time,
               time * 1.0 / (ISHAKE_BLOCK_SIZE * 1024));
    }
    printf("\n\n");

    bin2hex(&hex, hash, (unsigned long)hashbitlen / 8);
    printf("%s\n", hex);

    free(data);
    free(hash);
    free(hex);
}
