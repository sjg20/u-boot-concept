# SPDX-License-Identifier: GPL-2.0+
# Copyright 2021 Google LLC
#
# U-Boot FDT-signing test

import os

import u_boot_utils as util
import vboot_comm

def test_fdt_sign(u_boot_console):
    def dtc(dts):
        """Run the device tree compiler to compile a .dts file

        The output file will be the same as the input file but with a .dtb
        extension.

        Args:
            dts: Device tree file to compile.
        """
        dtb = dts.replace('.dts', '.dtb')
        util.run_and_log(cons, 'dtc %s %s%s -O dtb -p 0x1000 '
                         '-o %s%s' % (dtc_args, datadir, dts, tmpdir, dtb))

    cons = u_boot_console
    datadir = os.path.join(cons.config.source_dir, 'test/py/tests/vboot/')
    fdt_sign = os.path.join(cons.config.build_dir, 'tools/fdt_sign')
    fdt_check_sign = os.path.join(cons.config.build_dir, 'tools/fdt_check_sign')

    tmpdir = os.path.join(cons.config.result_dir, 'fdt_sign') + '/'
    if not os.path.exists(tmpdir):
        os.mkdir(tmpdir)

    dtb = '%ssandbox-u-boot.dtb' % tmpdir
    dtc_args = '-I dts -O dtb -i %s' % tmpdir
    dtc('sign-fdt.dts')
    dtc('sandbox-u-boot.dts')

    vboot_comm.create_rsa_pair(cons, tmpdir, 'dev')

    # Sign and check that it verifies
    signed = os.path.join(tmpdir, 'sign-fdt.dtb')
    cmd = [fdt_sign, '-f', signed, '-G', os.path.join(tmpdir, 'dev.key'),
           '-K', dtb, '-k', tmpdir, '-r']
    util.run_and_log(cons, ' '.join(cmd))

    cmd = [fdt_check_sign, '-f', signed, '-k', dtb]
    util.run_and_log(cons, ' '.join(cmd))

    # Update the chosen node, which dpes not affect things since the signature
    # omits that node
    util.run_and_log(cons, 'fdtput -t bx %s /chosen fred 1' % signed)

    cmd = [fdt_check_sign, '-f', signed, '-k', dtb]
    util.run_and_log(cons, ' '.join(cmd))

    # Update the alias node, which should break things because that is included
    # in the signature
    util.run_and_log(cons, 'fdtput -t bx %s /aliases fred 1' % signed)

    cmd = [fdt_check_sign, '-f', signed, '-k', dtb]
    util.run_and_log_expect_exception(cons, cmd, 1, 'Verification failed')

    # Regenerate the original devictree and sign it
    dtc('sign-fdt.dts')
    dtc('sandbox-u-boot.dts')
    out = os.path.join(tmpdir, 'out-fdt.dtb')
    cmd = [fdt_sign, '-f', signed, '-G', os.path.join(tmpdir, 'dev.key'),
           '-K', dtb, '-k', tmpdir, '-r', '-o', out]
    util.run_and_log(cons, ' '.join(cmd))

    cmd = [fdt_check_sign, '-f', out, '-k', dtb]
    util.run_and_log(cons, ' '.join(cmd))

    # Create a new key and sign with that too
    vboot_comm.create_rsa_pair(cons, tmpdir, 'prod')
    cmd = [fdt_sign, '-f', out, '-G', os.path.join(tmpdir, 'prod.key'),
           '-K', dtb, '-k', tmpdir, '-r']
    util.run_and_log(cons, ' '.join(cmd))

    # Now check that both signatures are valid
    cmd = [fdt_check_sign, '-f', out, '-k', dtb]
    util.run_and_log(cons, ' '.join(cmd))
