import ellesmere_tftp

env__net_uses_usb = True

env__net_dhcp_server = True

env__tftp_boot_test_skip = False

env__net_tftp_bootable_file = {
    'fn': 'v6.6/image.fit.nocomp',
    'addr': 0x00200000,
    'size': 85984256,
    'crc32': '754c839a',
    'pattern': 'Linux',
    'config': 'conf-852',
}

# Details regarding a file that may be read from a TFTP server. This variable
# may be omitted or set to None if PXE testing is not possible or desired.
env__net_pxe_bootable_file = {
    'fn': 'default',
    'addr': 0x00200000,
    'size': 64,
    'timeout': 50000,
    'pattern': 'Linux',
    'valid_label': '1',
    'invalid_label': '2',
    'exp_str_invalid': 'Skipping install for failure retrieving',
    'local_label': '3',
    'exp_str_local': 'missing environment variable: localcmd',
    'empty_label': '4',
    'exp_str_empty': 'No kernel given, skipping boot',
}

# False or omitted if a PXE boot test should be tested.
# If PXE boot testing is not possible or desired, set this variable to True.
# For example: If pxe configuration file is not proper to boot
env__pxe_boot_test_skip = False
