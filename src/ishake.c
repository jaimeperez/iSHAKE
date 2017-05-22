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

#include <stdio.h>
#include <string.h>
#include "ishake.h"
#include "utils.h"
#include "KeccakCodePackage.h"


int _hash_block(
        ishake_t *is,
        ishake_block_t *block,
        uint64_t *hash
) {
    ishake_header h = block->header;
    if (block->header.length == 8) { // just an index
        h.value.idx = swap_uint64(h.value.idx);
    } else { // we have a nonce and a pointer to the next block
        h.value.nonce.nonce = swap_uint64(h.value.nonce.nonce);
        h.value.nonce.next = swap_uint64(h.value.nonce.next);
    }
    uint8_t *data = NULL;
    data = malloc((block->data_len + h.length) * sizeof(uint8_t));
    if (data == NULL) {
        return -1;
    }
    memcpy(data, block->data, block->data_len);
    memcpy(data + block->data_len, &h.value, h.length);

    uint8_t *buf;
    buf = calloc((unsigned int)is->output_len / 8, sizeof(uint8_t));

    Keccak_HashInstance keccak;
    if (is->output_len <= 4160) { // we're using iSHAKE128
        Keccak_HashInitialize_SHAKE128(&keccak);
    } else { // iSHAKE256
        Keccak_HashInitialize_SHAKE256(&keccak);
    }
    keccak.fixedOutputLength = is->output_len;
    Keccak_HashUpdate(&keccak, data, (block->data_len + h.length) * 8);
    Keccak_HashFinal(&keccak, buf);
    free(data);

    // cast the resulting hash to (uint64_t *) for simplicity
    uint8_t2uint64_t(hash, buf, (unsigned long)is->output_len/8);
    free(buf);
    return 0;
}

uint64_t *ishake_hash_block(ishake_t *is, ishake_block_t *block) {
    uint64_t *hash;
    hash = calloc((size_t)is->output_len/64, sizeof(uint64_t));
    if (_hash_block(is, block, hash) != 0) {
        return NULL;
    }
    return hash;
}

/*
 * Hash an ishake block and combine it into an existing hash in the way
 * specified by op.
 */
int _hash_and_combine(ishake_t *is, ishake_block_t *block, group_op op) {
    if (is->thrd_no > 0) { // use the workers to do this
        ishake_task_t *task = calloc(1, sizeof(ishake_task_t));
        task->block = block;
        task->prev = NULL;
        task->op = op;
        pthread_mutex_lock(&(is->stack_lck));
        if (is->stack == NULL) { // empty stack
            is->stack = task;
        } else { // non-empty stack
            task->prev = is->stack;
            is->stack = task;
        }
        pthread_mutex_unlock(&(is->stack_lck));
        pthread_cond_signal(&is->data_available);
        return 0;
    }

    // no threads, process here
    uint64_t *hash = ishake_hash_block(is, block);
    if (hash == NULL) {
        return -1;
    }
    combine(is->hash, hash, (uint16_t)(is->output_len/64), op);
    free(hash);
    return 0;
}


/**
 * Worker thread.
 *
 * It will pick up tasks from a stack, hash the corresponding block and combine
 * it with the existing hash.
 */
void *_worker(void *arg) {
    ishake_t *is = (ishake_t *) arg;

    pthread_mutex_lock(&(is->stack_lck));
    while (1) {
        if (is->stack != NULL) {
            ishake_task_t *task = is->stack;
            is->stack = task->prev;
            pthread_mutex_unlock(&(is->stack_lck));

            // hash the block
            uint64_t *hash = ishake_hash_block(is, task->block);
            free(task->block->data);
            free(task->block);

            // combine the resulting hash
            pthread_mutex_lock(&is->combine_lck);
            combine(is->hash, hash, (uint16_t)
                    (is->output_len/64), task->op);
            pthread_mutex_unlock(&is->combine_lck);
            free(hash);
            free(task);

            // do this once we stop calling ishake_append() in main()
            pthread_mutex_lock(&is->stack_lck);
            continue;
        }

        if (is->done == 1) {
            pthread_mutex_unlock(&is->stack_lck);
            break;
        }
        pthread_cond_wait(&is->data_available, &is->stack_lck);
    }

    pthread_exit(NULL);
}


