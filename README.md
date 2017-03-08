iSHAKE
======

![alt text](https://travis-ci.org/jaimeperez/iSHAKE.svg?branch=master "Build 
status")

This is an implementation of the [_iSHAKE_ incremental hashing
algorithm](http://csrc.nist.gov/groups/ST/hash/sha-3/Aug2014/documents/gligoroski_paper_sha3_2014_workshop.pdf), 
based on the relatively new _SHA3_ standard and particularly on the *shake*
__extendable-output functions__ included in
[NIST FIPS 202](http://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf).

## Status

This library is **under development**, and therefore should be considered 
unstable and buggy. Please do not use it unless you know what you are doing.

## Build

This project depends on the
[KeccakCodePackage](https://github.com/gvanas/KeccakCodePackage). We will try
to download and build it for you, optimizing it by default for generic 64-bit
platforms. If you are running on a different platform, take a look at the 
different targets provided by the KeccakCodePackage `Makefile.build`, and set
the `KECCAK_TARGET` environment variable to the one that fits the most. Make
sure to use a target to build the library, not the `KeccakSum` utility or the
tests. That means **only the `*/libkeccak.a` targets can be used**.

We use `cmake` and `make` to build. Just run `cmake` in the root 
directory of the project to generate a _Makefile_, and then run `make` to build.

```sh
% cmake .
% make
```

## Usage

A couple of binaries are provided when building:

* `sha3sum` is the equivalent to the UNIX utility _shasum_ for SHA3. It 
allows the computations of both SHA3 and SHAKE digest, with the latter allowing
 extendable output. The following parameters are supported:
 
    * `--shake128` to use the SHAKE128 XOF.
    * `--shake256` to use the SHAKE256 XOF.
    * `--sha3-224` to use SHA3 with 224 bits of output.
    * `--sha3-256` to use SHA3 with 256 bits of output.
    * `--sha3-384` to use SHA3 with 384 bits of output.
    * `--sha3-512` to use SHA3 with 512 bits of output.
    * `--bytes` to specify the amount of bytes desired in the output (only 
    for the two XOFs).
    * `--hex` to indicate that the input is hex-encoded.
    * `--quiet` to indicate that the output should only consist of the hash.
    * Additionally, a file can be specified as the source of the data to hash.
      Input can also be passed into the utility by using a UNIX pipe. 

* `ishakesum` is the equivalent to the UNIX utility _shasum_ for iSHAKE. It has
two different variants, iSHAKE128 and iSHAKE256, both allowing extendable 
output. The following parameters are supported:
  
    * `--128` to select the 128-bit equivalent version of the algorithm. 
    Output length defaults to 2688 bits.
    * `--256` to select the 256-bit equivalent version of the algorithm. 
    Output lenght defaults to 6528 bits.
    * `--bits` to specify the amount of bits in the output. Must be a 
    multiple of 64. The ranges 2688 - 4160 for 128-bit equivalent and 6528 - 
    16512 for 256-bit equivalent are allowed.
    * `--block-size` to specify the amount of bytes of input that should be 
    used per block.
    * `--hex` to indicate that the input is hex-encoded.
    * `--quiet` to indicate that the output should only consist of the hash.
    * Additionally, a file can be specified as the source of the data to hash.
      Input can also be passed into the utility by using a UNIX pipe. 

* `ishakesumd` is the equivalent to _ishakesum_ for directories. It takes a 
directory as a parameter, and searches for files in there, applying the
algorithm over each file as a different block. Files should be numbered
(starting with 1) with their corresponding block number. Its parameters are the
same as for _ishakesum_, with two main differences:
 
    * `--hex` is **not available**. Input cannot be hex-encoded, nor piped 
    into the program.
    * `--rehash` allows recomputing the hash, based on a previous hashed 
    passed as a parameter immediately after this option.
  
The library can also be used directly. Just include `ishake.h` and use the 
interface. Make sure to call `ishake_init()` before other functions of the 
interface, and `ishake_final()` when you've finished feeding data into the 
algorithm, in order to retrieve the final hash. `ishake_hash()` acts as a 
convenience wrapper around the rest of the functions.
 
Refer directly to the [ishake.h header](https://github.com/jaimeperez/iSHAKE/blob/master/src/ishake.h) for details on how to use 
the API. 

## API

The library can be used directly by including the `ishake.h` header file and
using the types and functions defined there. There are two ways to use the API:

- Call the `ishake_hash()` function if you just want to compute the digest
corresponding to a given input.
- Use the rest of the interface to compute the digest over dynamic data or
modify an existing digest.

While the former is very simple and convenient, it is also quite limited as it
will not allow you to use the incremental functionality of the _iSHAKE_
algorithm. In order to do that, you need to use the rest of the functions in
the API, and this is how you use them:

* First, you will need to allocate memory for a `ishake_t` structure that you
will pass around as the first parameter to every function. **Initialize** the
structure by calling the `ishake_init()` function. The structure **must** be 
initialized **always**.
* If you have a **precomputed hash** that you want to recompute based on a 
set of changes to the input, add that hash to the `ishake_t` structure. Bear in
mind that the `hash` field of the `ishake_t` algorithm is declared as a buffer
of `uint64_t` integers, and as such, you need to prepare the hash before. If
your hash is in hexadecimal form, you will need to convert it to binary form
first (use the `hex2bin()` function in defined `utils.h`), and then convert the
result to `uint64_t *` (again, a helper function called `uint8_t2uint64_t()` is 
provided in the `utils.h` header).
* Depending on the **mode** you use, you will be able to perform some 
operations or not. If you are using the _APPEND_ONLY_ mode, you can only append
new data or modify new blocks. On the other hand, the _FULL_RW_ mode will allow
you to insert, update and delete blocks at any given position.
* In order to use most functions, you will need to **pass a block** (or 
several) containing the data to be hashed, plus a header describing the position
of the block. In _APPEND_ONLY_ mode, the header is a simple 64-bit incremental 
index, while in _FULL_RW_ mode the header consist of a random, unique, 64-bit
nonce identifying the block, plus the nonce of the next block in the chain.
* Once you are done modifying the input by appending, updating, inserting or 
deleting blocks, you need to call the `ishake_final()` function to **obtain the
final digest** corresponding to the input given.
* After obtaining the final digest, remember to call the `ishake_cleanup()` 
function to ensure that all internal structures and memory used by _iSHAKE_ 
are **freed**.

### Data types

* `ishake_nonce` represents the header of an _iSHAKE_ block in _FULL_RW_ mode.
It contains the nonce for a given block, plus the nonce of the next block in 
the chain, as 64-bit unsigned integers.

* `ishake_header` represents the header of an _iSHAKE_ block, where that 
could be a 64-bit unsigned integer index in case of _APPEND_ONLY_ mode, or an
`ishake_nonce` structure in _FULL_RW_ mode.

* `ishake_block_t` represents an _iSHAKE_ block, containing the input data plus
the corresponding `ishake_header` structure.

* `ishake_t` is the internal structure used by _iSHAKE_ to pass state 
information around to the different functions of the API.

### Functions

Here is a more detailed description of the functions provided by the _iSHAKE_
API:

* `ishake_init()`: initializes an `ishake_t` structure. Accepts as parameters
the **block size** to use, the **length in bits** of the resulting hash, the 
**mode of operation** and the **number of threads** to use.

* `ishake_append()`: appends data to the existing input. It will split the 
data in chunks of the size of an _iSHAKE_ block automatically, and keep the 
excess until more data arrives and can be appended to form a new block. It 
accepts as parameters the **data** to append and its **length**.

* `ishake_insert()`: inserts an _iSHAKE_ block at a given position, right 
after another block given. It accepts an `ishake_block_t` structure with the
block after which the data must be inserted, and another `ishake_block_t` 
structure containing the data to insert itself. Note that it is your 
responsibility to build the inserted block correctly. This means if there was
a block already after the previous block (that is, we are not _appending_ the
block), then the `next` pointer of the inserted block must be the same as the
`next` pointer of the previous block. You don't need to modify the previous 
block yourself, _iSHAKE_ will take care of that for you though. Note also 
that if you want to insert a block at the beginning of the chain, you then need
to pass `NULL` as the previous block.
 
* `ishake_delete()`: deletes an _iSHAKE_ block at a given position, right 
after another block given. The behaviour of this function is analogous to the
`ishake_insert()` function.

* `ishake_update()`: updates the contents of a given _iSHAKE_ block and its
corresponding hash. Its parameters are the original block and the modified one,
where the difference between both must be only regarding the data. The function 
will not prevent you from updating a block by changing its headers too, but this
only makes sense under certain circumstances (like inserting or deleting blocks)
and you should not do that unless you know what you are doing.

* `ishake_final()`: computes the final digest corresponding to the input given.
It needs as a parameter a buffer of `uint8_t` integers previously allocated to
store as many bits as specified when initializing the algorithm when calling 
`ishake_init()`.

* `ishake_cleanup()`: cleans and frees all internal structures used by the 
algorithm. Make sure to call this function always.

* `ishake_hash()`: computes the _iSHAKE_ hash corresponding a given input. As
parameters, it needs a string with the data to hash plus its length, a buffer
of `uint8_t` integers where to store the result and its corresponding length 
in bits.

### Parallel processing

_iSHAKE_ allows you to process the blocks in parallel to boost performance. This
allows you to leverage the computing power of CPUs with multiple cores and 
pipelines to reduce the total processing time to the minimum possible. In 
order to use this feature, just pass a positive number of threads to use to the
`ishake_init()` function when initializing the `ishake_t` structure, and 
_iSHAKE_ will take care of the rest for you.

There is no magic bullet to determine what amount of threads is best for your
setup. Check the amount of cores you have available and test different 
configurations in order to find out the optimal number of threads to use.

## Credits

_iSHAKE_ was proposed by Hristina Mihajloska, Danilo Gligoroski and Simona
Samardjiska, and implemented by Jaime Pérez Crespo.

The [tiny SHA3 implementation](https://github.com/coruus/keccak-tiny)
used in the library by David Leon Gil, licensed 
under [CC0](https://wiki.creativecommons.org/wiki/CC0).

