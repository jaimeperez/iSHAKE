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

#include <string.h>
#include "ishake.h"
#include "../lib/libkeccak-tiny/keccak-tiny.h"

/*
 * Type definition for a function that performs our group operation.
 */
typedef uint64_t (*group_op)(uint64_t, uint64_t, uint64_t);

/*
 * Obtain the addition of a and b modulo m. Overflow safe.
 */
uint64_t add_mod(uint64_t a, uint64_t b, uint64_t m) {
    if (a <= (m - b)) {
        return a + b;
    }
    b = m - b;
    return (a >= b) ? a - b : m - b + a;
}

/*
 * Obtain the subtraction of a and b modulo m. Overflow safe.
 */
uint64_t sub_mod(uint64_t a, uint64_t b, uint64_t m) {
    if (a >= b) {
        return a - b;
    }
    return m - b + a;
}

/*
 * Compute the hash of a block.
 */
void _hash_block(
        uint8_t *block,
        uint32_t blen,
        uint64_t bidx,
        uint64_t *hash,
        uint16_t hlen,
        hash_function func
) {
    uint8_t *data;
    data = calloc(blen + sizeof(uint64_t), sizeof(uint8_t));
    memcpy(data, block, blen);

    // append the block index
    for (int i = 0; i < 8; i++) {
        *(data + blen + i) = (uint8_t) (bidx >> ((8 - i - 1) * 8));
    }

    uint8_t *buf;
    buf = calloc(hlen, sizeof(uint8_t));

    func(buf, hlen, data, blen + sizeof(uint64_t));
    free(data);

    // cast the resulting hash to (uint64_t *) for simplicity
    for (int i = 0; i < hlen / 8; i++) {
        *(hash + i)  = (uint64_t) *(buf + (8 * i)) << 56;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 1) << 48;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 2) << 40;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 3) << 32;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 4) << 24;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 5) << 16;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 6) << 8;
        *(hash + i) |= (uint64_t) *(buf + (8 * i) + 7);
    }
    free(buf);
}

/*
 * Combine a and b using a group operation op, and store the result in a.
 */
void _combine(uint64_t *out, uint64_t *in, uint16_t len, group_op op) {
    for (int i = 0; i < len; i++) {
        *(out + i) = op(*(out + i), *(in + i), UINT64_MAX);
    }
}

/*
 * iSHAKE public interface.
 */


int ishake_init(struct IShakeHash *is, uint32_t blk_size, uint16_t hashbitlen) {
    if (hashbitlen % 64 || !is || !blk_size) {
        return -1;
    }
    if (hashbitlen < 2688 || hashbitlen > 16512 ||
        (hashbitlen > 4160 && hashbitlen < 6528)) {
        return -1;
    }

    is->block_no = 0;
    is->block_size = blk_size;
    is->proc_bytes = 0;
    is->remaining = 0;
    is->output_len = hashbitlen / (uint16_t)8;
    is->hash = calloc((size_t)is->output_len/8, sizeof(uint64_t));

    is->hash_func = (hashbitlen <= 4160) ? shake128 : shake256;
    return 0;
}

int ishake_append(struct IShakeHash *is, unsigned char *data, uint64_t len) {
    if (!is || !data) return -1;

    unsigned char *input;
    input = malloc(len + is->remaining);
    if (input == NULL) return -1;

    // see if we have data pending from previous calls
    if (is->remaining) {
        memcpy(input, is->buf, is->remaining);
        free(is->buf);
    }
    memcpy(input + is->remaining, data, len);
    len += is->remaining;

    // iterate over data, processing as many blocks as possible
    unsigned char *ptr = input;
    while (len >= is->block_size) {
        uint64_t *bhash;
        bhash = calloc((size_t)is->output_len/8, sizeof(uint64_t));
        is->block_no++;
        _hash_block(ptr, is->block_size, is->block_no, bhash,
                   is->output_len, is->hash_func);
        _combine(is->hash, bhash, (uint16_t)(is->output_len/8), add_mod);

        ptr += is->block_size;
        is->proc_bytes += is->block_size;
        len -= is->block_size;
        free(bhash);
    }

    // store remaining data
    is->remaining = (uint32_t)len;
    if (is->remaining) {
        is->buf = malloc(is->remaining);
        if (is->buf == NULL) return -1;
        memcpy(is->buf, ptr, len);
    }
    free(input);

    return 0;
}


