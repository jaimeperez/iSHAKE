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
#include <stdlib.h>

#include "utils.h"

void bin2hex(char **output, uint8_t *data, unsigned long len) {
    *output = calloc((size_t)len * 2 + 1, sizeof(char));
    for (int i = 0; i < len; i++) {
        snprintf(*output + (i*2), 3, "%02x", data[i]);
    }
}

void hex2bin(char **output, uint8_t *data, unsigned long len) {
    *output = calloc((size_t)(len / 2) + 1, sizeof(char));
    for (int i = 0; i < len/2; i++) {
        uint8_t b[3];
        b[0] = *(data + i * 2);
        b[1] = *(data + i * 2 + 1);
        b[2] = 0;
        *(*output + i) = (char)strtol((const char *)b, NULL, 16);
    }
}

void uint8_t2uint64_t(uint64_t *output, uint8_t *data, unsigned long len) {
    for (int i = 0; i < len / 8; i++) {
        *(output + i) = ((uint64_t)*(data + i * 8)     << 56) +
                        ((uint64_t)*(data + i * 8 + 1) << 48) +
                        ((uint64_t)*(data + i * 8 + 2) << 40) +
                        ((uint64_t)*(data + i * 8 + 3) << 32) +
                        ((uint64_t)*(data + i * 8 + 4) << 24) +
                        ((uint64_t)*(data + i * 8 + 5) << 16) +
                        ((uint64_t)*(data + i * 8 + 6) << 8)  +
                        ((uint64_t)*(data + i * 8 + 7));
    }
}

void uint64_t2uint8_t(uint8_t *output, uint64_t *data, unsigned long len) {
    for (int i = 0; i < len; i++) {
        *(output + (i * 8) + 7) = (uint8_t) *(data + i);
        *(output + (i * 8) + 6) = (uint8_t) (*(data + i) >> 8);
        *(output + (i * 8) + 5) = (uint8_t) (*(data + i) >> 16);
        *(output + (i * 8) + 4) = (uint8_t) (*(data + i) >> 24);
        *(output + (i * 8) + 3) = (uint8_t) (*(data + i) >> 32);
        *(output + (i * 8) + 2) = (uint8_t) (*(data + i) >> 40);
        *(output + (i * 8) + 1) = (uint8_t) (*(data + i) >> 48);
        *(output + (i * 8))     = (uint8_t) (*(data + i) >> 56);
    }
}

uint64_t swap_uint64(uint64_t val)
{
    val = ((val << 8)  & 0xFF00FF00FF00FF00ULL ) |
          ((val >> 8)  & 0x00FF00FF00FF00FFULL );
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL ) |
          ((val >> 16) & 0x0000FFFF0000FFFFULL );
    return (val << 32) | (val >> 32);
}
