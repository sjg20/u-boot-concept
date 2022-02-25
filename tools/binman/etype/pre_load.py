# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2022 Softathome
# Written by Philippe Reynes <philippe.reynes@softathome.com>
#
# Entry-type for the global header
#

import struct
from dtoc import fdt_util
from patman import tools

from binman.entry import Entry
from binman.etype.collection import Entry_collection
from binman.entry import EntryArg

from Cryptodome.Hash import SHA256, SHA384, SHA512
from Cryptodome.PublicKey import RSA
from Cryptodome.Signature import pkcs1_15
from Cryptodome.Signature import pss

PRE_LOAD_MAGIC = b'UBSH'

RSAS = {
    'rsa1024': 1024 / 8,
    'rsa2048': 2048 / 8,
    'rsa4096': 4096 / 8
}

SHAS = {
    'sha256': SHA256,
    'sha384': SHA384,
    'sha512': SHA512
}

class Entry_pre_load(Entry_collection):
    """Pre load image header

    Properties / Entry arguments:
        - key-path: Path of the directory that store key (provided by the environment variable KEY_PATH)
        - content: List of phandles to entries to sign
        - algo-name: Hash and signature algo to use for the signature
        - padding-name: Name of the padding (pkcs-1.5 or pss)
        - key-name: Filename of the private key to sign
        - header-size: Total size of the header
        - version: Version of the header

    This entry create a pre-load header that contain a global
    image signature.

    For example, this creates an image with a pre-load header and a binary::

        binman {
	    image2 {
		filename = "sandbox.bin";

		pre-load {
		    content = <&image>;
		    algo-name = "sha256,rsa2048";
		    padding-name = "pss";
		    key-name = "private.pem";
		    header-size = <4096>;
		    version = <1>;
		};

		image: blob-ext {
		    filename = "sandbox.itb";
		};
	    };
        };
    """

    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self.algo_name = fdt_util.GetString(self._node, 'algo-name')
        self.padding_name = fdt_util.GetString(self._node, 'padding-name')
        self.key_name = fdt_util.GetString(self._node, 'key-name')
        self.header_size = fdt_util.GetInt(self._node, 'header-size')
        self.version = fdt_util.GetInt(self._node, 'version')

    def _CreateHeader(self):
        """Create a pre load header"""
        hash_name, sign_name  = self.algo_name.split(',')
        padding_name=self.padding_name
        key_path, = self.GetEntryArgsOrProps([EntryArg('key-path', str)])
        if key_path is None or key_path == "":
            key_name = self.key_name
        else:
            key_name = key_path + "/" + self.key_name

        # Check hash and signature name/type
        if hash_name not in SHAS:
            raise ValueError(hash_name + " is not supported")
        if sign_name not in RSAS:
            raise ValueError(sign_name + "is not supported")

        # Read the key
        with open(key_name, 'rb') as pem:
            key = RSA.import_key(pem.read())

        # Check if the key has the expected size
        if key.size_in_bytes() != RSAS[sign_name]:
            raise ValueError("The key " + self.key_name + " don't have the expected size")

        # Compute the hash
        hash_image = SHAS[hash_name].new()
        hash_image.update(self.image)

        # Compute the signature
        if padding_name is None:
            padding_name = "pkcs-1.5"
        if padding_name == "pss":
            salt_len = key.size_in_bytes() - hash_image.digest_size - 2
            padding = pss
            padding_args = {'salt_bytes': salt_len}
        elif padding_name == "pkcs-1.5":
            padding = pkcs1_15
            padding_args = {}
        else:
            raise ValueError(padding_name + " is not supported")

        sig = padding.new(key, **padding_args).sign(hash_image)

        hash_sig = SHA256.new()
        hash_sig.update(sig)

        version = self.version
        header_size = self.header_size
        image_size = len(self.image)
        ofs_img_sig = 64 + len(sig)
        flags = 0
        reserved0 = 0
        reserved1 = 0

        first_header = bytearray(64)
        struct.pack_into('4s', first_header, 0, PRE_LOAD_MAGIC)
        struct.pack_into('>I', first_header, 4, version)
        struct.pack_into('>I', first_header, 8, header_size)
        struct.pack_into('>I', first_header, 12, image_size)
        struct.pack_into('>I', first_header, 16, ofs_img_sig)
        struct.pack_into('>I', first_header, 20, flags)
        struct.pack_into('>I', first_header, 24, reserved0)
        struct.pack_into('>I', first_header, 28, reserved1)
        struct.pack_into('32s', first_header, 32, hash_sig.digest())

        hash_first_header = SHAS[hash_name].new()
        hash_first_header.update(first_header)
        sig_first_header = padding.new(key, **padding_args).sign(hash_first_header)

        data = first_header + sig_first_header + sig
        pad  = bytearray(self.header_size - len(data))

        return data + pad

    def ObtainContents(self):
        """Obtain a placeholder for the header contents"""
        # wait that the image is available
        self.image = self.GetContents(False)
        if self.image is None:
            return False
        self.SetContents(self._CreateHeader())
        return True

    def ProcessContents(self):
        data = self._CreateHeader()
        return self.ProcessContentsUpdate(data)
