# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
#
# Common functions

import u_boot_utils as util

def create_rsa_pair(cons, tmpdir, name):
    """Generate a new RSA key pair and certificate

    Args:
        name: Name of the key (e.g. 'dev')
    """
    public_exponent = 65537
    util.run_and_log(cons, 'openssl genpkey -algorithm RSA -out %s%s.key '
                 '-pkeyopt rsa_keygen_bits:2048 '
                 '-pkeyopt rsa_keygen_pubexp:%d' %
                 (tmpdir, name, public_exponent))

    # Create a certificate containing the public key
    util.run_and_log(cons, 'openssl req -batch -new -x509 -key %s%s.key '
                     '-out %s%s.crt' % (tmpdir, name, tmpdir, name))
