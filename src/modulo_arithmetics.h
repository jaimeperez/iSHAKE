#include <stdint.h>

#ifndef ISHAKE_MODULO_ARITHMETICS_H
#define ISHAKE_MODULO_ARITHMETICS_H

/*
 * Type definition for a function that performs our group operation.
 */
typedef uint_fast64_t (*group_op)(uint_fast64_t, uint_fast64_t);

/*
 * Obtain the addition of a and b modulo 2^64. Overflow ignored.
 */
uint_fast64_t add_mod64(uint_fast64_t a, uint_fast64_t b);

/*
 * Obtain the subtraction of a and b modulo 2^64. Overflow ignored.
 */
uint_fast64_t sub_mod64(uint_fast64_t a, uint_fast64_t b);

#endif //ISHAKE_MODULO_ARITHMETICS_H
