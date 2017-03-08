cmake_minimum_required(VERSION 2.8)
project(iSHAKE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -std=gnu99 -fno-builtin-memset")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -std=gnu99
-fno-builtin-memset")

if (NOT DEFINED KECCAK_TARGET)
    set(KECCAK_TARGET generic64/libkeccak.a)
endif()

include_directories(includes/libkeccak.a.headers)
cmake_policy(SET CMP0015 NEW)
link_directories(lib/)

set(ISHAKE_UTILS src/utils.c src/modulo_arithmetics.c)
set(LIBKECCAK lib/libkeccak-tiny/keccak-tiny.c)
set(SHA3SUM_FILES src/sha3sum.c ${ISHAKE_UTILS})
set(SHA3SUMD_FILES src/sha3sumd.c ${LIBKECCAK} ${ISHAKE_UTILS})
set(LIBISHAKE src/ishake.c)
set(ISHAKESUM_FILES src/ishakesum.c ${LIBISHAKE} ${LIBKECCAK} ${ISHAKE_UTILS})
set(ISHAKESUMD_FILES src/ishakesumd.c ${LIBISHAKE} ${LIBKECCAK} ${ISHAKE_UTILS})
set(COMBINE_FILES src/combine.c ${ISHAKE_UTILS})

add_executable(sha3sum ${SHA3SUM_FILES})
add_executable(sha3sumd ${SHA3SUMD_FILES})
add_executable(ishakesum ${ISHAKESUM_FILES})
add_executable(ishakesumd ${ISHAKESUMD_FILES})
add_executable(combine ${COMBINE_FILES})

target_link_libraries(sha3sum libkeccak.a)

add_custom_target(KeccakCodePackage)
add_custom_command(
        TARGET KeccakCodePackage
        COMMAND mkdir -p ${CMAKE_SOURCE_DIR}/dependencies
        COMMAND git clone git@github.com:gvanas/KeccakCodePackage.git ${CMAKE_SOURCE_DIR}/dependencies/KeccakCodePackage || true
        COMMAND cd ${CMAKE_SOURCE_DIR}/dependencies/KeccakCodePackage && make ${KECCAK_TARGET}
        COMMAND cp -f ${CMAKE_SOURCE_DIR}/dependencies/KeccakCodePackage/bin/${KECCAK_TARGET} lib/
        COMMAND cp -f -r ${CMAKE_SOURCE_DIR}/dependencies/KeccakCodePackage/bin/${KECCAK_TARGET}.headers includes/
)
add_dependencies(sha3sum KeccakCodePackage)