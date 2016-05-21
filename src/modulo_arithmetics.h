#include <stdint.h>

#ifndef ISHAKE_MODULO_ARITHMETICS_H
#define ISHAKE_MODULO_ARITHMETICS_H

/*
 * Type definition for a function that performs our group operation.
 */
typedef uint64_t (*group_op)(uint64_t, uint64_t, uint64_t);

/*
 * Obtain the addition of a and b modulo m. Overflow safe.
 */
uint64_t add_mod(uint64_t a, uint64_t b, uint64_t m);

/*
 * Obtain the subtraction of a and b modulo m. Overflow safe.
 */
uint64_t sub_mod(uint64_t a, uint64_t b, uint64_t m);

#endif //ISHAKE_MODULO_ARITHMETICS_H
