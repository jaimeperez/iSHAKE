#!/usr/bin/env python

import os
import sys
basedir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
sys.path.append('%s/utils' % basedir)

import fire
import pprint
import random
import shutil
import string
import tempfile

from ishakelib import IShakeFulLRW
import setup_dir


def test_commands(tmpdir, threads, block_size=26, total_blocks=100):
    ishake = IShakeFulLRW(dir=tmpdir, profile=True, threads=threads, block_size=block_size)
    results = {}
    results['main'] = ishake.hash()
    results['insert'] = {}
    results['delete'] = {}

    results['delete']['middle'] = ishake.delete("%.10d" % 2, 2, "%.10d" % 1, 1, 2)
    if results['main']['digest'] == ishake.digest:
        raise Exception('Digest after deletion in the middle did not change.')
    results['insert']['middle'] = ishake.insert("%.10d" % 2, 2, "%.10d" % 1, 1, 2)
    if results['main']['digest'] != ishake.digest:
        raise Exception('Digest after re-insertion in the middle does not match the original.')

    results['delete']['first'] = ishake.delete("%.10d" % 1, 1, next_nonce=2)
    if results['main']['digest'] == ishake.digest:
        raise Exception('Digest after deletion in the beginning did not change.')
    results['insert']['first'] = ishake.insert("%.10d" % 1, 1, next_nonce=2)
    if results['main']['digest'] != ishake.digest:
        raise Exception('Digest after re-insertion in the beginning does not match the original.')

    results['delete']['last'] = ishake.delete("%.10d" % total_blocks, total_blocks, "%.10d" % (total_blocks - 1),
                                              total_blocks - 1)
    if results['main']['digest'] == ishake.digest:
        raise Exception('Digest after deletion in the end did not change.')
    results['insert']['last'] = ishake.insert("%.10d" % total_blocks, total_blocks, "%.10d" % (total_blocks - 1),
                                              total_blocks - 1)
    if results['main']['digest'] != ishake.digest:
        raise Exception('Digest after re-insertion in the end does not match the original.')

    results['update1'] = ishake.update("%.10d" % 1, "%.10d" % 2, 1)
    if results['main']['digest'] == ishake.digest:
        raise Exception('Digest after first update did not change.')
    results['update2'] = ishake.update("%.10d" % 2, "%.10d" % 1, 1)
    if results['main']['digest'] != ishake.digest:
        raise Exception('Digest after second update does not match the original.')

    return results


def main(dirname=None, repetitions=1, block_size=26, total_blocks=100, threads=0, debug=False):
    if not debug:
        sys.tracebacklimit = 0
    if not block_size > 0:
        raise ValueError('block-size must be greater than 0')
    if not total_blocks > 2:
        raise ValueError('total-blocks must be greater than 2')

    clean = False
    if not dirname:
        dirname = "%s/%s" % (
            tempfile.gettempdir(),
            ''.join(random.choice(string.ascii_letters + string.digits) for _ in range(10))
        )
        setup_dir.setup(dirname, block_size - 16, (block_size - 16) * total_blocks)
        clean = True

    # initialize time recording
    time = {
        'hash': {'cpu': 0, 'wall': 0},
        'update': {'cpu': 0, 'wall': 0},
        'insert': {
            'first': {'cpu': 0, 'wall': 0},
            'middle': {'cpu': 0, 'wall': 0},
            'last': {'cpu': 0, 'wall': 0},
        },
        'delete': {
            'first': {'cpu': 0, 'wall': 0},
            'middle': {'cpu': 0, 'wall': 0},
            'last': {'cpu': 0, 'wall': 0}
        }
    }

    # run the test, adding the timing results together
    for i in range(repetitions):
        results = test_commands(dirname, threads, block_size, total_blocks)
        time['hash']['cpu'] += float(results['main']['cpu'])
        time['hash']['wall'] += float(results['main']['wall'])
        time['update']['cpu'] += float(results['update1']['cpu'])
        time['update']['cpu'] += float(results['update2']['cpu'])
        time['update']['wall'] += float(results['update1']['wall'])
        time['update']['wall'] += float(results['update2']['wall'])
        time['insert']['first']['cpu'] += float(results['insert']['first']['cpu'])
        time['insert']['first']['wall'] += float(results['insert']['first']['wall'])
        time['insert']['middle']['cpu'] += float(results['insert']['middle']['cpu'])
        time['insert']['middle']['wall'] += float(results['insert']['middle']['wall'])
        time['insert']['last']['cpu'] += float(results['insert']['last']['cpu'])
        time['insert']['last']['wall'] += float(results['insert']['last']['wall'])
        time['delete']['first']['cpu'] += float(results['delete']['first']['cpu'])
        time['delete']['first']['wall'] += float(results['delete']['first']['wall'])
        time['delete']['middle']['cpu'] += float(results['delete']['middle']['cpu'])
        time['delete']['middle']['wall'] += float(results['delete']['middle']['wall'])
        time['delete']['last']['cpu'] += float(results['delete']['last']['cpu'])
        time['delete']['last']['wall'] += float(results['delete']['last']['wall'])

    # compute averages
    time['hash']['cpu'] /= repetitions
    time['hash']['wall'] /= repetitions
    time['update']['cpu'] /= repetitions * 2
    time['update']['wall'] /= repetitions * 2
    time['insert']['first']['cpu'] /= repetitions
    time['insert']['first']['wall'] /= repetitions
    time['insert']['middle']['cpu'] /= repetitions
    time['insert']['middle']['wall'] /= repetitions
    time['insert']['last']['cpu'] /= repetitions
    time['insert']['last']['wall'] /= repetitions
    time['delete']['first']['cpu'] /= repetitions
    time['delete']['first']['wall'] /= repetitions
    time['delete']['middle']['cpu'] /= repetitions
    time['delete']['middle']['wall'] /= repetitions
    time['delete']['last']['cpu'] /= repetitions
    time['delete']['last']['wall'] /= repetitions

    pp = pprint.PrettyPrinter(indent=2,)
    pp.pprint(time)

    # remove temp dir
    if clean:
        shutil.rmtree(dirname)

if __name__ == '__main__':
    fire.Fire(main, name=sys.argv[0])
