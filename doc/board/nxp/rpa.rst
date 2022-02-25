i.MX8M DDR RPA Register Programming Aid
=======================================

Introduction
------------
The Synopsys/Designware DDR controller (*) present in i.MX8M series SoCs
uses special procedure for initial DRAM calibration during bring up. The
vendor procedure depends on access to non-free OS, the procedure below
does not.

* Also used in Allwinner, Altera N5X, ST STM32MP1, Rockchip, TI K3,
  Xilinx ZynqMP, each of which has unique custom driver for this IP.

Prerequisites
-------------
An RPA document is mandatory thus far, the document is used to generate
a list of registers used by the calibrator blob. The RPA document does
not support all possible system configurations, especially DDR DRAM PLL
settings and UART IOMUXC settings are not supported for all possible
working configurations.

RPA document is available at the following locations and can be opened
in e.g. LibreOffice:

   - i.MX8M
     https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/i-MX8M-m850D-DDR-Register-Programming-Aid-RPA/ta-p/1172441
   - i.MX8M Mini
     https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/i-MX8MMini-m845S-DDR-Register-Programming-Aid-RPA/ta-p/1172443
   - i.MX8M Nano
     https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/i-MX8MNano-m815S-DDR-Register-Programming-Aid-RPA/ta-p/1172444
   - i.MX8M Plus
     https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/i-MX-8MPlus-m865S-DDR-Register-Programming-Aids-RPA/ta-p/1235352

Calibrator blobs are mandatory to perform the on-board calibration of
the DRAM and are no longer used afterward. The calibrator blobs are
available in archive 'mscale_ddr_tool _v3.30_setup.exe.zip' or newer
at the following URL:
  https://community.nxp.com/t5/i-MX-Processors-Knowledge-Base/i-MX-8M-Family-DDR-Tool-Release/ta-p/1104467

RPA document
------------
This document contains multiple tabs, follow the "How To Use" tab instructions.
This means "Register Configuration" tab and "BoardDataBusConfig" tabs must be
filled in correct per selected DRAM datasheet. The end goal if to obtain list
of registers in "DDR stress test file" tab, this list of registers must be
copied and pasted to a text file 'rpa.txt' for later use in this documentation.

Note that each "Debug UART port" setting in "Register Configuration" tab can
map to multiple IOMUXC settings, verify "step 0: configure debug uart port."
section of "DDR stress test file" tab and perform IOMUXC adjustment if needed.
Example for i.MX8M Plus UART1 muxed on custom pins::

  memory set 0x303301A0 32 0x00000004 #IOMUXC_SW_MUX_SAI2_RXC
  memory set 0x3033019C 32 0x00000004 #IOMUXC_SW_MUX_SAI2_RXFS
  memory set 0x30330400 32 0x00000049 #IOMUXC_SW_PAD_SAI2_RXC
  memory set 0x303303FC 32 0x00000049 #IOMUXC_SW_PAD_SAI2_RXFS
  memory set 0x303305E8 32 0x00000003 #IOMUXC_SW_MUX_UART1_SEL_RXD
  sysparam set debug_uart 0 #UART index from 0 ('0' = UART1, '1' = UART2, '2' = UART3, '3' = UART4)

Note that not all DRAM PLL settings are supported. Many DRAM frequencies and/or
data rates are not supported and must be manually configured instead. In case
DRAM PLL frequency required by DRAM is not supported, "DDR stress test file"
tab 'DRAM_PLL_FDIV_CTL0' row will contain value 'TBD'. U-Boot supports far more
DRAM PLL frequencies than the RPA document, supported frequencies can be found
in U-Boot source:
- 'drivers/ddr/imx/imx8m/ddrphy_utils.c' 'imx8mm_fracpll_tbl()'
- 'arch/arm/mach-imx/imx8m/clock_imx8mm.c' 'ddrphy_init_set_dfi_clk()'
The DRAM PLL frequency entries in U-Boot source map to the following::


  PLL_1443X_RATE(pll_frequency, m, p, s, 0)
  pll_frequency (MHz) = ((24 / p) * m) / (1 << s)

