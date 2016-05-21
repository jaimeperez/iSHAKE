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

#include <stdint.h>

#ifndef ISHAKE_UTILS_H
#define ISHAKE_UTILS_H

void bin2hex(char **output, uint8_t *data, unsigned long len);

void hex2bin(char **output, uint8_t *data, unsigned long len);

void uint8_t2uint64_t(uint64_t *output, uint8_t *data, unsigned long len);

void uint64_t2uint8_t(uint8_t *output, uint64_t *data, unsigned long len);

/*
 * Change a 64-bit unsigned integer from big to little endian, or vice-versa.
 */
uint64_t swap_uint64(uint64_t val);

/*
 * Return the full path to a file located in dirname with name filename.
 *
 * The path will be dirname + '/' + filename + '\0'.
 */
void resolve_file_path(char **path, char *dirname, char *filename);

/*
 * Parse a string into a number, given the base.
 */
uint64_t str2uint64_t(char *str, int base);

#endif //ISHAKE_UTILS_H