int ishake_update(struct IShakeHash *is,
                  uint64_t blk_no,
                  unsigned char *old_data,
                  unsigned char *new_data) {

    if (is == NULL || blk_no == 0 || old_data == NULL || new_data == NULL) {
        return -1;
    }

    uint64_t *oldhash, *newhash;

    oldhash = calloc((size_t)is->output_len/8, sizeof(uint64_t));
    newhash = calloc((size_t)is->output_len/8, sizeof(uint64_t));

    _hash_block(old_data, is->block_size, blk_no, oldhash, is->output_len,
                is->hash_func);
    _hash_block(new_data, is->block_size, blk_no, newhash, is->output_len,
                is->hash_func);

    _combine(is->hash, oldhash, (uint16_t)(is->output_len/8), sub_mod);
    _combine(is->hash, newhash, (uint16_t)(is->output_len/8), add_mod);

    free(oldhash);
    free(newhash);

    return 0;
}


int ishake_final(struct IShakeHash *is, uint8_t *output) {

    if (output == NULL || is == NULL) return -1;

    if (is->remaining || !is->proc_bytes) { // hash the last remaining data
        uint64_t *bhash;
        bhash = calloc((size_t)is->output_len/8, sizeof(uint64_t));
        is->block_no++;
        _hash_block(is->buf, is->remaining, is->block_no, bhash,
                    is->output_len, is->hash_func);
        _combine(is->hash, bhash, (uint16_t)(is->output_len/8), add_mod);
        is->proc_bytes += is->remaining;
        is->remaining = 0;
        free(bhash);
        if (is->buf) free(is->buf);
        is->buf = NULL;
    }

    // copy the resulting digest into output
    for (int i = 0; i < is->output_len / 8; i++) {
        *(output + (i * 8) + 7) = (uint8_t) *(is->hash + i);
        *(output + (i * 8) + 6) = (uint8_t) (*(is->hash + i) >> 8);
        *(output + (i * 8) + 5) = (uint8_t) (*(is->hash + i) >> 16);
        *(output + (i * 8) + 4) = (uint8_t) (*(is->hash + i) >> 24);
        *(output + (i * 8) + 3) = (uint8_t) (*(is->hash + i) >> 32);
        *(output + (i * 8) + 2) = (uint8_t) (*(is->hash + i) >> 40);
        *(output + (i * 8) + 1) = (uint8_t) (*(is->hash + i) >> 48);
        *(output + (i * 8))     = (uint8_t) (*(is->hash + i) >> 56);
    }

    return 0;
}

void ishake_cleanup(struct IShakeHash *is) {
    if (is->buf) free(is->buf);
    if (is->hash) free(is->hash);
    free(is);
}

int ishake_hash(unsigned char *data,
                uint64_t len,
                uint8_t *hash,
                uint16_t hashbitlen) {

    if (hashbitlen % 8) return -1;

    struct IShakeHash *is;
    is = malloc(sizeof(struct IShakeHash));

    int rinit = ishake_init(is, (uint32_t) _ISHAKE_BLOCK_SIZE, hashbitlen);
    if (rinit) {
        ishake_cleanup(is);
        return -2;
    }

    int rappend = ishake_append(is, data, len);
    if (rappend) {
        ishake_cleanup(is);
        return -3;
    }

    int rfinal = ishake_final(is, hash);
    if (rfinal) {
        ishake_cleanup(is);
        return -4;
    }

    ishake_cleanup(is);
    return 0;
}
