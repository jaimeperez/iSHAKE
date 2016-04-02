cmake_minimum_required(VERSION 2.8)
project(iSHAKE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu99")

set(ISHAKE_UTILS src/utils.c)
set(LIBKECCAK lib/libkeccak-tiny/keccak-tiny.c)
set(SHA3SUM_FILES src/sha3sum.c ${LIBKECCAK} ${ISHAKE_UTILS})
set(LIBISHAKE src/ishake.c)
set(ISHAKESUM_FILES src/ishakesum.c ${LIBISHAKE} ${LIBKECCAK} ${ISHAKE_UTILS})

add_executable(sha3sum ${SHA3SUM_FILES})
add_executable(ishakesum ${ISHAKESUM_FILES})

