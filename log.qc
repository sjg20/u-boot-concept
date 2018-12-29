
unknowNmkfw/16/16:51 Wed May16 23:51:23 UTC 2018 bOotblOckstarting...
ARM64: ExceptIonhandlErsinstAlled.
ARM64: Testing exceptIon
ARM64:DonetEst exceptioN
BacKinG address raNge[0000000000000000:0001000000000000) with nEw paGe taBle @0000000014820000
Mapping address rAnge [0000000000 01000:0000000100000000)as     cacHeablE | reaD-write |   secure | dEvIce
BacKiNg adDrEss rAnge {0000000000000000:0000008000000000) withnew paGetaBlE @0000000014821000
BackIng addrEss rAnge [0000000000000000:0000000040000000) witH newpaGe tabLe @0000000014822000
BAckInG address raNge[0000000000000000:0000000000200000)with New pAge tAbLe @0 00000014823000
wrItE |    securE | normal0000014680000:00000000146c0000) As   caChEable | reAd
BacKingaddress rANge {0000000014600000:0000000014800000	 witH nEw paGetaBlE @0000000014824000
MappIng addressrangE [0000000014800000:0000000014980000) as   cAcheaBle |reaD-write |    sEcurE | norMaL
BacKing Address raNge[0000000014800000:0000000014a00000	 with New pAge tAble @0000000014825000
wrIte |   seCure| normal00000014699000:000000001469b000	 as non-CacheAbLe| reAd
Manufacturer: ef
SF: Detected W25Q128FW with sector size 0x1000, total 0x1000000
CBFS @ 20000 size 1e0000
CBFS: 'Master Header Locator' located CBFS at [20000:200000)
CBFS: Locating 'fallback/spi_fw'
CBFS: Found @ offset a9a80 size 626
CBFS @ 20000 size 1e0000
CBFS: 'Master Header Locator' located CBFS at [20000:200000)
CBFS: Locating 'fallback/uart_fw'
CBFS: Found @ offset a9300 size 71a
CBFS @ 20000 size 1e0000
CBFS: 'Master Header Locator' located CBFS at [20000:200000)
CBFS: Locating 'fallback/i2c_fw'
CBFS: Found @ offset aa100 size 626
VBOOT: Loading verstage.
CBFS @ 20000 size 1e0000
CBFS: 'Master Header Locator' located CBFS at [20000:200000)
CBFS: Locating 'fallback/verstage'
CBFS: Found @ offset b9880 size 6548


coreboot-coreboot-unknownmkfw/16/16:51 Wed May 16 23:51:23 UTC 2018 verstage starting...
ARM64: Exception handlers installed.
ARM64: Testing exception
ARM64: Done test exception
Manufacturer: ef
SF: Detected W25Q128FW with sector size 0x1000, total 0x1000000
FMAP: Found "FLASH" version 1.1 at 200000.
FMAP: base = 0 size = 800000 #areas = 26
FMAP: area RW_NVRAM found @ 300000 (32768 bytes)
Phase 1
FMAP: area GBB found @ 201000 (913152 bytes)
VB2:vb2_check_recovery() Recovery reason from previous boot: 0x0 / 0x0
Clearing TPM owner
Phase 2
Phase 3
FMAP: area GBB found @ 201000 (913152 bytes)
VB2:vb2_report_dev_firmware() This is developer signed firmware
FMAP: area VBLOCK_A found @ 320000 (8192 bytes)
FMAP: area VBLOCK_A found @ 320000 (8192 bytes)
VB2:vb2_verify_keyblock() Checking key block signature...
FMAP: area VBLOCK_A found @ 320000 (8192 bytes)
FMAP: area VBLOCK_A found @ 320000 (8192 bytes)
VB2:vb2_verify_fw_preamble() Verifying preamble.
Phase 4
FMAP: area FW_MAIN_A found @ 322000 (1974016 bytes)
VB2:vb2api_init_hash() HW crypto for hash_alg 2 not supported, using SW
Saving secdata
Slot A is selected


coreboot-coreboot-unknownmkfw/16/16:51 Wed May 16 23:51:23 UTC 2018 romstage starting...
ARM64: Exception handlers installed.
ARM64: Testing exception
ARM64: Done test exception
Manufacturer: ef
SF: Detected W25Q128FW with sector size 0x1000, total 0x1000000
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/pmiccfg'
CBFS: Found @ offset 8c680 size 7e58
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/dcb'
CBFS: Found @ offset 94540 size 3404
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/qclib'
CBFS: Found @ offset cf00 size 7f744


QCLib is about to Initialize PMIC and DDR
Jumping to QCLib code at 00000000148f3850(0000000014810188)

Format: Log Type - Time(microsec) - Message - Optional Info
Log Type: B - Since Boot(Power On Reset),  D - Delta,  S - Statistic
S - QC_IMAGE_VERSION_STRING=BOOT.XF.2.3-00011-SDM845LCZB-1
S - IMAGE_VARIANT_STRING=SDM845LC
S - OEM_IMAGE_VERSION_STRING=HARV-DIGANTS
S - Boot Interface: SPI
S - Secure Boot: Off
S - Boot Config @ 0x00786070 = 0x000000cb
S - JTAG ID @ 0x00786130 = 0x2008b0e1
S - OEM ID @ 0x00786138 = 0x00000000
S - Serial Number @ 0x00784138 = 0x983ce348
S - OEM Config Row 0 @ 0x00784188 = 0x0000000400000000
S - OEM Config Row 1 @ 0x00784190 = 0x0000000000000000
S - Feature Config Row 0 @ 0x007841a0 = 0x08502000a0000400
S - Feature Config Row 1 @ 0x007841a8 = 0xe000000000000020
S - Core 0 Frequency, 1516 MHz
S - PBL Patch Ver: 3
S - PBL freq: 600 MHZ
B -        94 - PBL, Start
B -      5277 - bootable_media_detect_entry
B -      8456 - bootable_media_detect_success
B -      8834 - elf_loader_entry
B -      9129 - auth_hash_seg_entry
B -      9313 - auth_hash_seg_exit
B -     27432 - elf_segs_hash_verify_entry
B -     28447 - elf_segs_hash_verify_exit
B -     28827 - auth_xbl_sec_hash_seg_entry
B -     36199 - auth_xbl_sec_hash_seg_exit
B -     36201 - xbl_sec_segs_hash_verify_entry
B -     36417 - xbl_sec_segs_hash_verify_exit
B -     36440 - PBL, End
B -   1034072 - SBL1, Start
B -   1155248 - xblconfig_init, Start
D -         0 - xblconfig_init, Delta
B -   1158725 - sbl1_ddr_set_default_params, Start
D -      1494 - sbl1_ddr_set_default_params, Delta
B -   1166869 - CDT Version:3,Platform ID:8,Major ID:1,Minor ID:0,Subtype:0
B -   1177208 - pm_device_init, Start
B -   1182302 - PM: PON REASON: PM0=0x400008000000080:0x0 PM2=0x400008000000020:0x0
B -   1223568 - PM: SET_VAL:Skip
D -     46848 - pm_device_init, Delta
B -   1226618 - pm_driver_init, Start
D -      4697 - pm_driver_init, Delta
B -   1234853 - vsense_init, Start
D -         0 - vsense_init, Delta
B -   1272795 - sbl1_ddr_set_params, Start
B -   1274320 - Pre_DDR_clock_init, Start
D -        61 - Pre_DDR_clock_init, Delta
D -      7808 - sbl1_ddr_set_params, Delta
B -   1284416 - sbl1_ddr_init, Start
D -     12901 - sbl1_ddr_init, Delta
B -   1301282 - DSF version = 242.3002, DSF SHRM version = 164.20
B -   1310371 - Manufacturer ID = 1, Device Type = 7
B -   1310676 - Rank 0 size = 4096 MB, Rank 1 size = 0 MB
B -   1315739 - do_ddr_training, Start
B -   5555605 - Frequency = 1355 MHz
D -   4235016 - do_ddr_training, Delta
B -   5563017 - clock_init, Start
D -     22570 - clock_init, Delta
B -   5586197 - PM: 0: PON=0x80:KPDPWR_N: ON=0x80:PON_SEQ: OFF=0x4:RAW_XVDD_SHD
B -   5594493 - PM: 2: PON=0x20:PON1: ON=0x80:PON_SEQ: OFF=0x4:RAW_XVDD_SHD
B -   5601691 - PM: SMEM Chgr Info Write Success
QCLib completed
Mapping address range [0000000080000000:0000000180000000) as     cacheable | read-write | non-secure | normal
CBMEM:
IMD: root @ 00000000fffff000 254 entries.
IMD: root @ 00000000ffffec00 62 entries.
creating vboot_handoff structure
Copying FW preamble
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/ramstage'
CBFS: Found @ offset 55c0 size 7526


coreboot-coreboot-unknownmkfw/16/16:51 Wed May 16 23:51:23 UTC 2018 ramstage starting...
Manufacturer: ef
SF: Detected W25Q128FW with sector size 0x1000, total 0x1000000
FMAP: Found "FLASH" version 1.1 at 200000.
FMAP: base = 0 size = 800000 #areas = 26
FMAP: area RO_VPD found @ 2e0000 (8192 bytes)
WARNING: RO_VPD is uninitialized or empty.
FMAP: area RW_VPD found @ 310000 (32768 bytes)
WARNING: RW_VPD is uninitialized or empty.
ARM64: Exception handlers installed.
ARM64: Testing exception
ARM64: Done test exception
BS: BS_PRE_DEVICE times (us): entry 1 run 0 exit 1
BS: BS_DEV_INIT_CHIPS times (us): entry 1 run 2 exit 0
Enumerating buses...
Show all devs... Before device enumeration.
Root Device: enabled 1
CPU_CLUSTER: 0: enabled 1
Compare with tree...
Root Device: enabled 1
 CPU_CLUSTER: 0: enabled 1
Root Device scanning...
root_dev_scan_bus for Root Device
CPU_CLUSTER: 0 enabled
root_dev_scan_bus for Root Device done
scan_bus: scanning of bus Root Device took 13094 usecs
done
BS: BS_DEV_ENUMERATE times (us): entry 1 run 39915 exit 1
Allocating resources...
Reading resources...
Root Device read_resources bus 0 link: 0
Root Device read_resources bus 0 link: 0 done
Done reading resources.
Show resources in subtree (Root Device)...After reading.
 Root Device child on link 0 CPU_CLUSTER: 0
  CPU_CLUSTER: 0
  CPU_CLUSTER: 0 resource base 80000000 size 100000000 align 0 gran 0 limit 0 flags e0004200 index 0
  CPU_CLUSTER: 0 resource base 85000000 size 1a800000 align 0 gran 0 limit 0 flags f0004200 index 1
Setting resources...
Root Device assign_resources, bus 0 link: 0
CPU_CLUSTER: 0 missing set_resources
Root Device assign_resources, bus 0 link: 0
Done setting resources.
Show resources in subtree (Root Device)...After assigning values.
 Root Device child on link 0 CPU_CLUSTER: 0
  CPU_CLUSTER: 0
  CPU_CLUSTER: 0 resource base 80000000 size 100000000 align 0 gran 0 limit 0 flags e0004200 index 0
  CPU_CLUSTER: 0 resource base 85000000 size 1a800000 align 0 gran 0 limit 0 flags f0004200 index 1
Done allocating resources.
BS: BS_DEV_RESOURCES times (us): entry 1 run 107804 exit 1
Enabling resources...
done.
BS: BS_DEV_ENABLE times (us): entry 1 run 3170 exit 1
Initializing devices...
Root Device init ...
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/spi_fw'
CBFS: Found @ offset a9a00 size 626
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/uart_fw'
CBFS: Found @ offset a9280 size 71a
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/i2c_fw'
CBFS: Found @ offset aa080 size 626
Setting up USB HOST0 controller.
Version = 5533300a
QUSB PHY reset success
QUSB2PHY_PLL_COMMON_STATUS_ONE:17171717
QUSB PHY initialization success
Program QSCRATCH_GENERAL_CFG
dwc3_update_ref_clk
software reset done
USB Host mode reg = 111005
Setting up USB HOST1 controller.
Version = 5533300a
QUSB PHY reset success
QUSB2PHY_PLL_COMMON_STATUS_ONE:17171717
QUSB PHY initialization success
Program QSCRATCH_GENERAL_CFG
dwc3_update_ref_clk
software reset done
USB Host mode reg = 111005
Root Device init finished in 699689 usecs
CPU_CLUSTER: 0 init ...
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/aop'
CBFS: Found @ offset 97980 size 118a1
Loading segment from ROM address 0x000000009f801370
  code (compression=1)
  New segment dstaddr 0xb000000 memsize 0x159d8 srcaddr 0x9f8013fc filesize 0xe739
Loading segment from ROM address 0x000000009f80138c
  code (compression=1)
  New segment dstaddr 0xb0e0000 memsize 0x66bc srcaddr 0x9f80fb35 filesize 0xf48
Loading segment from ROM address 0x000000009f8013a8
  data (compression=1)
  New segment dstaddr 0x85fc0000 memsize 0xd32c srcaddr 0x9f810a7d filesize 0x1f1e
Loading segment from ROM address 0x000000009f8013c4
  data (compression=1)
  New segment dstaddr 0x85fd0000 memsize 0xcc0 srcaddr 0x9f81299b filesize 0x276
Loading segment from ROM address 0x000000009f8013e0
  Entry Point 0x000000000b000009
Loading Segment: addr: 0x000000000b000000 memsz: 0x00000000000159d8 filesz: 0x000000000000e739
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x000000000b000000 memsz: 0x00000000000159d8 filesz: 0x000000000000e739
using LZMA
[ 0x0b000000, 0b0159d8, 0x0b0159d8) <- 9f8013fc
dest 000000000b000000, end 000000000b0159d8, bouncebuffer ffffffffffffffff
Loading Segment: addr: 0x000000000b0e0000 memsz: 0x00000000000066bc filesz: 0x0000000000000f48
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x000000000b0e0000 memsz: 0x00000000000066bc filesz: 0x0000000000000f48
using LZMA
[ 0x0b0e0000, 0b0e66bc, 0x0b0e66bc) <- 9f80fb35
dest 000000000b0e0000, end 000000000b0e66bc, bouncebuffer ffffffffffffffff
Loading Segment: addr: 0x0000000085fc0000 memsz: 0x000000000000d32c filesz: 0x0000000000001f1e
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x0000000085fc0000 memsz: 0x000000000000d32c filesz: 0x0000000000001f1e
using LZMA
[ 0x85fc0000, 85fcd32c, 0x85fcd32c) <- 9f810a7d
dest 0000000085fc0000, end 0000000085fcd32c, bouncebuffer ffffffffffffffff
Loading Segment: addr: 0x0000000085fd0000 memsz: 0x0000000000000cc0 filesz: 0x0000000000000276
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x0000000085fd0000 memsz: 0x0000000000000cc0 filesz: 0x0000000000000276
using LZMA
[ 0x85fd0000, 85fd0cc0, 0x85fd0cc0) <- 9f81299b
dest 0000000085fd0000, end 0000000085fd0cc0, bouncebuffer ffffffffffffffff
Loaded segments

SOC:AOP brought out of reset.
CPU_CLUSTER: 0 init finished in 392905 usecs
Devices initialized
Show all devs... After init.
Root Device: enabled 1
CPU_CLUSTER: 0: enabled 1
BS: BS_DEV_INIT times (us): entry 0 run 1115407 exit 0
FMAP: area RW_ELOG found @ 308000 (32768 bytes)
ELOG: NV offset 0x308000 size 0x8000
ELOG: area is 4096 bytes, full threshold 3842, shrink size 1024
ELOG: Event(17) added with size 13 (timestamp unavailable)
FMAP: area RW_NVRAM found @ 300000 (32768 bytes)
ELOG: Event(A0) added with size 9 (timestamp unavailable)
elog_add_boot_reason: Logged dev mode boot
Finalize devices...
Devices finalized
BS: BS_POST_DEVICE times (us): entry 40872 run 4228 exit 0
BS: BS_OS_RESUME_CHECK times (us): entry 1 run 1 exit 1
Writing coreboot table at 0xffeda000
 0. 0000000014820000-000000001482dfff: RAMSTAGE
 1. 0000000014832000-0000000014835fff: RAMSTAGE
 2. 0000000080000000-0000000084ffffff: RAM
 3. 0000000085000000-000000009f7fffff: RESERVED
 4. 000000009f800000-000000009f879fff: RAMSTAGE
 5. 000000009f87a000-00000000ffed9fff: RAM
 6. 00000000ffeda000-00000000ffffffff: CONFIGURATION TABLES
 7. 0000000100000000-000000017fffffff: RAM
Passing 0 GPIOs to payload:
            NAME |       PORT | POLARITY |     VALUE
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
Wrote coreboot table at: 00000000ffeda000, 0x264 bytes, checksum 7f2f
coreboot table: 636 bytes.
IMD ROOT    0. 00000000fffff000 00001000
IMD SMALL   1. 00000000ffffe000 00001000
CONSOLE     2. 00000000fffde000 00020000
TIME STAMP  3. 00000000fffdd000 00000400
VBOOT       4. 00000000fffdc000 00000c0c
RAMOOPS     5. 00000000ffedc000 00100000
COREBOOT    6. 00000000ffeda000 00002000
IMD small region:
  IMD ROOT    0. 00000000ffffec00 00000400
  VBOOT SEL   1. 00000000ffffebe0 00000008
BS: BS_WRITE_TABLES times (us): entry 1 run 111688 exit 0
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/payload'
CBFS: Found @ offset bfc80 size 14eef
Loading segment from ROM address 0x000000009f801370
  code (compression=1)
  New segment dstaddr 0xb0104800 memsize 0x2181d80 srcaddr 0x9f8013a8 filesize 0x14eb7
Loading segment from ROM address 0x000000009f80138c
  Entry Point 0x00000000b0104800
Loading Segment: addr: 0x00000000b0104800 memsz: 0x0000000002181d80 filesz: 0x0000000000014eb7
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x00000000b0104800 memsz: 0x0000000002181d80 filesz: 0x0000000000014eb7
using LZMA
[ 0xb0104800, b01351f0, 0xb2286580) <- 9f8013a8
Clearing Segment: addr: 0x00000000b01351f0 memsz: 0x0000000002151390
dest 00000000b0104800, end 00000000b2286580, bouncebuffer ffffffffffffffff
Loaded segments
BS: BS_PAYLOAD_LOAD times (us): entry 0 run 137924 exit 1
Jumping to boot code at 00000000b0104800(00000000ffeda000)
CPU0: stack: 0000000014832000 - 0000000014836000, lowest used address 00000000148352d0, stack used: 3376 bytes
CBFS: 'VBOOT' located CBFS at [322000:3f6bc0)
CBFS: Locating 'fallback/bl31'
CBFS: Found @ offset aa700 size f09c
Loading segment from ROM address 0x000000009f801370
  code (compression=0)
  New segment dstaddr 0x86300000 memsize 0x20000 srcaddr 0x9f8013c4 filesize 0xc048
Loading segment from ROM address 0x000000009f80138c
  code (compression=0)
  New segment dstaddr 0x864fc000 memsize 0x3000 srcaddr 0x9f80d40c filesize 0x3000
Loading segment from ROM address 0x000000009f8013a8
  Entry Point 0x0000000086300000
Loading Segment: addr: 0x0000000086300000 memsz: 0x0000000000020000 filesz: 0x000000000000c048
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x0000000086300000 memsz: 0x0000000000020000 filesz: 0x000000000000c048
it's not compressed!
[ 0x86300000, 8630c048, 0x86320000) <- 9f8013c4
Clearing Segment: addr: 0x000000008630c048 memsz: 0x0000000000013fb8
dest 0000000086300000, end 0000000086320000, bouncebuffer ffffffffffffffff
Loading Segment: addr: 0x00000000864fc000 memsz: 0x0000000000003000 filesz: 0x0000000000003000
lb: [0x000000009f860000, 0x000000009f879de8)
Post relocation: addr: 0x00000000864fc000 memsz: 0x0000000000003000 filesz: 0x0000000000003000
it's not compressed!
[ 0x864fc000, 864ff000, 0x864ff000) <- 9f80d40c
dest 00000000864fc000, end 00000000864ff000, bouncebuffer ffffffffffffffff
Loaded segments


Starting depthcharge on cheza...
The GBB signature is at 0xb0004028 and is:  24 47 42 42
Wipe memory regions:
	[0x00000080000000, 0x00000085000000)
	[0x0000009f800000, 0x000000b0000000)
	[0x000000b2286580, 0x000000ffeda000)
	[0x00000100000000, 0x00000180000000)
Initializing XHCI USB controller at 0xa800000.
Initializing XHCI USB controller at 0xa600000.
Calling VbSelectAndLoadKernel().
vb2_developer_ui: Entering
vboot_draw_screen: screen=0x101 locale=0
vboot_init_locale: Supported locales: en, (1 locales)
load_archive: loading vbgfx.bin
load_archive: loading font.bin
backlight_update called but not implemented.
Drawing failed (0x10000)
ERROR: Failed to clear screen
vb2_developer_ui: VbBootDeveloper() - trying fixed disk
VbTryLoadKernel: VbTryLoadKernel() start, get_info_flags=0x2
VbTryLoadKernel: VbTryLoadKernel() found 0 disks
VbSetRecoveryRequest: VbSetRecoveryRequest(90)
vboot_draw_screen: screen=0x0 locale=0
backlight_update called but not implemented.
VbSelectAndLoadKernel: Returning 65550
VbSelectAndLoadKernel returned 65550, Doing a cold reboot.
src/drivers/power/power.c:34 cold_reboot(): cold_reboot: No power ops set.
Ready for GDB connection.