/***************************
 | iSHAKE public interface |
 ***************************/


int ishake_init(ishake_t *is,
                uint32_t blk_size,
                uint16_t hashbitlen,
                uint8_t mode,
                uint16_t threads) {
    if (hashbitlen % 64 || !is || !blk_size) {
        return -1;
    }
    if (hashbitlen < 2688 || hashbitlen > 16512 ||
        (hashbitlen > 4160 && hashbitlen < 6528)) {
        return -1;
    }

    // common initialization
    is->mode = mode;
    is->block_no = 0;
    is->block_size = blk_size;
    is->proc_bytes = 0;
    is->remaining = 0;
    is->buf = 0;
    is->output_len = hashbitlen;// / (uint16_t)8;
    is->hash = calloc((size_t)is->output_len/64, sizeof(uint64_t));

    if (threads > 0) { // we are asked to use threads
        // basic thread initialization
        is->thrd_no = threads;
        is->threads = calloc(1, sizeof(pthread_t *) * threads);
        is->done = 0;
        is->stack = NULL;

        // mutexes/conditions initialization
        pthread_mutex_init(&is->stack_lck, NULL);
        pthread_mutex_init(&is->combine_lck, NULL);
        pthread_cond_init(&is->data_available, NULL);

        // initialize worker pool
        for (int i = 0; i < threads; i++) {
            is->threads[i] = calloc(1, sizeof(pthread_t));
            if (pthread_create(is->threads[i], NULL, _worker, (void *)is)) {
                return -1;
            }
        }
    }

    return 0;
}


int ishake_append(ishake_t *is, unsigned char *data, uint64_t len) {
    if (!is || !data || is->mode == ISHAKE_FULL_MODE) return -1;

    unsigned char *input = NULL;
    input = calloc(len + is->remaining, sizeof(unsigned char));
    if (input == NULL) return -1;

    // see if we have data pending from previous calls
    if (is->remaining) {
        memcpy(input, is->buf, is->remaining);
        free(is->buf);
        is->buf = NULL;
    }
    memcpy(input + is->remaining, data, len);
    len += is->remaining;

    // iterate over data, processing as many blocks as possible
    unsigned char *ptr = input;
    while (len >= (is->block_size - sizeof(uint64_t))) {
        is->block_no++;
        ishake_block_t *block = malloc(sizeof(ishake_block_t));
        block->header.value.idx = is->block_no;
        block->header.length = sizeof(is->block_no);
        block->data_len = is->block_size - sizeof(uint64_t);
        block->data = calloc(block->data_len, sizeof(unsigned char));
        memcpy(block->data, ptr, block->data_len);

        _hash_and_combine(is, block, add_mod64);

        ptr += block->data_len;
        is->proc_bytes += block->data_len;
        len -= block->data_len;
    }

    // store remaining data
    is->remaining = (uint32_t)len;
    if (is->remaining) {
        is->buf = calloc(is->remaining, sizeof(unsigned char));
        if (is->buf == NULL) return -1;
        memcpy(is->buf, ptr, is->remaining);
    }
    free(input);

    return 0;
}


int ishake_insert(ishake_t *is, ishake_block_t *previous, ishake_block_t *new) {
    if (is == NULL) {
        return -1;
    }

    // insert() only available in FULL mode, 16 byte headers required per block
    if (new->header.length != 16) {
        return -1;
    }

    if (previous != NULL) {
        if (previous->header.length != 16) {
            return -1;
        }

        // clone the block to change the "next" pointer
        ishake_block_t *new_prev = calloc(1, sizeof(ishake_block_t));
        new_prev->data_len = previous->data_len;
        new_prev->header.length = previous->header.length;
        new_prev->header.value.nonce.nonce = previous->header.value.nonce.nonce;
        new_prev->header.value.nonce.next = new->header.value.nonce.nonce;
        new_prev->data = calloc(previous->data_len, sizeof(unsigned char));
        memcpy(new_prev->data, previous->data, previous->data_len);

        // rehash the previous block with "next" pointing to new block
        ishake_update(is, previous, new_prev);
    }

    // add the new block
    _hash_and_combine(is, new, add_mod64);

    return 0;
}


