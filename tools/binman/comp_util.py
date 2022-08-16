# SPDX-License-Identifier: GPL-2.0+
# Copyright 2022 Google LLC
# Copyright (C) 2022 Weidmüller Interface GmbH & Co. KG
# Stefan Herbrechtsmeier <stefan.herbrechtsmeier@weidmueller.com>
#
"""Utilities to compress and decompress data

This supports the following compression algorithm:
  none
  bzip2
  gzip
  lz4
  lzma

Note that for lzma this uses an old version of the algorithm, not that
provided by xz.

This requires the following tools:
  bzip2
  gzip
  lz4
  lzma_alone

It also requires an output directory to be previously set up, by calling
PrepareOutputDir().
"""

import tempfile

from binman import bintool
from patman import tools

# Supported compression algorithms
ALGORITHMS = ['bzip2', 'gzip', 'lz4', 'lzma']

bintools = {}

def _get_tool_name(algo):
    """Get the tool name of a compression algorithm

    Args:
        algo (str): Algorithm to use

    Returns:
        str: Tool name
    """
    names = {'lzma': 'lzma_alone'}
    return names.get(algo, algo)

def _get_tool(algo):
    """Get the bintool object of a compression algorithm

    The function creates new bintool object on demand per compression algorithm
    and save it in a global bintools dictionary.

    Args:
        algo (str): Algorithm to use

    Returns:
        A bintool object for the compression algorithm
    """
    global bintools
    name = _get_tool_name(algo)
    tool = bintools.get(name)
    if not tool:
        tool = bintool.Bintool.create(name)
        bintools[name] = tool
    return tool

def compress(indata, algo):
    """Compress some data using a given algorithm

    Args:
        indata (bytes): Input data to compress
        algo (str): Algorithm to use

    Returns:
        bytes: Compressed data
    """
    if algo == 'none':
        return indata
    if algo not in ALGORITHMS:
        raise ValueError("Unknown algorithm '%s'" % algo)

    tool = _get_tool(algo)
    data = tool.compress(indata)

    return data

def decompress(indata, algo):
    """Decompress some data using a given algorithm

    Args:
        indata (bytes): Input data to decompress
        algo (str): Algorithm to use

    Returns:
        bytes: Decompressed data
    """
    if algo == 'none':
        return indata
    if algo not in ALGORITHMS:
        raise ValueError("Unknown algorithm '%s'" % algo)

    tool = _get_tool(algo)
    data = tool.decompress(indata)

    return data

def is_present(algo):
     tool = _get_tool(algo)
     return tool.is_present()
