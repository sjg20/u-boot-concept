# SPDX-License-Identifier: GPL-2.0+
# Copyright (c) 2023 Weidmueller GmbH
# Written by Lukas Funke <lukas.funke@weidmueller.com>
#
# Entry-type module for signed ZynqMP boot images (boot.bin)
#

import tempfile

from collections import OrderedDict

from binman import elf
from binman.entry import Entry

from dtoc import fdt_util

from u_boot_pylib import tools
from u_boot_pylib import command

# pylint: disable=C0103
class Entry_xilinx_fsbl_auth(Entry):
    """Authenticated SPL for booting Xilinx ZynqMP devices

    Properties / Entry arguments:
        - auth-params: (Optional) Authentication parameters passed to bootgen
        - fsbl-config: (Optional) FSBL parameters passed to bootgen
        - keysrc-enc: (Optional) Key source when using decryption engine
        - pmufw-filename: Filename of PMU firmware. Default: pmu-firmware.elf
        - psk-filename: Filename of primary public key
        - ssk-filename: Filename of secondary public key

    The following example builds an authenticated boot image. The fuses of
    the primary public key (ppk) should be fused together with the RSA_EN flag.

    Example node::

        spl {
            filename = "boot.signed.bin";

            xilinx-fsbl-auth {
                psk-filename = "psk0.pem";
                ssk-filename = "ssk0.pem";
                auth-params = "ppk_select=0", "spk_id=0x00000000";

                u-boot-spl-nodtb {
                };
                u-boot-spl-pubkey-dtb {
                    algo = "sha384,rsa4096";
                    required = "conf";
                    key-name = "dev";
                };
            };
        };

    For testing purposes, e.g. if no RSA_EN should be fused, one could add
    the "bh_auth_enable" flag in the fsbl-config field. This will skip the
    verification of the ppk fuses and boot the image, even if ppk hash is
    invalid.

    Example node::

        xilinx-fsbl-auth {
            psk-filename = "psk0.pem";
            ssk-filename = "ssk0.pem";
            ...
            fsbl-config = "bh_auth_enable";
            ...
        };
    """
    def __init__(self, section, etype, node):
        super().__init__(section, etype, node)
        self._auth_params = None
        self._entries = OrderedDict()
        self._filename = None
        self._fsbl_config = None
        self._keysrc_enc = None
        self._pmufw_filename = None
        self._psk_filename = None
        self._ssk_filename = None
        self.align_default = None
        self.bootgen = None
        self.required_props = ['psk-filename', 'ssk-filename']

    def ReadNode(self):
        """Read properties from the xilinx_fsbl_auth node"""
        super().ReadNode()
        self._auth_params = fdt_util.GetStringList(self._node,
                                                   'auth-params')
        self._filename = fdt_util.GetString(self._node, 'filename')
        self._fsbl_config = fdt_util.GetStringList(self._node,
                                                   'fsbl-config')
        self._keysrc_enc = fdt_util.GetString(self._node,
                                                   'keysrc-enc')
        self._pmufw_filename = fdt_util.GetString(self._node,
                                                  'pmufw-filename',
                                                  'pmu-firmware.elf')
        self._psk_filename = fdt_util.GetString(self._node, 'psk-filename',
                                            'psk.pem')
        self._ssk_filename = fdt_util.GetString(self._node, 'ssk-filename',
                                            'ssk.pem')
        self.ReadEntries()

    def ReadEntries(self):
        """Read the subnodes to find out what should go in this image"""
        for node in self._node.subnodes:
            entry = Entry.Create(self, node)
            entry.ReadNode()
            self._entries[entry.name] = entry

    @classmethod
    def __ToElf(self, data, output_fname):
        """ Convert SPL object file to bootable ELF file.

        Args:
            data (bytearray): u-boot-spl-nodtb + u-boot-spl-pubkey-dtb obj file
                                data
            output_fname (str): Filename of converted FSBL ELF file
        """
        platform_elfflags = []

        gcc, args = tools.get_target_compile_tool('cc')
        args += ['-dumpmachine']
        stdout = command.output(gcc, *args)
        # split target machine triplet (arch, vendor, os)
        arch, _, _ = stdout.split('-')

        if arch == 'aarch64':
            platform_elfflags = ["-B", "aarch64", "-O", "elf64-littleaarch64"]
        elif arch == 'x86_64':
            # amd64 support makes no sense for the target platform, but we
            # include it here to enable testing on hosts
            platform_elfflags = ["-B", "i386", "-O", "elf64-x86-64"]

        spl_text_base = hex(elf.GetSymbolAddress(
            tools.get_input_filename('spl/u-boot-spl'), ".text"))

        # Obj file to swap data and text section (rename-section)
        with tempfile.NamedTemporaryFile(prefix="u-boot-spl-pubkey-",
                                    suffix=".o.tmp",
                                    dir=tools.get_output_dir())\
                                    as tmp_obj:
            input_objcopy_fname = tmp_obj.name
            # Align packed content to 4 byte boundary
            pad = bytearray(tools.align(len(data), 4) - len(data))
            tools.write_file(input_objcopy_fname, data + pad)
            # Final output elf file which contains a valid start address
            with tempfile.NamedTemporaryFile(prefix="u-boot-spl-pubkey-elf-",
                                            suffix=".o.tmp",
                                            dir=tools.get_output_dir())\
                                                as tmp_elf_obj:
                input_ld_fname = tmp_elf_obj.name
                objcopy, args = tools.get_target_compile_tool('objcopy')
                args += ["--rename-section", ".data=.text",
                        "-I", "binary"]
                args += platform_elfflags
                args += [input_objcopy_fname, input_ld_fname]
                command.run(objcopy, *args)

                ld, args = tools.get_target_compile_tool('ld')
                args += [input_ld_fname, '-o', output_fname,
                         "--defsym", f"_start={spl_text_base}",
                         "-Ttext", spl_text_base]
                command.run(ld, *args)

    def ObtainContents(self, skip_entry=None, fake_size=0):
        """ Pack node content, and create bootable, signed ZynqMP boot image

        The method collects the content of this node (usually SPL + dtb) and
        converts them to an ELF file. The ELF file is passed to the
        Xilinx bootgen tool which packs the SPL ELF file together with
        Platform Management Unit (PMU) firmware into a bootable image
        for ZynqMP devices. The image is signed within this step.

        The result is a bootable, authenticated SPL image for Xilinx ZynqMP
        devices.

        """
        bootbin_fname = self._filename if self._filename else \
                            tools.get_output_filename(
                            f'boot.{self.GetUniqueName()}.bin')

        pmufw_elf_fname = tools.get_input_filename(self._pmufw_filename)
        psk_fname = tools.get_input_filename(self._psk_filename)
        ssk_fname = tools.get_input_filename(self._ssk_filename)
        fsbl_config = ";".join(self._fsbl_config) if self._fsbl_config else None
        auth_params = ";".join(self._auth_params) if self._auth_params else None

        spl_elf_fname = tools.get_output_filename('u-boot-spl-pubkey.dtb.elf')

        # Collect node contents. This is usually the SPL concatenated
        # with the SPL dtb (device tree).
        data, _, _ = self.collect_contents_to_file(
                                self._entries.values(), 'spl')

        # We need to convert to node content (see above) into an ELF
        # file in order to be processed by bootgen.
        self.__ToElf(bytearray(data), spl_elf_fname)

        # Call Bootgen in order to sign the SPL
        self.bootgen.sign('zynqmp', spl_elf_fname, pmufw_elf_fname,
                        psk_fname, ssk_fname, fsbl_config,
                        auth_params, self._keysrc_enc, bootbin_fname)

        self.SetContents(tools.read_file(bootbin_fname))

        return True

    # pylint: disable=C0116
    def AddBintools(self, btools):
        super().AddBintools(btools)
        for entry in self._entries.values():
            entry.AddBintools(btools)
        self.bootgen = self.AddBintool(btools, 'bootgen')