int ishake_delete(ishake_t *is, ishake_block_t *previous,
                  ishake_block_t *deleted) {
    if (is == NULL) {
        return -1;
    }

    if (previous != NULL) {
        if ((*previous).header.length != 16) {
            return -1;
        }

        // clone the block to change the "next" pointer
        ishake_block_t *new = calloc(1, sizeof(ishake_block_t));
        new->data_len = previous->data_len;
        new->header.length = previous->header.length;
        new->header.value.nonce.nonce = previous->header.value.nonce.nonce;
        new->header.value.nonce.next = deleted->header.value.nonce.next;
        new->data = calloc(previous->data_len, sizeof(unsigned char));
        memcpy(new->data, previous->data, previous->data_len);

        // "remove" the previous block, and add it back with the new "next"
        // pointer
        ishake_update(is, previous, new);
    }

    // delete the block
    _hash_and_combine(is, deleted, sub_mod64);

    return 0;
}


int ishake_update(ishake_t *is, ishake_block_t *old, ishake_block_t *new) {
    if (is == NULL) {
        return -1;
    }

    _hash_and_combine(is, old, sub_mod64);
    _hash_and_combine(is, new, add_mod64);

    return 0;
}


int ishake_final(ishake_t *is, uint8_t *output) {
    if (output == NULL || is == NULL) return -1;

    uint64_t *empty = calloc((size_t)is->output_len/64, sizeof(uint64_t));

    if (is->mode == ISHAKE_APPEND_ONLY_MODE &&
         ( // we still need to hash, because either:
           is->remaining || // there's still data remaining, or
           ( // we didn't hash anything yet, so we need to hash an empty string
             (memcmp(is->hash, empty, (size_t)is->output_len/64) == 0) &&
             !is->proc_bytes
           )
         )
    ) {
        // hash the last remaining data
        is->block_no++;
        ishake_block_t *block = malloc(sizeof(ishake_block_t));
        block->data = is->buf;
        block->data = calloc(is->remaining, sizeof(unsigned char));
        memcpy(block->data, is->buf, is->remaining);
        block->data_len = is->remaining;
        block->header.value.idx = is->block_no;
        block->header.length = sizeof(is->block_no);

        _hash_and_combine(is, block, add_mod64);

        is->proc_bytes += is->remaining;
        is->remaining = 0;
        free(is->buf);
        is->buf = NULL;
    }

    if (is->thrd_no > 0) { // we are using threads, tell them we are done
        pthread_mutex_lock(&is->stack_lck);
        is->done = 1;
        pthread_cond_broadcast(&is->data_available);
        pthread_mutex_unlock(&is->stack_lck);

        // block until all workers are done
        for (int i = 0; i < is->thrd_no; i++) {
            pthread_join(*(is->threads)[i], NULL);
            free(is->threads[i]);
        }
        free(is->threads);
    }

    // copy the resulting digest into output
    uint64_t2uint8_t(output, is->hash, (unsigned long)is->output_len/64);

    return 0;
}


void ishake_cleanup(ishake_t *is) {
    if (is->buf) free(is->buf);
    if (is->hash) free(is->hash);
    free(is);
}


int ishake_hash_p(unsigned char *data,
                uint64_t len,
                uint8_t *hash,
                uint16_t hashbitlen,
                uint16_t threadno) {

    if (hashbitlen % 8) return -1;

    ishake_t *is;
    is = malloc(sizeof(ishake_t));

    int rinit = ishake_init(is, (uint32_t) ISHAKE_BLOCK_SIZE, hashbitlen,
                            ISHAKE_APPEND_ONLY_MODE, threadno);
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


int ishake_hash(unsigned char *data,
                uint64_t len,
                uint8_t *hash,
                uint16_t hashbitlen) {
    return ishake_hash_p(data, len, hash, hashbitlen, 0);
}
