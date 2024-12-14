import os
import ellesmere_tftp

#env__net_static_env_vars = [
    #('ipaddr', '192.168.4.41'),
    #('netmask', '255.255.255.0'),
    #('serverip', '192.168.4.1'),
#]

env__net_uses_pci = True
env__net_dhcp_server = True
env__net_tftp_readable_file = ellesmere_tftp.file2env('u-boot.bin', 0x40400000)
env__efi_loader_helloworld_file = ellesmere_tftp.file2env('helloworld.efi', 0x40400000)
env__efi_loader_grub_file = ellesmere_tftp.file2env('grubaa64.efi', 0x40400000)
env__efi_fit_tftp_file = {
    'addr' : 0x40400000,
    "dn" : os.environ['UBOOT_TRAVIS_BUILD_DIR'],
}