The data rate in MT/s is 4x 'pll_frequency', so e.g. 933 MHz DRAM PLL setting
leads to 1866 MHz DRAM clock and 3733 MT/s data rate and 'TBD' value '0x137041'.

Calibrator blob
---------------
The calibrator blobs must be extracted from mscale ddr tool archive, the file
name is 'mscale_ddr_tool _v3.30_setup.exe.zip' or similar, the space in file
name is not a typo in this documentation. The archive can be extracted as
follows::

  $ unzip "mscale_ddr_tool _v3.30_setup.exe.zip"
  $ 7z x "mscale_ddr_tool _v3.30_setup.exe"

The relevant blobs become available in 'mscale_ddr_tool _v3.30/bin' .

The i.MX8M Plus using LPDDR4 DRAM requires the following blobs:
   - lpddr4_dmem_1d_v202006.bin
   - lpddr4_dmem_2d_v202006.bin
   - lpddr4_imem_1d_v202006.bin
   - lpddr4_imem_2d_v202006.bin
   - lpddr4_train1d_string_v202006.bin
   - lpddr4_train2d_string_v202006.bin
   - m865_ddr_stress_test.bin

Bootable flash.bin
------------------
To turn aforementioned 'rpa.txt' RPA document output and calibrator blob into
bootable 'flash.bin' i.MX image, perform the following steps.

First, convert 'rpa.txt' into 'rpa.bin' using 'rpa2bin.sh' script. This script
is currently tested on i.MX8M Plus only::

  rpa2bin.sh rpa.txt rpa.bin

Second, pad blobs and assemble combined 'loader.bin' blob for mkimage. The blob
load addresses on i.MX8M Plus with LPDDR4 DRAM are as follows:
   - 0x920000 - m865_ddr_stress_test.bin
   - 0x940000 - lpddr4_train1d_string_v202006.bin
   - 0x947000 - lpddr4_train2d_string_v202006.bin
   - 0x94fc00 - lpddr4_imem_1d_v202006.bin
   - 0x958400 - lpddr4_imem_2d_v202006.bin
   - 0x960400 - lpddr4_dmem_1d_v202006.bin
   - 0x960800 - lpddr4_dmem_1d_v202006.bin
   - 0x960c00 - rpa.bin

::

  $ cp m865_ddr_stress_test.bin m865_ddr_stress_test-pad.bin
  $ truncate -s 131072 m865_ddr_stress_test-pad.bin

  $ objcopy -I binary -O binary --pad-to 0x7000 --gap-fill=0x0 lpddr4_train1d_string_v202006.bin lpddr4_pmu_train_1d_string_pad.bin
  $ objcopy -I binary -O binary --pad-to 0x8c00 --gap-fill=0x0 lpddr4_train2d_string_v202006.bin lpddr4_pmu_train_2d_string_pad.bin
  $ objcopy -I binary -O binary --pad-to 0x8000 --gap-fill=0x0 lpddr4_imem_1d_v202006.bin lpddr4_pmu_train_1d_imem_pad.bin
  $ objcopy -I binary -O binary --pad-to 0x800 --gap-fill=0x0 lpddr4_dmem_1d_v202006.bin lpddr4_pmu_train_1d_dmem_pad.bin
  $ objcopy -I binary -O binary --pad-to 0x8000 --gap-fill=0x0 lpddr4_imem_2d_v202006.bin lpddr4_pmu_train_2d_imem_pad.bin
  $ objcopy -I binary -O binary --pad-to 0x800 --gap-fill=0x0 lpddr4_dmem_2d_v202006.bin lpddr4_pmu_train_2d_dmem_pad.bin

  $ cat m865_ddr_stress_test-pad.bin lpddr4_pmu_train_1d_string_pad.bin lpddr4_pmu_train_2d_string_pad.bin lpddr4_pmu_train_1d_imem_pad.bin lpddr4_pmu_train_1d_dmem_pad.bin lpddr4_pmu_train_2d_imem_pad.bin lpddr4_pmu_train_2d_dmem_pad.bin rpa.bin > loader.bin

