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

#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#ifndef _ISHAKE_PARTIAL_H
#define _ISHAKE_PARTIAL_H

#define _ISHAKE_BLOCK_SIZE pow(2, 15)

/*
 * Type definition for a function that obtains the hash of some data.
 */
typedef int (*hash_function)(uint8_t*, size_t, const uint8_t*, size_t);

struct IShakeHash {
    uint64_t block_no;
    uint32_t block_size;
    uint16_t output_len;
    uint64_t proc_bytes;
    uint32_t remaining;
    uint64_t *hash;
    unsigned char *buf;
    hash_function hash_func;

};

/**
 * Initialize a hash.
 */
int ishake_init(struct IShakeHash *is, uint32_t blk_size, uint16_t hashbitlen);

/**
 * Append data to be hashed. Its size doesn't need to be multiple of the block
 * size.
 */
int ishake_append(struct IShakeHash *is, unsigned char *data, uint64_t len);

/**
 * Update a block with new data. Old data must be provided too.
 */
int ishake_update(struct IShakeHash *is,
                  uint64_t blk_no,
                  unsigned char *old_data,
                  unsigned char *new_data);

/**
 * Finalise the process and get the hash result.
 */
int ishake_final(struct IShakeHash *is, uint8_t *output);

/**
 * Obtain the hash corresponding to some piece of data.
 */
int ishake_hash(unsigned char *data,
                uint64_t len,
                uint8_t *hash,
                uint16_t hashbitlen);

/**
 * Cleanup the resources attached to the passed iSHAKE structure.
 */
void ishake_cleanup(struct IShakeHash *is);

#endif // _ISHAKE_PARTIAL_H
