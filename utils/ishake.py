#!/usr/bin/env python

import fire
from ishakelib import IShakeAppendOnly, IShakeFulLRW


class IShakeCommand(object):
    """Command line utility to use the iSHAKE hash algorithm."""

    def __init__(self):
        fire.Fire(self, name='ishake')

    append_only = IShakeAppendOnly
    full_rw = IShakeFulLRW


if __name__ == '__main__':
    IShakeCommand()
