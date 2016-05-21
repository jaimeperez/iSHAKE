#include "modulo_arithmetics.h"

uint64_t add_mod(uint64_t a, uint64_t b, uint64_t m) {
    if (a <= (m - b)) {
        return a + b;
    }
    b = m - b;
    return (a >= b) ? a - b : m - b + a;
}

uint64_t sub_mod(uint64_t a, uint64_t b, uint64_t m) {
    if (a >= b) {
        return a - b;
    }
    return m - b + a;
}
