#!/usr/bin/env python

import glob
import os
import re
import subprocess
import tempfile

basedir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)));
files = glob.glob('%s/tests/*/*.rsp' % basedir)
cmds = {
    'shake': '%s/%s' % (basedir, 'bin/sha3sum %alg% --hex --quiet --bytes %bytes% %file%'),
    'ishake': '%s/%s' % (basedir, 'bin/ishakesum %alg% --hex --quiet --bits %bits% %file%'),
}
algs = {
    'SHAKE128': '--shake128',
    'SHAKE256': '--shake256',
    'iSHAKE128': '--128',
    'iSHAKE256': '--256',
}
patterns = ['%alg%', '%bytes%', '%bits%', '%file%']

def processFile(file):
    [alg, ver] = re.match('%s/tests/(\w+)/([a-zA-Z]+\d+).*' % basedir, file).groups()
    cmd = cmds[alg]
    bytes = 0
    bits = 0
    inlen = 0
    msg = ''
    out = ''

    fd = open(file, 'r')

    print "Processing %s..." % file

    for line in iter(fd.readline, b''):
        # discard lines we don't care about
        line = line.strip()
        if not line \
           or line[0] == '#' \
           or line.find('COUNT = ') == 0 \
           or line.find('[Tested for') == 0 \
           or line.find('[Minimum Output Length (bits) =') == 0 \
           or line.find('[Maximum Output Length (bits) =') == 0:
            continue

        m = re.match('^\[?Outputlen = (\d+)\]?$', line)
        if m:
            bits = m.group(1)
            bytes = str(int(bits) / 8)
            continue

        m = re.match('^Len = (\d+)$', line)
        if m:
            inlen = (int(m.group(1)) / 8) * 2
            continue

        m = re.match('^\[Input Length = (\d+)\]$', line)
        if m:
            inlen = (int(m.group(1)) / 8) * 2
            continue

        m = re.match('^Msg = (\w+).*', line)
        if m:
            msg = m.group(1)
            continue

        m = re.match('^Output = (\w+)$', line)
        if m:
            expected = m.group(1)
            msg = msg[0:inlen]
            tmp_f = tempfile.NamedTemporaryFile(prefix='shake-build-', delete=False)
            tmp_f.write(msg)
            replacements = [algs[ver], bytes, bits, tmp_f.name]
            for op in zip(patterns, replacements):
                cmd = re.sub(op[0], op[1], cmd)

            # commit contents to file
            tmp_f.close()

            computed = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE).stdout.read().strip()

            # discard file
            os.unlink(tmp_f.name)

            if expected != computed:
                print "Failure: %s (expecting '%s', got '%s'" % (cmd, expected, computed)
                fd.close()
                return False

            cmd = cmds[alg]
    fd.close()

    return True


if __name__ == '__main__':
    success = True
    for file in files:
        success = success and processFile(file)

    exit(0 if success else -1)
