iSHAKE
======

This is an implementation of the [_iSHAKE_ incremental hashing
algorithm](http://csrc.nist.gov/groups/ST/hash/sha-3/Aug2014/documents/gligoroski_paper_sha3_2014_workshop.pdf), 
based on the relatively new _SHA3_ standard and particularly on the *shake*
__extendable-output functions__ included in
[NIST FIPS 202](http://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.202.pdf).

## Status

This library is **under development**, and therefore should be considered 
unstable and buggy. Please do not use it unless you know what you are doing.

## Build

This project uses `cmake` and `make` to build. Just run `cmake` in the root 
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
    * Additionally, a file can be specified as the source of the data to hash.
      Input can also be passed into the utility by using a UNIX pipe. 

* `ishakesum` the equivalent to the UNIX utility _shasum_ for iSHAKE. It has
two different variants, iSHAKE128 and iSHAKE256, both allowing extendable 
output. The following parameters are supported:
  
    * `--128` to select the 128-bit equivalent version of the algorithm. 
    Output length defaults to 2688 bits.
    * `--256` to select the 256-bit equivalent version of the algorithm. 
    Output lenght defaults to 6528 bits.
    * `--bits` to specify the amount of bits in the output. Must be a 
    multiple of 64. The ranges 2688 - 4160 for 128-bit equivalent and 6528 - 
    16512 for 256-bit equivalent are allowed.
    * Additionally, a file can be specified as the source of the data to hash.
      Input can also be passed into the utility by using a UNIX pipe. 

The library can also be used directly. Just include `ishake.h` and use the 
interface. Make sure to call `ishake_init()` before other functions of the 
interface, and `ishake_final()` when you've finished feeding data into the 
algorithm, in order to retrieve the final hash. `ishake_hash()` acts as a 
convenience wrapper around the rest of the functions. 

## Credits

_iSHAKE_ is proposed by Hristina Mihajloska, Danilo Gligoroski and Simona
Samardjiska.

The [tiny SHA3 implementation](https://github.com/coruus/keccak-tiny)
used in the library by David Leon Gil, licensed 
under [CC0](https://wiki.creativecommons.org/wiki/CC0).

