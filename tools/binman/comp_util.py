# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Copyright (C) 2022 Weidmüller Interface GmbH & Co. KG
# Stefan Herbrechtsmeier <stefan.herbrechtsmeier@weidmueller.com>
#
"""Utilities to compress and decompress data"""

import tempfile

from binman import bintool
from patman import tools

# Supported compressions
COMPRESSIONS = ['bzip2', 'gzip', 'lz4', 'lzma', 'lzo', 'xz']

bintools = {}

def _get_tool_name(algo):
    names = {'lzma': 'lzma_alone', 'lzo': 'lzop'}
    return names.get(algo, algo)

def _get_tool(algo):
    global bintools
    name = _get_tool_name(algo)
    tool = bintools.get(name)
    if not tool:
        tool = bintool.Bintool.create(name)
        bintools[name] = tool
    return tool

def compress(indata, algo):
    """Compress some data using a given algorithm

    Note that for lzma this uses an old version of the algorithm, not that
    provided by xz.

    This requires 'bzip2', 'gzip', 'lz4', 'lzma_alone' 'lzop' and 'xz' tools.
    It also requires an output directory to be previously set up, by calling
    PrepareOutputDir().

    Args:
        indata (bytes): Input data to compress
        algo (str): Algorithm to use ('none', 'bzip2', 'gzip', 'lz4', 'lzma',
                    'lzo' or 'xz')

    Returns:
        bytes: Compressed data
    """
    if algo == 'none':
        return indata
    if algo not in COMPRESSIONS:
        raise ValueError("Unknown algorithm '%s'" % algo)

    tool = _get_tool(algo)
    data = tool.compress(indata)

    return data

def decompress(indata, algo):
    """Decompress some data using a given algorithm

    Note that for lzma this uses an old version of the algorithm, not that
    provided by xz.

    This requires 'bzip2', 'gzip', 'lz4', 'lzma_alone', 'lzop' and 'xz' tools.
    It also requires an output directory to be previously set up, by calling
    PrepareOutputDir().

    Args:
        indata (bytes): Input data to decompress
        algo (str): Algorithm to use ('none', 'bzip2', 'gzip', 'lz4', 'lzma',
                    'lzo' or 'xz')

    Returns:
        (bytes) Compressed data
    """
    if algo == 'none':
        return indata
    if algo not in COMPRESSIONS:
        raise ValueError("Unknown algorithm '%s'" % algo)

    tool = _get_tool(algo)
    data = tool.decompress(indata)

    return data

def is_present(algo):
     tool = _get_tool(algo)
     return tool.is_present()
