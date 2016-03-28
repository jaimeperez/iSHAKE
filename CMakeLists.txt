cmake_minimum_required(VERSION 3.2)
project(iSHAKE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c11")

set(LIBKECCAK lib/libkeccak-tiny/keccak-tiny.c)
set(SHA3SUM_FILES src/sha3sum.c ${LIBKECCAK})
set(LIBISHAKE src/ishake.c)
set(ISHAKESUM_FILES src/ishakesum.c ${LIBISHAKE} ${LIBKECCAK})

add_executable(sha3sum ${SHA3SUM_FILES})
add_executable(ishakesum ${ISHAKESUM_FILES})

