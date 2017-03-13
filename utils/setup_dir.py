#!/usr/bin/env python

import fire
import os
import random
import string
import sys


rand_str = lambda n: ''.join([random.choice(string.printable) for i in xrange(n)])


def setup(dir, block_size, total_bytes):
    """Sets up a directory suitable to test with the ishakesumd utility.

    It writes <total-bytes> split into the necessary amount of <block-size> size files to the target
     <dir> directory.
    """
    try:
        blk_sz = int(block_size)
    except ValueError:
        raise ValueError('invalid block size: %s' % block_size)
    if blk_sz == 0:
        raise ValueError('invalid block size: 0')

    try:
        tot_b = int(total_bytes)
    except ValueError:
        raise ValueError('invalid total bytes: %s' % total_bytes)
    if tot_b == 0:
        raise ValueError('invalid total bytes: 0')

    if os.path.isdir(dir):
        raise ValueError('invalid directory: %s' % dir)

    try:
        os.mkdir(dir, 0755)
    except OSError:
        raise Exception('could not create directory %s' % dir)

    i = 1
    written_b = 0
    while tot_b > 0:
        len = blk_sz if tot_b > blk_sz else tot_b
        s = rand_str(len)

        fd = os.open("%s/%.10d" % (dir, i), os.O_CREAT | os.O_RDWR)
        os.write(fd, s)
        os.close(fd)

        i += 1
        tot_b -= len
        written_b += len

    print "%i files written." % written_b


if __name__ == '__main__':
    fire.Fire(setup, name=sys.argv[0])
