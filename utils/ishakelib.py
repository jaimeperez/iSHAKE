import os
import subprocess
import tempfile

from shutil import copyfile
from subprocess import PIPE


class IShakeModes(object):

    def __init__(self, alg, dir='.', threads=0, profile=False, block_size=1048576, output_bits=2688, mode=128):
        self._dir = dir
        bits = {
            128: range(2688, 4160),
            256: range(6528, 16512)
        }

        if alg not in ['APPEND_ONLY', 'FULL']:
            print 'Invalid algorithm mode "%s", must be "APPEND_ONLY" or "FULL"' % alg
            exit(-1)
        self._alg = alg

        self._mode = int(mode)
        if self._mode not in [128, 256]:
            raise ValueError('Invalid mode "%s", must be one of "128" or "256"' % mode)

        self._threads = int(threads)

        self._output_bits = int(output_bits)
        if self._output_bits not in bits[self._mode] or (self._output_bits % 64) != 0:
            raise ValueError('Invalid output bits "%s"' % output_bits)

        self._hash = '0' * (self._output_bits / 4)

        self._block_size = int(block_size)

        if profile not in [False, True]:
            print 'Invalid profile "%s", must be one of "True" or "False"' % profile
            exit(-1)

        self._profile = '--profile' if profile else ''
        self._tempdir = tempfile.mkdtemp()
        self._ishakesumd = self._which('ishakesumd')
        self._ishakesum = self._which('ishakesum')

    def _which(self, program):
        """Find a command and return a path to it."""

        def is_exe(path):
            return os.path.isfile(path) and os.access(path, os.X_OK)

        fpath, fname = os.path.split(program)
        if fpath:
            if is_exe(program):
                return program
        else:
            os.environ["PATH"] += os.pathsep + '../bin/'
            for path in os.environ["PATH"].split(os.pathsep):
                path = path.strip('"')
                exe_file = os.path.join(path, program)
                if is_exe(exe_file):
                    return exe_file

        print 'ishakesumd not found, build it or place it in the PATH before using this tool.'
        exit(1)

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Clean up all traces of our execution."""
        os.rmdir(self._tempdir)

    def _run(self, command):
        """Run a given command, optionally measuring its performance."""
        # print command
        if self._profile:
            result = subprocess.Popen(command, shell=True, stdout=PIPE).stdout.read().splitlines()
            self._hash = result[0]
            cpu = result[1].split(':')[1].strip()
            wall = result[2].split(':')[1].strip()
            return {'digest': self._hash, 'cpu': cpu, 'wall': wall}

        self._hash = subprocess.Popen(command, shell=True, stdout=PIPE).stdout.read().strip()
        return self._hash

    def update(self, old, new, blk_id):
        """Update the has according to the changes performed to a given file, respect to its old version.

        Arguments:
        old: the file name of the old block
        new: the file name of the new block
        blk_id: the block identifier (either its index in append-only mode or its nonce in full-r/w mode)
        """

        oldsrc = "%s/%s" % (self._dir, old)
        newsrc = "%s/%s" % (self._dir, new)
        olddst = "%s/.%d.old" % (self._tempdir, blk_id)
        newdst = "%s/%d" % (self._tempdir, blk_id)
        copyfile(oldsrc, olddst)
        copyfile(newsrc, newdst)
        result = self._run("%s --%d --block-size %d --bits %d --quiet --threads %d %s --mode %s --rehash %s %s" %
                           (self._ishakesumd, self._mode, self._block_size, self._output_bits, self._threads,
                            self._profile, self._alg, self._hash, self._tempdir))

        os.remove(olddst)
        os.remove(newdst)
        return result

    def hash(self):
        """Hash the contents of a directory."""
        return self._run("%s --%d --block-size %d --bits %d --quiet --threads %d %s --mode %s %s" %
                         (self._ishakesumd, self._mode, self._block_size, self._output_bits, self._threads,
                          self._profile, self._alg, self._dir))

    @property
    def digest(self):
        """The current digest that has been computed so far."""
        return self._hash


class IShakeAppendOnly(IShakeModes):
    """Append-only (fixed size) mode, allowing to append blocks or update existing blocks."""

    def __init__(self, dir='.', threads=0, profile=False, block_size=1048576, output_bits=2688, mode=128):
        self._echo = self._which('echo')
        super(IShakeAppendOnly, self).__init__('APPEND_ONLY', dir=dir, threads=threads, profile=profile,
                                               block_size=block_size, output_bits=output_bits, mode=mode)

    def append(self, file, idx):
        """Append data from a file with a given index for the new block."""

        # print "append %s %d" % (file, idx)
        src = "%s/%s" % (self._dir, file)
        dst = "%s/.%d.new" % (self._tempdir, idx)
        copyfile(src, dst)
        result = self._run("%s --%d --block-size %d --bits %d --quiet --threads %d %s --mode %s --rehash %s %s" %
                           (self._ishakesumd, self._mode, self._block_size, self._output_bits, self._threads,
                            self._profile, self._alg, self._hash, self._tempdir))
        os.remove(dst)
        return result

    def hash(self, data=''):
        if data:
            return self._run('%s -n \'%s\' | %s --%d --block-size %d --bits %d --quiet --threads %d %s' %
                             (self._echo, data, self._ishakesum, self._mode, self._block_size, self._output_bits,
                              self._threads, self._profile))
        super(IShakeAppendOnly, self).hash()


class IShakeFulLRW(IShakeModes):
    """Full R/W (variable size) mode, allowing to update, insert or delete blocks at any given position."""

    def __init__(self, dir='.', threads=0, profile=False, block_size=1048576, output_bits=2688, mode=128):
        super(IShakeFulLRW, self).__init__('FULL', dir=dir, threads=threads, profile=profile,
                                           block_size=block_size, output_bits=output_bits, mode=mode)

    def insert(self, new, prev=None, next=None):
        """Insert a block from a file, specifying the previous and next blocks in the chain."""

        dst = "%s/." % self._tempdir
        dst += "%s" % (prev if prev else '')
        dst += ".%s." % new
        dst += "%s" % (next if next else '')
        dst += ".new"

        prevdst = "%s/%s" % (self._tempdir, prev)
        if prev:
            copyfile("%s/%s" % (self._dir, prev), prevdst)
        copyfile("%s/%s" % (self._dir, new), dst)
        result = self._run("%s --%d --block-size %d --bits %d --quiet --threads %d %s --mode %s --rehash %s %s" %
                           (self._ishakesumd, self._mode, self._block_size, self._output_bits, self._threads,
                            self._profile, self._alg, self._hash, self._tempdir))

        os.remove(prevdst)
        os.remove(dst)
        return result

    def delete(self, delete, prev=None, next=None):
        """Delete a block corresponding to a file, specifying the previous and next blocks in the chain."""

        dst = "%s/." % self._tempdir
        dst += "%s" % (prev if prev else '')
        dst += ".%s." % delete
        dst += "%s" % (next if next else '')
        dst += ".del"

        prevdst = "%s/%s" % (self._tempdir, prev)
        if prev:
            copyfile("%s/%s" % (self._dir, prev), prevdst)
        copyfile("%s/%s" % (self._dir, delete), dst)
        result = self._run("%s --%d --block-size %d --bits %d --quiet --threads %d %s --mode %s --rehash %s %s" %
                           (self._ishakesumd, self._mode, self._block_size, self._output_bits, self._threads,
                            self._profile, self._alg, self._hash, self._tempdir))

        os.remove(prevdst)
        os.remove(dst)
        return result