Third, assemble bootable 'flash.bin' from 'loader.bin' using the following
imximage.cfg::

  ROM_VERSION v2
  BOOT_FROM sd
  LOADER loader.bin 0x920000

Use mkimage to generate 'flash.bin'::

  $ mkimage -n imximage.cfg -T imx8mimage -e 0x920000 -d loader.bin flash.bin

Calibrator serial protocol
--------------------------
The calibrator tool uses binary serial protocol composed of 5-byte long commands:
   - '85 00 00 00 00' - Connect, Target=MX8M-plus, Clock=Default, DDR=LPDDR4, Density=Default

     - The first byte is Density in 256 MiB increments above 256 MiB:

       - '85 20 00 00 00' - Density=32 MiB
       - '85 40 00 00 00' - Density=64 MiB
       - '85 80 00 00 00' - Density=128 MiB
       - '85 01 00 00 00' - Density=256 MiB
       - '85 02 00 00 00' - Density=512 MiB
       - '85 04 00 00 00' - Density=1 GiB
       - '85 08 00 00 00' - Density=2 GiB
       - '85 0c 00 00 00' - Density=3 GiB
       - '85 10 00 00 00' - Density=4 GiB
       - '85 18 00 00 00' - Density=6 GiB

     - Last two bytes are Clock in MHz:

       - '85 00 00 04 b0' - Clock=1200 MHz
       - '85 00 00 06 40' - Clock=1600 MHz
       - '85 00 00 07 08' - Clock=1600 MHz

   - '04 0b  00 00 00 01 22 34 00 dc  04 02 00' -- Read memory

     - The address is 64-bit value viewed from CPU address space:

       - '04 0b  00 00 00 01 22 34 00 dc  04 02 00' - Read from 0x00000001223400dc 4x 32bit word

     - The last three elements are number of words to read, width and 0:

       - Number of words: 1, 4, 8, 16, 32
       - Width: 0x0 - 8bit, 0x1 - 16bit, 0x2 - 32bit, 0x3 - 64bit
       - '04 0b  00 00 00 01 22 34 00 dc  01 03 00' - 1x 64bit word

   - '05 12  00 00 00 00 52 34 00 dc  12 34 ab cd ef 01 23 45  03 00' - Write memory

     - The address is first 64-bit value viewed from CPU address space.
     - The data is second 64-bit value.
     - The last two elements are width and 0:

       - Width: 0x0 - 8bit, 0x1 - 16bit, 0x2 - 32bit, 0x3 - 64bit
       - '05 12  00 00 00 00 52 34 00 dc  00 00 00 00 12 34 ab cd  02 00' - Write to 0x523400dc 32bit word 0x1234abcd

   - '01 03 00 00 00 aa aa aa aa aa aa' - Trigger calibration

Trigger calibration
-------------------
Since the calibrator reports calibration data in binary format, it is necessary
to dump raw serial port communication to file as follows. The '/dev/ttySx' is
the serial port connected to the board debug UART::

  $ ( stty raw ; tee cal.log ) < /dev/ttySx


Next, start the 'flash.bin' using e.g. UUU::

  $ uuu -v -V -brun spl flash.bin

Finally, connect and trigger calibration::

  $ printf '\x85\x00\x00\x00\x00' > /dev/ttySx
  $ printf '\x01\x03\x00\x00\x00\xaa\xaa\xaa\xaa\xaa\xaa' > /dev/ttySx

The calibration data are stored in 'cal.log' file now, in binary form.
