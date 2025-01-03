.. SPDX-License-Identifier: GPL-2.0-or-later

Universal Payload
-----------------

`Universal Payload (UPL) <https://universalpayload.github.io/spec/index.html>`_
is an Industry Standard for firmware components. UPL
is designed to improve interoperability within the firmware industry, allowing
mixing and matching of projects with less friction and fewer project-specific
implementations. UPL is cross-platform, supporting ARM, x86 and RISC-V
initially.

UPL is defined in termns of two firmware components:

`Platform Init`
	Perhaps initial setup of the hardware and jumps to the payload.

`Payload`
	Selects the OS to boot

In practice UPL can be used to handle any number of handoffs through the
firmware startup process, with one program acting as platform init and another
acting as the payload.

UPL provides a standard for three main pieces:

- file format for the payload, which may comprise multiple images to load
- handoff format for the information the payload needs, such as serial port,
  memory layout, etc.
- machine state and register settings at the point of handoff

See also the :doc:`cmd/upl`.

UPL in U-Boot
~~~~~~~~~~~~~

U-Boot supports:

- writing a UPL handoff (devicetree) in SPL and U-Boot proper
- reading a UPL handoff in U-Boot proper
- creating a FIT, but does not support building U-Boot as a UPL payload

The spec targeted is commit 3f1450d at git@github.com:UniversalPayload/spec.git

Booting EDK2 / Tianocore
~~~~~~~~~~~~~~~~~~~~~~~~

At present some additions are needed for Tianocore to work.

Tianocore can be built as a UPL payload using::

    python UefiPayloadPkg/UniversalPayloadBuild.py -t GCC5 --Fit  -a IA32 -q
        --pcd gUefiPayloadPkgTokenSpaceGuid.PcdHandOffFdtEnable=TRUE
        -D CPU_TIMER_LIB_ENABLE=FALSE -D BOOT_MANAGER_ESCAPE=TRUE
        -D CPU_RNG_ENABLE=FALSE -D SIO_BUS_ENABLE=TRUE
        -D PS2_KEYBOARD_ENABLE=TRUE -D PS2_MOUSE_ENABLE=TRUE

It is normal for the build to produce output even with the ``-q`` flag. Add
``-b release`` to build a non-debug version, if required. This drops the
debugging output.

The payload can then be run with the qemu-x86 build of U-Boot using::

    ./scripts/build-qemu.sh -a x86
       -u .../edk2/Build/UefiPayloadPkgIA32/UniversalPayload.fit -rsw

The following commands start the payload::

    => virtio scan; load virtio 0 100000 upl.fit; upl exec 100000

A sample run::

    $ ./scripts/build-qemu.sh -a x86
    mkfs.fat 4.2 (2021-01-31)
    Running qemu-system-x86_64  -drive if=virtio,file=upl.img,format=raw,id=hd0 -display none -serial mon:stdio
        bloblist_init() Bloblist at 0 not found (err=-2)


    U-Boot 2025.01-rc3-00372-gf3b9ed224d37-dirty (Jan 03 2025 - 20:18:31 +1300)

    CPU:   QEMU Virtual CPU version 2.5+
    DRAM:  512 MiB
    Core:  21 devices, 14 uclasses, devicetree: separate, universal payload active
    Loading Environment from nowhere... OK
    Video: 1024x768x32
    Model: QEMU x86 (I440FX)
    Net:         eth_initialize() No ethernet found.

    Hit any key to stop autoboot: 0
    => virtio scan; load virtio 0 100000 upl.fit; upl exec 100000
    3575808 bytes read in 4 ms (852.5 MiB/s)
    ## Loading firmware from FIT Image at 00100000 ...
    Using 'conf-1' configuration
    Trying 'tianocore' firmware subimage
        Description:  Uefi Universal Payload
        Created:      2025-01-03   7:17:10 UTC
        Type:         Firmware
        Compression:  uncompressed
        Data Start:   0x00101000
        Data Size:    77824 Bytes = 76 KiB
        Architecture: Unknown Architecture
        OS:           EFI Firmware
        Load Address: 0x00801000
    Loading firmware from 0x00101000 to 0x00801000
    ## Loading loadables from FIT Image at 00100000 ...
    Trying 'uefi-fv' loadables subimage
        Description:  UEFI Firmware Volume
        Created:      2025-01-03   7:17:10 UTC
        Type:         Firmware
        Compression:  uncompressed
        Data Start:   0x00115000
        Data Size:    3129344 Bytes = 3 MiB
        Architecture: Unknown Architecture
        OS:           EFI Firmware
        Load Address: 0x00814000
    Loading loadables from 0x00115000 to 0x00814000
    ## Loading loadables from FIT Image at 00100000 ...
    Trying 'bds-fv' loadables subimage
        Description:  BDS Firmware Volume
        Created:      2025-01-03   7:17:10 UTC
        Type:         Firmware
        Compression:  uncompressed
        Data Start:   0x00411000
        Data Size:    360448 Bytes = 352 KiB
        Architecture: Unknown Architecture
        OS:           EFI Firmware
        Load Address: 0x00b10000
    Loading loadables from 0x00411000 to 0x00b10000

    UPL: handoff at 1ed1d858 size 1000
    Starting at 8076fe ...

    Timer summary in microseconds (26 records):
        Mark    Elapsed  Stage
            0          0  reset
        23,052     23,052  board_init_f
        34,742     11,690  board_init_r
        82,386     47,644  eth_common_init
        89,713      7,327  main_loop
    2,118,058  2,028,345  fit_image_load
    2,118,126         68  fit_image_load
    2,118,139         13  fit_image_load
    2,121,226      3,087  fit_image_load
    2,153,860     32,634  fit_image_load
    2,153,886         26  fit_image_load
    2,154,014        128  fit_image_load
    2,154,052         38  fit_image_load
    2,160,129      6,077  fit_image_load
    2,166,210      6,081  fit_image_load
    2,166,248         38  fit_image_load
    2,166,263         15  fit_image_load
    2,197,150     30,887  fit_image_load
    2,197,162         12  fit_image_load
    2,197,208         46  fit_image_load
    2,197,261         53  fit_image_load
    2,206,528      9,267  fit_image_load
    2,264,384     57,856  upl_prepare

    Accumulated time:
                    3,046  dm_f
                    3,251  dm_r
                    6,648  vesa display
    Entering Universal Payload...
    sizeof(UINTN) = 0x4
    BootloaderParameter = 0x1ED1D858
    Start init HOBs...
    UplInitHob() FDT blob
    FDT = 0x1ED1D858  D00DFEED
    Start parsing DTB data
    MinimalNeededSize :4000000

    Node(00000028)  framebuffer@d0000000   Depth 1

    CheckNodeType  framebuffer@d0000000

    Node(000000C8)  isa   Depth 1

    CheckNodeType  isa

    Node(00000100)  serial@1000003f8   Depth 2

    CheckNodeType  serial@1000003f8

    Node(000001A0)  chosen   Depth 1

    CheckNodeType  chosen

    Node(000001B0)  reserved-memory   Depth 1

    CheckNodeType  reserved-memory

    Node(000001E4)  memory@1ecd1000   Depth 2

    CheckNodeType  memory@1ecd1000
    Skipping reserved-memory block.

    Node(00000228)  memory@1ece2000   Depth 2

    CheckNodeType  memory@1ece2000
    Skipping reserved-memory block.

    Node(0000026C)  memory@d0000000   Depth 2

    CheckNodeType  memory@d0000000
    Skipping reserved-memory block.

    Node(000002AC)  memory@0   Depth 1

    CheckNodeType  memory@0

            Property(000002BC)  reg  0000000000000000  0000000020000000
    MemoryBottom :0x1C000000
    FreeMemoryBottom :0x1C000000
    FreeMemoryTop :0x20000000
    MemoryTop :0x20000000

    Node(000002D8)  options   Depth 1

    CheckNodeType  options

    Node(00000304)  upl-images@100000   Depth 2

    CheckNodeType  upl-images@100000

    Node(0000037C)  image@b10000   Depth 3

    CheckNodeType  image@b10000

    Node(000003DC)  image@814000   Depth 3

    CheckNodeType  image@814000

    Node(00000440)  image@801000   Depth 3

    CheckNodeType  image@801000

    Node(000004BC)  upl-params   Depth 2

    CheckNodeType  upl-params

    Node(000004F8)  #address-cells   Depth FFFFFFFF

    CheckNodeType  #address-cells

    Node(00000028)  framebuffer@d0000000   Depth 1
    CheckNodeType  framebuffer@d0000000
    NodeType :0x5
    ParseFrameBuffer

    Node(000000C8)  isa   Depth 1
    CheckNodeType  isa
    NodeType :0x1
    ParseIsaSerial
    1C200 find serial compatible isa
    in espi serial  Property()  ns16550
    StartAddress   000003F8
    Attribute      00000001

    Node(00000100)  serial@1000003f8   Depth 2
    CheckNodeType  serial@1000003f8
    NodeType :0x2
    ParseMmioSerial
    1C200 NOT  serial compatible isa

    Node(000001A0)  chosen   Depth 1
    CheckNodeType  chosen
    NodeType :0x8
    ParseNothing

    Node(000001B0)  reserved-memory   Depth 1
    CheckNodeType  reserved-memory
    NodeType :0x3
    ParseReservedMemory

        SubNode(000001E4)  memory@1ecd1000
            Property    000000001ECD1000  0000000000010000

    RecordMemoryNode  1E4 , mNodeIndex :0
    compatible:  acpi
    acpi, StartAddress:1ECD1000, NumberOfBytes:0
    build gUniversalPayloadAcpiTableGuid , NumberOfBytes:10000

        SubNode(00000228)  memory@1ece2000
            Property    000000001ECE2000  0000000000001000

    RecordMemoryNode  228 , mNodeIndex :1
    compatible:  smbios
    build smbios, NumberOfBytes:1000

        SubNode(0000026C)  memory@d0000000
            Property    00000000D0000000  0000000000300000

    RecordMemoryNode  26C , mNodeIndex :2
    compatible:

    Node(000001E4)  memory@1ecd1000   Depth 2
    CheckNodeType  memory@1ecd1000
    NodeType :0x4
    ParseMemory
    Memory has initialized

    Node(00000228)  memory@1ece2000   Depth 2
    CheckNodeType  memory@1ece2000
    NodeType :0x4
    ParseMemory
    Memory has initialized

    Node(0000026C)  memory@d0000000   Depth 2
    CheckNodeType  memory@d0000000
    NodeType :0x4
    ParseMemory
    Memory has initialized

    Node(000002AC)  memory@0   Depth 1
    CheckNodeType  memory@0
    NodeType :0x4
    ParseMemory

    Node(000002D8)  options   Depth 1
    CheckNodeType  options
    NodeType :0x7
    ParseOptions

        SubNode(00000304)  upl-images@100000  Found image@ node

            Property(00000000)  entry  0000000000100000

        SubNode(000004BC)  upl-params
            Property(00000000)  address_width  28  Not Found PciEnumDone

    Node(00000304)  upl-images@100000   Depth 2
    CheckNodeType  upl-images@100000
    NodeType :0x8
    ParseNothing

    Node(0000037C)  image@b10000   Depth 3
    CheckNodeType  image@b10000
    NodeType :0x8
    ParseNothing

    Node(000003DC)  image@814000   Depth 3
    CheckNodeType  image@814000
    NodeType :0x8
    ParseNothing

    Node(00000440)  image@801000   Depth 3
    CheckNodeType  image@801000
    NodeType :0x8
    ParseNothing

    Node(000004BC)  upl-params   Depth 2
    CheckNodeType  upl-params
    NodeType :0x8
    ParseNothing

    Node(000004F8)  #address-cells   Depth FFFFFFFF
    CheckNodeType  #address-cells
    NodeType :0x8
    ParseNothing

    Platform Init violates the spec! No PCI root bridges found.
    CustomFdtNodeParser() #1 CHobList 1C000000
    Found options node (000002D8)
    Not Found hob list node
    Rsdp at 0x1ECD1000
    Rsdt at 0x1ECFAAC0, Xsdt at 0x0
    Found Fadt in Rsdt
    PmCtrl  Reg 0xE404
    PmTimer Reg 0xE408
    Reset   Reg 0x4F42880300000078
    Reset   Value 0x43
    PmEvt   Reg 0xE400
    PmGpeEn Reg 0xAFE2
    PcieBaseAddr 0x0
    PcieBaseSize 0x0
    Create acpi board info guid hob
    PayloadBase Entry = 0x00100000
    PayloadBase->Entry 100000 DataOffset 15000, DataSize 2FC000 FvLength 2FC000
    UPL Multiple fv[uefi-fv], Base=0x00115000, size=0x002FC000
    UPL Multiple fv[bds-fv], Base=0x00411000, size=0x00058000
    UPL FVs found by parsing FIT header
    Print all Hob information from Hob 0x1C000000
    HOB[0]: Type = EFI_HOB_TYPE_HANDOFF, Offset = 0x0, Length = 0x38
    BootMode            = 0x0
    EfiMemoryTop        = 0x20000000
    EfiMemoryBottom     = 0x1C000000
    EfiFreeMemoryTop    = 0x1FFF8000
    EfiFreeMemoryBottom = 0x1C000330
    EfiEndOfHobList     = 0x1C000328
    HOB[1]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x38, Length = 0x48
    Name       = 39F62CCE-6825-4669-BB56-541ABA753A07
    DataLength = 0x30
    HOB[2]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x80, Length = 0x30
    Guid   = gUniversalPayloadSerialPortInfoGuid(Serial Port Info)
    Revision       = 0x1
    Length         = 0x12
    UseMmio        = 0x0
    RegisterStride = 0x1
    BaudRate       = 115200
    RegisterBase   = 0x3F8
    HOB[3]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0xB0, Length = 0x30
    Guid   = gUniversalPayloadSerialPortInfoGuid(Serial Port Info)
    Revision       = 0x1
    Length         = 0x12
    UseMmio        = 0x1
    RegisterStride = 0x1
    BaudRate       = 115200
    RegisterBase   = 0x1
    HOB[4]: Type = EFI_HOB_TYPE_MEMORY_ALLOCATION, Offset = 0xE0, Length = 0x30
    Type              = EFI_HOB_TYPE_MEMORY_ALLOCATION
    Name              = 00000000-0000-0000-0000-000000000000
    MemoryBaseAddress = 0x1ECD1000
    MemoryLength      = 0x10000
    MemoryType        = EfiBootServicesData
    HOB[5]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x110, Length = 0x28
    Guid   = gUniversalPayloadAcpiTableGuid(ACPI table Guid)
    Revision  = 0x1
    Length    = 0xC
    Rsdp      = 0x1ECD1000
    HOB[6]: Type = EFI_HOB_TYPE_MEMORY_ALLOCATION, Offset = 0x138, Length = 0x30
    Type              = EFI_HOB_TYPE_MEMORY_ALLOCATION
    Name              = 00000000-0000-0000-0000-000000000000
    MemoryBaseAddress = 0x1ECE2000
    MemoryLength      = 0x1000
    MemoryType        = EfiBootServicesData
    HOB[7]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x168, Length = 0x28
    Guid   = gUniversalPayloadSmbios3TableGuid(SmBios Guid)
    Revision         = 0x1
    Length           = 0xC
    SmBiosEntryPoint = 0x1ECE2000
    HOB[8]: Type = EFI_HOB_TYPE_MEMORY_ALLOCATION, Offset = 0x190, Length = 0x30
    Type              = EFI_HOB_TYPE_MEMORY_ALLOCATION
    Name              = 00000000-0000-0000-0000-000000000000
    MemoryBaseAddress = 0xD0000000
    MemoryLength      = 0x300000
    MemoryType        = EfiReservedMemoryType
    HOB[9]: Type = EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, Offset = 0x1C0, Length = 0x30
    ResourceType      = EFI_RESOURCE_SYSTEM_MEMORY
    ResourceAttribute = 0x3C07
    PhysicalStart     = 0x0
    ResourceLength    = 0x20000000
    HOB[10]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x1F0, Length = 0x28
    Name       = 03D4C61D-2713-4EC5-A1CC-883BE9DC18E5
    DataLength = 0x10
    HOB[11]: Type = EFI_HOB_TYPE_CPU, Offset = 0x218, Length = 0x10
    SizeOfMemorySpace = 0x28
    SizeOfIoSpace     = 0x10
    HOB[12]: Type = EFI_HOB_TYPE_MEMORY_ALLOCATION, Offset = 0x228, Length = 0x30
    Type              = EFI_HOB_TYPE_MEMORY_ALLOCATION
    Name              = 00000000-0000-0000-0000-000000000000
    MemoryBaseAddress = 0x1FFF8000
    MemoryLength      = 0x8000
    MemoryType        = EfiReservedMemoryType
    HOB[13]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x258, Length = 0x48
    Guid   = gEfiMemoryTypeInformationGuid(Memory Type Information Guid)
    Type            = 0x9
    NumberOfPages   = 0x19
    HOB[14]: Type = EFI_HOB_TYPE_GUID_EXTENSION, Offset = 0x2A0, Length = 0x58
    Guid   = gUefiAcpiBoardInfoGuid(Acpi Guid)
    Revision        = 0x30
    Reserved0       = 0x1C0002B9
    ResetValue      = 0x43
    PmEvtBase       = 0xE400
    PmGpeEnBase     = 0xAFE2
    PmCtrlRegBase   = 0xE404
    PmTimerRegBase  = 0xE408
    ResetRegAddress = 0x4F42880300000078
    PcieBaseAddress = 0x0
    PcieBaseSize    = 0x0
    HOB[15]: Type = EFI_HOB_TYPE_FV, Offset = 0x2F8, Length = 0x18
    BaseAddress = 0x115000
    Length      = 0x2FC000
    HOB[16]: Type = EFI_HOB_TYPE_FV, Offset = 0x310, Length = 0x18
    BaseAddress = 0x411000
    Length      = 0x58000
    There are totally 17 Hobs, the End Hob address is 1C000328
    PayloadEntry: AddressBits=40 LevelOfPaging=4 1GPage=0
    Pml5=1 Pml4=2 Pdp=512 TotalPage=1027
    HandOffToDxeCore() Stack Base: 0x1FFA7000, Stack Size: 0x20000
    PROGRESS CODE: V03040003 I0
    Loading driver C68DAA4E-7AB5-41E8-A91D-5954421053F3
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F34A0C0
    Loading driver at 0x0001F34F000 EntryPoint=0x0001F34FE60 BlSupportDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F34A318
    ProtectUefiImageCommon - 0x1F34A0C0
    - 0x000000001F34F000 - 0x0000000000001AC0
    PROGRESS CODE: V03040002 I0
    PROGRESS CODE: V03040003 I0
    Loading driver F80697E9-7FD6-4665-8646-88E33EF71DFC
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F34E040
    Loading driver at 0x0001F324000 EntryPoint=0x0001F328B14 SecurityStubDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F34A498
    ProtectUefiImageCommon - 0x1F34E040
    - 0x000000001F324000 - 0x00000000000075C0
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 94AB2F58-1438-4EF1-9152-18941A3A0E68 1F32AF08
    InstallProtocolInterface: A46423E3-4617-49F1-B9FF-D1BFA9115839 1F32AF00
    InstallProtocolInterface: 15853D7C-3DDF-43E0-A1CB-EBF85B8F872C 1F32AEE0
    PROGRESS CODE: V03040003 I0
    Loading driver 1A1E4886-9517-440E-9FDE-3BE44CEE2136
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F34E4C0
    Loading driver at 0x0001F2EC000 EntryPoint=0x0001F2F86A9 CpuDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F34DF18
    ProtectUefiImageCommon - 0x1F34E4C0
    - 0x000000001F2EC000 - 0x000000000001B7C0
    PROGRESS CODE: V03040002 I0
    Paging: added 512 pages to page table pool
    CurrentPagingContext:
    MachineType   - 0x8664
    PageTableBase - 0x1F801000
    Attributes    - 0xC0000002
    InstallProtocolInterface: 26BACCB1-6F42-11D4-BCE7-0080C73C8881 1F300AC0
    MemoryProtectionCpuArchProtocolNotify:
    ProtectUefiImageCommon - 0x1FFEE408
    - 0x000000001FFC7000 - 0x0000000000031000
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    ProtectUefiImageCommon - 0x1F36E7C0
    - 0x000000001F33B000 - 0x000000000000CA00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    ProtectUefiImageCommon - 0x1F354BC0
    - 0x000000001F334000 - 0x0000000000006080
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    ProtectUefiImageCommon - 0x1F34B040
    - 0x000000001F67A000 - 0x0000000000006000
    SetUefiImageMemoryAttributes - 0x000000001F67A000 - 0x0000000000001000 (0x0000000000004000)
    SetUefiImageMemoryAttributes - 0x000000001F67B000 - 0x0000000000003000 (0x0000000000020000)
    SetUefiImageMemoryAttributes - 0x000000001F67E000 - 0x0000000000002000 (0x0000000000004000)
    ProtectUefiImageCommon - 0x1F34B4C0
    - 0x000000001F674000 - 0x0000000000006000
    SetUefiImageMemoryAttributes - 0x000000001F674000 - 0x0000000000001000 (0x0000000000004000)
    SetUefiImageMemoryAttributes - 0x000000001F675000 - 0x0000000000004000 (0x0000000000020000)
    SetUefiImageMemoryAttributes - 0x000000001F679000 - 0x0000000000001000 (0x0000000000004000)
    ProtectUefiImageCommon - 0x1F34A0C0
    - 0x000000001F34F000 - 0x0000000000001AC0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    ProtectUefiImageCommon - 0x1F34E040
    - 0x000000001F324000 - 0x00000000000075C0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    ProtectUefiImageCommon - 0x1F34E4C0
    - 0x000000001F2EC000 - 0x000000000001B7C0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    Failed to update capability: [8] 00000000FED00000 - 00000000FED003FF (C000000000000001 -> C000000000026001)
    AP Loop Mode is 1
    AP Vector: non-16-bit = 1F333000/44A
    WakeupBufferStart = 87000, WakeupBufferSize = DD
    AP Vector: 16-bit = 87000/39, ExchangeInfo = 87039/A4
    CpuDxe: 5-Level Paging = 0
    APIC MODE is 1
    MpInitLib: Find 1 processors in system.
    GetMicrocodePatchInfoFromHob: Microcode patch cache HOB is not found.
    CPU[0000]: Microcode revision = 00000000, expected = 00000000
    Detect CPU count: 1
    Does not find any HOB stored CPU BIST information!
    InstallProtocolInterface: 3FDDA605-A76E-4F46-AD29-12F4531B3D08 1F300A60
    PROGRESS CODE: V03040003 I0
    Loading driver C8339973-A563-4561-B858-D8476F9DEFC4
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F34C0C0
    Loading driver at 0x0001F32F000 EntryPoint=0x0001F32FE77 Metronome.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F34D218
    ProtectUefiImageCommon - 0x1F34C0C0
    - 0x000000001F32F000 - 0x0000000000001780
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 26BACCB2-6F42-11D4-BCE7-0080C73C8881 1F3305F0
    PROGRESS CODE: V03040003 I0
    Loading driver B601F8C4-43B7-4784-95B1-F4226CB40CEE
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F34C6C0
    Loading driver at 0x0001F66E000 EntryPoint=0x0001F670518 RuntimeDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F332B18
    ProtectUefiImageCommon - 0x1F34C6C0
    - 0x000000001F66E000 - 0x0000000000006000
    SetUefiImageMemoryAttributes - 0x000000001F66E000 - 0x0000000000001000 (0x0000000000004001)
    SetUefiImageMemoryAttributes - 0x000000001F66F000 - 0x0000000000003000 (0x0000000000020001)
    SetUefiImageMemoryAttributes - 0x000000001F672000 - 0x0000000000002000 (0x0000000000004001)
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: B7DFB4E1-052F-449F-87BE-9818FC91B733 1F6720C0
    PROGRESS CODE: V03040003 I0
    Loading driver 4B28E4C7-FF36-4E10-93CF-A82159E777C5
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F3321C0
    Loading driver at 0x0001F668000 EntryPoint=0x0001F66A84A ResetSystemRuntimeDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F332698
    ProtectUefiImageCommon - 0x1F3321C0
    - 0x000000001F668000 - 0x0000000000006000
    SetUefiImageMemoryAttributes - 0x000000001F668000 - 0x0000000000001000 (0x0000000000004001)
    SetUefiImageMemoryAttributes - 0x000000001F669000 - 0x0000000000003000 (0x0000000000020001)
    SetUefiImageMemoryAttributes - 0x000000001F66C000 - 0x0000000000002000 (0x0000000000004001)
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 27CFAC88-46CC-11D4-9A38-0090273FC14D 0
    InstallProtocolInterface: 9DA34AE0-EAF9-4BBF-8EC3-FD60226C44BE 1F66C128
    InstallProtocolInterface: 695D7835-8D47-4C11-AB22-FA8ACCE7AE7A 1F66C168
    InstallProtocolInterface: 2DF6BA0B-7092-440D-BD04-FB091EC3F3C1 1F66C0E8
    PROGRESS CODE: V03040003 I0
    Loading driver CBD2E4D5-7068-4FF5-B462-9822B4AD8D60
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F331140
    Loading driver at 0x0001F659000 EntryPoint=0x0001F662C5E VariableRuntimeDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F331418
    ProtectUefiImageCommon - 0x1F331140
    - 0x000000001F659000 - 0x000000000000F000
    SetUefiImageMemoryAttributes - 0x000000001F659000 - 0x0000000000001000 (0x0000000000004001)
    SetUefiImageMemoryAttributes - 0x000000001F65A000 - 0x000000000000C000 (0x0000000000020001)
    SetUefiImageMemoryAttributes - 0x000000001F666000 - 0x0000000000002000 (0x0000000000004001)
    PROGRESS CODE: V03040002 I0
    Variable driver will work at emulated non-volatile variable mode!
    Variable driver will work with auth variable format!
    InstallProtocolInterface: CD3D0A05-9E24-437C-A891-1EE053DB7638 1F666260
    InstallProtocolInterface: AF23B340-97B4-4685-8D4F-A3F28169B21D 1F666230
    InstallProtocolInterface: 1E5668E2-8481-11D4-BCF1-0080C73C8881 0
    NOTICE - AuthVariableLibInitialize() returns Unsupported!
    Variable driver will continue to work without auth variable support!
    RecordSecureBootPolicyVarData GetVariable SecureBoot Status E
    InstallProtocolInterface: 6441F818-6362-4E44-B570-7DBA31DD2453 0
    VarCheckLibRegisterSetVariableCheckHandler - 0x1F65EC64 Success
    InstallProtocolInterface: 81D1675C-86F6-48DF-BD95-9A6E4F0925C3 1F6661C0
    PROGRESS CODE: V03040003 I0
    Loading driver A19B1FE7-C1BC-49F8-875F-54A5D542443F
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F32E2C0
    Loading driver at 0x0001F322000 EntryPoint=0x0001F32352F CpuIo2Dxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F32E618
    ProtectUefiImageCommon - 0x1F32E2C0
    - 0x000000001F322000 - 0x0000000000001DC0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: AD61F191-AE5F-4C0E-B9FA-E869D288C64F 1F323C60
    PROGRESS CODE: V03040003 I0
    Loading driver 96B5C032-DF4C-4B6E-8232-438DCF448D0E
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F32DCC0
    Loading driver at 0x0001F31E000 EntryPoint=0x0001F31F06F NullMemoryTestDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F32D898
    ProtectUefiImageCommon - 0x1F32DCC0
    - 0x000000001F31E000 - 0x0000000000001A80
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 309DE7F1-7F5E-4ACE-B49C-531BE5AA95EF 1F31F8C0
    PROGRESS CODE: V03040003 I0
    Loading driver 348C4D62-BFBD-4882-9ECE-C80BB1C4783B
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F32D3C0
    Loading driver at 0x0001F2AA000 EntryPoint=0x0001F2C4E5E HiiDatabase.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F32C018
    ProtectUefiImageCommon - 0x1F32D3C0
    - 0x000000001F2AA000 - 0x0000000000020780
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: E9CA4775-8657-47FC-97E7-7ED65A084324 1F2CA2E8
    InstallProtocolInterface: 0FD96974-23AA-4CDC-B9CB-98D17750322A 1F2CA360
    InstallProtocolInterface: EF9FC172-A1B2-4693-B327-6D32FC416042 1F2CA388
    InstallProtocolInterface: 587E72D7-CC50-4F79-8209-CA291FC1A10F 1F2CA3E0
    InstallProtocolInterface: 0A8BADD5-03B8-4D19-B128-7B8F0EDAA596 1F2CA410
    InstallProtocolInterface: 31A6406A-6BDF-4E46-B2A2-EBAA89C40920 1F2CA308
    InstallProtocolInterface: 1A1241E6-8F19-41A9-BC0E-E8EF39E06546 1F2CA330
    PROGRESS CODE: V03040003 I0
    Loading driver 13AC6DD0-73D0-11D4-B06B-00AA00BD6DE7
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F32C1C0
    Loading driver at 0x0001F312000 EntryPoint=0x0001F316793 EbcDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F321018
    ProtectUefiImageCommon - 0x1F32C1C0
    - 0x000000001F312000 - 0x0000000000005BC0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 13AC6DD1-73D0-11D4-B06B-00AA00BD6DE7 1F321C98
    InstallProtocolInterface: 96F46153-97A7-4793-ACC1-FA19BF78EA97 1F3175C0
    InstallProtocolInterface: 2755590C-6F3C-42FA-9EA4-A3BA543CDA25 1F321A18
    InstallProtocolInterface: AAEACCFD-F27B-4C17-B610-75CA1F2DFB52 1F321918
    PROGRESS CODE: V03040003 I0
    Loading driver F9D88642-0737-49BC-81B5-6889CD57D9EA
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F3211C0
    Loading driver at 0x0001F30E000 EntryPoint=0x0001F3108C1 SmbiosDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F321B98
    ProtectUefiImageCommon - 0x1F3211C0
    - 0x000000001F30E000 - 0x0000000000003C40
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 03583FF6-CB36-4940-947E-B9B39F4AFAF7 1F311AD0
    PROGRESS CODE: V03040003 I0
    Loading driver 9A5163E7-5C29-453F-825C-837A46A81E15
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F3200C0
    Loading driver at 0x0001F318000 EntryPoint=0x0001F319A26 SerialDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F320D18
    ProtectUefiImageCommon - 0x1F3200C0
    - 0x000000001F318000 - 0x0000000000002440
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: BB25CF6F-F1D4-11D2-9A0C-0090273FC1FD 1F31A1C0
    InstallProtocolInterface: 09576E91-6D3F-11D2-8E39-00A0C969723B 1F31A2A0
    PROGRESS CODE: V03040003 I0
    Loading driver 9622E42C-8E38-4A08-9E8F-54F784652F6B
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F320540
    Loading driver at 0x0001F2DC000 EntryPoint=0x0001F2E0743 AcpiTableDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F31DE18
    ProtectUefiImageCommon - 0x1F320540
    - 0x000000001F2DC000 - 0x0000000000007680
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    Fail to locate AcpiSiliconHob!!
    InstallProtocolInterface: FFE06BDD-6107-46A6-7BB2-5A9C7EC5275C 1F31D120
    InstallProtocolInterface: EB97088E-CFDF-49C6-BE4B-D906A5B20E86 1F31D130
    PROGRESS CODE: V03040003 I0
    Loading driver 6CE6B0DE-781C-4F6C-B42D-98346C614BEC
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F31D240
    Loading driver at 0x0001F2E8000 EntryPoint=0x0001F2E9B91 HpetTimerDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F31D518
    ProtectUefiImageCommon - 0x1F31D240
    - 0x000000001F2E8000 - 0x0000000000003440
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    Init HPET Timer Driver
    HPET Base Address = 0xFED00000
    HPET_GENERAL_CAPABILITIES_ID  = 0x009896808086A201
    HPET_GENERAL_CONFIGURATION    = 0x0000000000000000
    HPET_GENERAL_INTERRUPT_STATUS = 0x0000000000000000
    HPET_MAIN_COUNTER             = 0x0000000000000000
    HPET Main Counter Period      = 10000000 (fs)
    HPET_TIMER0_CONFIGURATION     = 0x0000000400000030
    HPET_TIMER0_COMPARATOR        = 0xFFFFFFFFFFFFFFFF
    HPET_TIMER0_MSI_ROUTE         = 0x0000000000000000
    HPET_TIMER1_CONFIGURATION     = 0x0000000400000030
    HPET_TIMER1_COMPARATOR        = 0xFFFFFFFFFFFFFFFF
    HPET_TIMER1_MSI_ROUTE         = 0x0000000000000000
    HPET_TIMER2_CONFIGURATION     = 0x0000000400000030
    HPET_TIMER2_COMPARATOR        = 0xFFFFFFFFFFFFFFFF
    HPET_TIMER2_MSI_ROUTE         = 0x0000000000000000
    Choose 64-bit HPET timer.
    HPET Interrupt Mode I/O APIC
    HPET I/O APIC IRQ         = 0x02
    HPET Interrupt Vector     = 0x40
    HPET Counter Mask         = 0xFFFFFFFFFFFFFFFF
    HPET Timer Period         = 100000
    HPET Timer Count          = 0x00000000000F4240
    HPET_TIMER0_CONFIGURATION = 0x0000000400000436
    HPET_TIMER0_COMPARATOR    = 0x00000000000F4241
    HPET_TIMER0_MSI_ROUTE     = 0x0000000000000000
    InstallProtocolInterface: 26BACCB3-6F42-11D4-BCE7-0080C73C8881 1F2EB240
    PROGRESS CODE: V03040003 I0
    Loading driver 42857F0A-13F2-4B21-8A23-53D3F714B840
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F31CCC0
    Loading driver at 0x0001F655000 EntryPoint=0x0001F656FD8 CapsuleRuntimeDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F31C898
    ProtectUefiImageCommon - 0x1F31CCC0
    - 0x000000001F655000 - 0x0000000000004000
    SetUefiImageMemoryAttributes - 0x000000001F655000 - 0x0000000000001000 (0x0000000000004001)
    SetUefiImageMemoryAttributes - 0x000000001F656000 - 0x0000000000002000 (0x0000000000020001)
    SetUefiImageMemoryAttributes - 0x000000001F658000 - 0x0000000000001000 (0x0000000000004001)
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 5053697E-2CBC-4819-90D9-0580DEEE5754 0
    PROGRESS CODE: V03040003 I0
    Loading driver AD608272-D07F-4964-801E-7BD3B7888652
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F31B040
    Loading driver at 0x0001F651000 EntryPoint=0x0001F652D0A MonotonicCounterRuntimeDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F31BF18
    ProtectUefiImageCommon - 0x1F31B040
    - 0x000000001F651000 - 0x0000000000004000
    SetUefiImageMemoryAttributes - 0x000000001F651000 - 0x0000000000001000 (0x0000000000004001)
    SetUefiImageMemoryAttributes - 0x000000001F652000 - 0x0000000000002000 (0x0000000000020001)
    SetUefiImageMemoryAttributes - 0x000000001F654000 - 0x0000000000001000 (0x0000000000004001)
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 1DA97072-BDDC-4B30-99F1-72A0B56FFF2A 0
    PROGRESS CODE: V03040003 I0
    Loading driver 378D7B65-8DA9-4773-B6E4-A47826A833E1
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F31B440
    Loading driver at 0x0001F64B000 EntryPoint=0x0001F64E3EF PcRtc.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F30DF98
    ProtectUefiImageCommon - 0x1F31B440
    - 0x000000001F64B000 - 0x0000000000006000
    SetUefiImageMemoryAttributes - 0x000000001F64B000 - 0x0000000000001000 (0x0000000000004001)
    SetUefiImageMemoryAttributes - 0x000000001F64C000 - 0x0000000000004000 (0x0000000000020001)
    SetUefiImageMemoryAttributes - 0x000000001F650000 - 0x0000000000001000 (0x0000000000004001)
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 27CFAC87-46CC-11D4-9A38-0090273FC14D 0
    PROGRESS CODE: V03040003 I0
    Loading driver EBF342FE-B1D3-4EF8-957C-8048606FF671
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F30D2C0
    Loading driver at 0x0001F272000 EntryPoint=0x0001F284E66 SetupBrowser.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F30D698
    ProtectUefiImageCommon - 0x1F30D2C0
    - 0x000000001F272000 - 0x000000000001B940
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: B9D4C360-BCFB-4F9B-9298-53C136982258 1F28D3B0
    InstallProtocolInterface: A770C357-B693-4E6D-A6CF-D21C728E550B 1F28D3E0
    InstallProtocolInterface: 1F73B18D-4630-43C1-A1DE-6F80855D7DA4 1F28D3C0
    PROGRESS CODE: V03040003 I0
    Loading driver 128FB770-5E79-4176-9E51-9BB268A17DD1
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F30C240
    Loading driver at 0x0001F29C000 EntryPoint=0x0001F2A46EF PciHostBridgeDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F30C518
    ProtectUefiImageCommon - 0x1F30C240
    - 0x000000001F29C000 - 0x000000000000DE00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    Error: Image at 0001F29C000 start failed: Unsupported
    PROGRESS CODE: V03040003 I0
    Loading driver 6D33944A-EC75-4855-A54D-809C75241F6C
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F30C240
    Loading driver at 0x0001F25A000 EntryPoint=0x0001F26A176 BdsDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F30C718
    ProtectUefiImageCommon - 0x1F30C240
    - 0x000000001F25A000 - 0x0000000000017FC0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 665E3FF6-46CC-11D4-9A38-0090273FC14D 1F2717D0
    PROGRESS CODE: V03040003 I0
    Loading driver F099D67F-71AE-4C36-B2A3-DCEB0EB2B7D8
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F30B1C0
    Loading driver at 0x0001F2E6000 EntryPoint=0x0001F2E6EE4 WatchdogTimer.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F30BB98
    ProtectUefiImageCommon - 0x1F30B1C0
    - 0x000000001F2E6000 - 0x0000000000001840
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 665E3FF5-46CC-11D4-9A38-0090273FC14D 1F2E7690
    PROGRESS CODE: V03040003 I0
    Loading driver E660EA85-058E-4B55-A54B-F02F83A24707
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F30B640
    Loading driver at 0x0001F244000 EntryPoint=0x0001F254844 DisplayEngine.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F30AB18
    ProtectUefiImageCommon - 0x1F30B640
    - 0x000000001F244000 - 0x0000000000015740
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 9BBE29E9-FDA1-41EC-AD52-452213742D2E 1F257B70
    InstallProtocolInterface: 4311EDC0-6054-46D4-9E40-893EA952FCCC 1F257B88
    PROGRESS CODE: V03040003 I0
    Loading driver 35034CE2-A6E5-4FB4-BABE-A0156E9B2549
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2E47C0
    Loading driver at 0x0001F233000 EntryPoint=0x0001F23E624 PlatDriOverrideDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2E4118
    ProtectUefiImageCommon - 0x1F2E47C0
    - 0x000000001F233000 - 0x0000000000010140
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 09576E91-6D3F-11D2-8E39-00A0C969723B 1F242A80
    InstallProtocolInterface: 330D4706-F2A0-4E4F-A369-B66FA8D54385 1F2E4438
    InstallProtocolInterface: 6B30C738-A391-11D4-9A3B-0090273FC14D 1F2E4450
    PROGRESS CODE: V03040003 I0
    Loading driver 93B80004-9FB3-11D4-9A3A-0090273FC14D
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2D9340
    Loading driver at 0x0001F21E000 EntryPoint=0x0001F22E54A PciBusDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2D5F98
    ProtectUefiImageCommon - 0x1F2D9340
    - 0x000000001F21E000 - 0x0000000000014D00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F232380
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F232260
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F232890
    InstallProtocolInterface: 19CB87AB-2CB9-4665-8360-DDCF6054F79D 1F232870
    The status to Get Platform Driver Override Variable is Not Found
    PROGRESS CODE: V03040003 I0
    Loading driver 864E1CA8-85EB-4D63-9DCC-6E0FC90FFD55
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2D95C0
    Loading driver at 0x0001F2CF000 EntryPoint=0x0001F2D1075 SioBusDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2D6B98
    ProtectUefiImageCommon - 0x1F2D95C0
    - 0x000000001F2CF000 - 0x0000000000002C80
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F2D1A00
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F2D1B40
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F2D19E0
    PROGRESS CODE: V03040003 I0
    Loading driver C4D1F932-821F-4744-BF06-6D30F7730F8D
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2D8CC0
    Loading driver at 0x0001F29E000 EntryPoint=0x0001F2A22BE Ps2KeyboardDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2D8B98
    ProtectUefiImageCommon - 0x1F2D8CC0
    - 0x000000001F29E000 - 0x0000000000005440
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F2A3280
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F2A32E0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F2A31F0
    PROGRESS CODE: V03040003 I0
    Loading driver 08464531-4C99-4C4C-A887-8D8BA4BBB063
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2D82C0
    Loading driver at 0x0001F2A6000 EntryPoint=0x0001F2A8A57 Ps2MouseDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2D8998
    ProtectUefiImageCommon - 0x1F2D82C0
    - 0x000000001F2A6000 - 0x00000000000036C0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F2A9500
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F2A9560
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F2A94A0
    PROGRESS CODE: V03040003 I0
    Loading driver 51CCF399-4FDF-4E55-A45B-E123F84D456A
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2D70C0
    Loading driver at 0x0001F29A000 EntryPoint=0x0001F29CBBC ConPlatformDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2D7A98
    ProtectUefiImageCommon - 0x1F2D70C0
    - 0x000000001F29A000 - 0x0000000000003C40
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F29D960
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F29DA80
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F29D930
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F29D900
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F29DA80
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F29D930
    PROGRESS CODE: V03040003 I0
    Loading driver 408EDCEC-CF6D-477C-A5A8-B4844E3DE281
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2D4040
    Loading driver at 0x0001F216000 EntryPoint=0x0001F21C0EC ConSplitterDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2D4F18
    ProtectUefiImageCommon - 0x1F2D4040
    - 0x000000001F216000 - 0x0000000000007D40
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F21DA40
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F21DB20
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F21D340
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F21D9C0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F21DB00
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F21D320
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F21D940
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F21DAE0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F21D300
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F21D8C0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F21DAC0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F21D2E0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F21D840
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F21DAA0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F21D2C0
    InstallProtocolInterface: 387477C1-69C7-11D2-8E39-00A0C969723B 1F21D670
    InstallProtocolInterface: DD9E7534-7762-4698-8C14-F58517A625AA 1F21D6A0
    InstallProtocolInterface: 31878C87-0B75-11D5-9A4F-0090273FC14D 1F21D710
    InstallProtocolInterface: 8D59D32B-C655-4AE9-9B15-F25904992A43 1F21D768
    InstallProtocolInterface: 387477C2-69C7-11D2-8E39-00A0C969723B 1F21D530
    InstallProtocolInterface: 387477C2-69C7-11D2-8E39-00A0C969723B 1F21D410
    PROGRESS CODE: V03040003 I0
    Loading driver CCCB0C28-4B24-11D5-9A5A-0090273FC14D
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2CE740
    Loading driver at 0x0001F28E000 EntryPoint=0x0001F291405 GraphicsConsoleDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2CEA98
    ProtectUefiImageCommon - 0x1F2CE740
    - 0x000000001F28E000 - 0x0000000000005AC0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F292140
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F293900
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F292110
    PROGRESS CODE: V03040003 I0
    Loading driver 9E863906-A40F-4875-977F-5B93FF237FC6
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2CD140
    Loading driver at 0x0001F204000 EntryPoint=0x0001F20A753 TerminalDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2CD918
    ProtectUefiImageCommon - 0x1F2CD140
    - 0x000000001F204000 - 0x0000000000008080
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F20BE60
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F20BEC0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F20BBF0
    PROGRESS CODE: V03040003 I0
    Loading driver 0B04B2ED-861C-42CD-A22F-C3AAFACCB896
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2CCB40
    Loading driver at 0x0001CBB5000 EntryPoint=0x0001CBB7EF2 GraphicsOutputDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2CD698
    ProtectUefiImageCommon - 0x1F2CCB40
    - 0x000000001CBB5000 - 0x0000000000004100
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CBB8E00
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CBB8E60
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CBB8DE0
    PROGRESS CODE: V03040003 I0
    Loading driver 6B38F7B4-AD98-40E9-9093-ACA2B5A253C4
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2CC1C0
    Loading driver at 0x0001CBB0000 EntryPoint=0x0001CBB31BE DiskIoDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2CC498
    ProtectUefiImageCommon - 0x1F2CC1C0
    - 0x000000001CBB0000 - 0x00000000000042C0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CBB4000
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CBB4140
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CBB3FD0
    PROGRESS CODE: V03040003 I0
    Loading driver 1FA1F39E-FEFF-4AAE-BD7B-38A070A3B609
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2CB040
    Loading driver at 0x0001CBA9000 EntryPoint=0x0001CBAE4B0 PartitionDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2CBE98
    ProtectUefiImageCommon - 0x1F2CB040
    - 0x000000001CBA9000 - 0x0000000000006800
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CBAF540
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CBAF660
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CBAF510
    PROGRESS CODE: V03040003 I0
    Loading driver CD3BAFB6-50FB-4FE8-8E4E-AB74D2C1A600
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2CB440
    Loading driver at 0x0001F296000 EntryPoint=0x0001F297002 EnglishDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2CB818
    ProtectUefiImageCommon - 0x1F2CB440
    - 0x000000001F296000 - 0x0000000000001C40
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 1D85CD7F-F43D-11D2-9A0C-0090273FC14D 1F297780
    InstallProtocolInterface: A4C751FC-23AE-4C3E-92E9-4964CF63F349 1F297720
    PROGRESS CODE: V03040003 I0
    Loading driver 820C59BB-274C-43B2-83EA-DAC673035A59
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2A51C0
    Loading driver at 0x0001F210000 EntryPoint=0x0001F211F5C SataController.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2A5B98
    ProtectUefiImageCommon - 0x1F2A51C0
    - 0x000000001F210000 - 0x0000000000002D00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1F212B20
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1F212B80
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1F212A90
    PROGRESS CODE: V03040003 I0
    Loading driver 19DF145A-B1D4-453F-8507-38816676D7F6
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2A5540
    Loading driver at 0x0001CB9B000 EntryPoint=0x0001CB9FD1F AtaBusDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2A4F98
    ProtectUefiImageCommon - 0x1F2A5540
    - 0x000000001CB9B000 - 0x0000000000006200
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CBA0D80
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CBA0E80
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CBA0E60
    PROGRESS CODE: V03040003 I0
    Loading driver 5E523CB4-D397-4986-87BD-A6DD8B22F455
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2A4140
    Loading driver at 0x0001CB83000 EntryPoint=0x0001CB8C4BA AtaAtapiPassThruDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F2A4398
    ProtectUefiImageCommon - 0x1F2A4140
    - 0x000000001CB83000 - 0x000000000000B340
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB8DEC0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB8DF20
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB8DE50
    PROGRESS CODE: V03040003 I0
    Loading driver 0167CCC4-D0F7-4F21-A3EF-9E64B7CDCE8B
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2A4540
    Loading driver at 0x0001CBA5000 EntryPoint=0x0001CBA80AB ScsiBus.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F295B18
    ProtectUefiImageCommon - 0x1F2A4540
    - 0x000000001CBA5000 - 0x0000000000003D00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CBA8AE0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CBA8BA0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CBA8AC0
    PROGRESS CODE: V03040003 I0
    Loading driver 0A66E322-3740-4CCE-AD62-BD172CECCA35
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2950C0
    Loading driver at 0x0001CB77000 EntryPoint=0x0001CB80B28 ScsiDisk.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F295A98
    ProtectUefiImageCommon - 0x1F2950C0
    - 0x000000001CB77000 - 0x000000000000B480
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB822A0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB82300
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB82200
    PROGRESS CODE: V03040003 I0
    Loading driver 5BE3BDF4-53CF-46A3-A6A9-73C34A6E5EE3
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F294040
    Loading driver at 0x0001CB5D000 EntryPoint=0x0001CB670C3 NvmExpressDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F294D18
    ProtectUefiImageCommon - 0x1F294040
    - 0x000000001CB5D000 - 0x000000000000C640
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB69460
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB694C0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB69390
    InstallProtocolInterface: 5C198761-16A8-4E69-972C-89D67954F81D 1CB692D0
    PROGRESS CODE: V03040003 I0
    Loading driver 961578FE-B6B7-44C3-AF35-6BC705CD2B1F
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F294340
    Loading driver at 0x0001CB6C000 EntryPoint=0x0001CB74C52 Fat.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F294598
    ProtectUefiImageCommon - 0x1F294340
    - 0x000000001CB6C000 - 0x000000000000A340
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB76100
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB76160
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB75F20
    PROGRESS CODE: V03040003 I0
    Loading driver 8E325979-3FE1-4927-AAE2-8F5C4BD2AF0D
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F215040
    Loading driver at 0x0001CB52000 EntryPoint=0x0001CB5A87D SdMmcPciHcDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F215E98
    ProtectUefiImageCommon - 0x1F215040
    - 0x000000001CB52000 - 0x000000000000AE80
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB5CA20
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB5CA80
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB5C9D0
    PROGRESS CODE: V03040003 I0
    Loading driver 2145F72F-E6F1-4440-A828-59DC9AAB5F89
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2153C0
    Loading driver at 0x0001CB49000 EntryPoint=0x0001CB4F351 EmmcDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F215718
    ProtectUefiImageCommon - 0x1F2153C0
    - 0x000000001CB49000 - 0x0000000000008380
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB511C0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB51220
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB51140
    PROGRESS CODE: V03040003 I0
    Loading driver 430AC2F7-EEC6-4093-94F7-9F825A7C1C40
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F214B40
    Loading driver at 0x0001CB8F000 EntryPoint=0x0001CB93562 SdDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F214098
    ProtectUefiImageCommon - 0x1F214B40
    - 0x000000001CB8F000 - 0x0000000000005A80
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB948E0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB94940
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB946D0
    PROGRESS CODE: V03040003 I0
    Loading driver 2FB92EFA-2EE0-4BAE-9EB6-7464125E1EF7
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2141C0
    Loading driver at 0x0001CB39000 EntryPoint=0x0001CB3EEE1 UhciDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F214798
    ProtectUefiImageCommon - 0x1F2141C0
    - 0x000000001CB39000 - 0x0000000000007240
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB40020
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB400A0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB40080
    PROGRESS CODE: V03040003 I0
    Loading driver BDFE430E-8F2A-4DB0-9991-6F856594777E
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F213CC0
    Loading driver at 0x0001CB27000 EntryPoint=0x0001CB2E08B EhciDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F213918
    ProtectUefiImageCommon - 0x1F213CC0
    - 0x000000001CB27000 - 0x0000000000008B80
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB2F9C0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB2FA20
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB2F950
    PROGRESS CODE: V03040003 I0
    Loading driver B7F50E91-A759-412C-ADE4-DCD03E7F7C28
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F213340
    Loading driver at 0x0001CB0B000 EntryPoint=0x0001CB16389 XhciDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F213718
    ProtectUefiImageCommon - 0x1F213340
    - 0x000000001CB0B000 - 0x000000000000DF40
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB18C60
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB18D60
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB18D40
    PROGRESS CODE: V03040003 I0
    Loading driver 240612B7-A063-11D4-9A3A-0090273FC14D
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F20FCC0
    Loading driver at 0x0001CB01000 EntryPoint=0x0001CB086AF UsbBusDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F20F798
    ProtectUefiImageCommon - 0x1F20FCC0
    - 0x000000001CB01000 - 0x0000000000009B00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB0A840
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB0A7A0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB0A740
    PROGRESS CODE: V03040003 I0
    Loading driver 2D2E62CF-9ECF-43B7-8219-94E7FC713DFE
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F20F3C0
    Loading driver at 0x0001CB32000 EntryPoint=0x0001CB36BC7 UsbKbDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F20F718
    ProtectUefiImageCommon - 0x1F20F3C0
    - 0x000000001CB32000 - 0x0000000000006500
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB37B40
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB38320
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB38300
    PROGRESS CODE: V03040003 I0
    Loading driver 9FB4B4A7-42C0-4BCD-8540-9BCC6711F83E
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F20E0C0
    Loading driver at 0x0001CB43000 EntryPoint=0x0001CB474C7 UsbMassStorageDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F20E898
    ProtectUefiImageCommon - 0x1F20E0C0
    - 0x000000001CB43000 - 0x0000000000005A00
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB487A0
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB48880
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB48780
    PROGRESS CODE: V03040003 I0
    Loading driver 2D2E62AA-9ECF-43B7-8219-94E7FC713DFE
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F20E540
    Loading driver at 0x0001CB97000 EntryPoint=0x0001CB99A91 UsbMouseDxe.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F20DF98
    ProtectUefiImageCommon - 0x1F20E540
    - 0x000000001CB97000 - 0x0000000000003740
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03040002 I0
    InstallProtocolInterface: 18A031AB-B443-4D1A-A5C0-0C09261E9F71 1CB9A580
    InstallProtocolInterface: 107A772C-D5E1-11D4-9A46-0090273FC14D 1CB9A5E0
    InstallProtocolInterface: 6A7A5CFF-E8D9-4F70-BADA-75AB3025CE14 1CB9A520
    PROGRESS CODE: V03040003 I0
    Driver A487A478-51EF-48AA-8794-7BEE2A0562F1 was discovered but not loaded!!
    PROGRESS CODE: V03041001 I0
    [Bds] Entry...
    [BdsDxe] Locate Variable Policy protocol - Success
    Variable Driver Auto Update Lang, Lang:eng, PlatformLang:en Status: Success
    PROGRESS CODE: V03051005 I0
    InstallReadyToLock  entering......
    DXE - Total Runtime Image Count: 0x8
    DXE - Dump Runtime Image Records:
    PcRtc.efi: 0x1F64B000 - 0x1F651000
    Code Section: 0x1F64C000 - 0x1F650000
    MonotonicCounterRuntimeDxe.efi: 0x1F651000 - 0x1F655000
    Code Section: 0x1F652000 - 0x1F654000
    CapsuleRuntimeDxe.efi: 0x1F655000 - 0x1F659000
    Code Section: 0x1F656000 - 0x1F658000
    VariableRuntimeDxe.efi: 0x1F659000 - 0x1F668000
    Code Section: 0x1F65A000 - 0x1F666000
    ResetSystemRuntimeDxe.efi: 0x1F668000 - 0x1F66E000
    Code Section: 0x1F669000 - 0x1F66C000
    RuntimeDxe.efi: 0x1F66E000 - 0x1F674000
    Code Section: 0x1F66F000 - 0x1F672000
    StatusCodeHandlerRuntimeDxe.efi: 0x1F674000 - 0x1F67A000
    Code Section: 0x1F675000 - 0x1F679000
    ReportStatusCodeRouterRuntimeDxe.efi: 0x1F67A000 - 0x1F680000
    Code Section: 0x1F67B000 - 0x1F67E000
    [Variable]END_OF_DXE is signaled
    Initialize variable error flag (FF)
    All EndOfDxe callbacks have returned successfully
    InstallReadyToLock  end
    [Bds]RegisterKeyNotify: 0017/0000 80000000/00 Success
    [Bds]RegisterKeyNotify: 0002/0000 80000000/00 Success
    [Bds]RegisterKeyNotify: 0000/000D 80000000/00 Success
    Terminal - Mode 0, Column = 80, Row = 25
    Terminal - Mode 1, Column = 80, Row = 50
    Terminal - Mode 2, Column = 100, Row = 31
    PROGRESS CODE: V01040001 I0
    InstallProtocolInterface: 387477C1-69C7-11D2-8E39-00A0C969723B 1F202040
    InstallProtocolInterface: DD9E7534-7762-4698-8C14-F58517A625AA 1F202128
    InstallProtocolInterface: 387477C2-69C7-11D2-8E39-00A0C969723B 1F202058
    InstallProtocolInterface: 09576E91-6D3F-11D2-8E39-00A0C969723B 1F203518
    InstallProtocolInterface: D3B36F2B-D551-11D4-9A46-0090273FC14D 0
    InstallProtocolInterface: D3B36F2C-D551-11D4-9A46-0090273FC14D 0
    InstallProtocolInterface: D3B36F2D-D551-11D4-9A46-0090273FC14D 0

        Esc or Down      to enter Boot Manager Menu.
        ENTER            to boot directly.

    [Bds]OsIndication: 0000000000000000
    [Bds]=============Begin Load Options Dumping ...=============
    Driver Options:
    SysPrep Options:
    Boot Options:
        Boot0000: Enter Setup 		 0x0109
        Boot0001: UEFI Shell 		 0x0001
    PlatformRecovery Options:
        PlatformRecovery0000: Default PlatformRecovery 		 0x0001
    [Bds]=============End Load Options Dumping=============
    [Bds]BdsWait ...Zzzzzzzzzzzz...
    [Bds]BdsWait(3)..Zzzz...
    [Bds]BdsWait(2)..Zzzz...
    [Bds]BdsWait(1)..Zzzz...
    [Bds]Exit the waiting!
    PROGRESS CODE: V03051007 I0
    [Bds]Stop Hotkey Service!
    [Bds]UnregisterKeyNotify: 0017/0000 Success
    [Bds]UnregisterKeyNotify: 0002/0000 Success
    [Bds]UnregisterKeyNotify: 0000/000D Success
    PROGRESS CODE: V03051001 I0
    Memory  Previous  Current    Next
    Type    Pages     Pages     Pages
    ======  ========  ========  ========
    09    00000019  00000002  00000019
    0A    00000004  00000000  00000004
    00    00000008  00000000  00000008
    06    00000100  0000004C  00000100
    05    00000100  00000035  00000100
    [Bds]Booting UEFI Shell
    [Bds] Expand Fv(8063C21A-8E58-4576-95CE-089E87975D23)/FvFile(7C04A583-9E3E-4F1C-AD65-E05268D0B4D1) -> Fv(8063C21A-8E58-4576-95CE-089E87975D23)/FvFile(7C04A583-9E3E-4F1C-AD65-E05268D0B4D1)
    PROGRESS CODE: V03058000 I0
    InstallProtocolInterface: 5B1B31A1-9562-11D2-8E3F-00A0C969723B 1F2001C0
    Loading driver at 0x0001C800000 EntryPoint=0x0001C82D831 Shell.efi
    InstallProtocolInterface: BC62157E-3E33-4FEC-9920-2D3B36D750DF 1F203018
    ProtectUefiImageCommon - 0x1F2001C0
    - 0x000000001C800000 - 0x00000000000F19C0
    !!!!!!!!  Image Section Alignment(0x40) does not match Required Alignment (0x1000)  !!!!!!!!
    ProtectUefiImage failed to create image properties record
    PROGRESS CODE: V03058001 I0
    InstallProtocolInterface: 387477C2-69C7-11D2-8E39-00A0C969723B 1CB26F20
    InstallProtocolInterface: 752F3136-4E16-4FDC-A22A-E5F46812F4CA 1CB25F98
    InstallProtocolInterface: 6302D008-7F9B-4F30-87AC-60C9FEF5DA4E 1C8894A0
    UEFI Interactive Shell v2.2
    EDK II
    UEFI v2.70 (EDK II, 0x00010000)
    Mapping table
    mmap: No mapping found.
    Press ESC in 5 seconds to skip startup.nsh or any other key to continue.
    Shell> echo "Hi from EDK2"
    Hi from EDK2
    Shell>

.. sectionauthor:: Simon Glass <sjg@chromium.org>
.. July 2024
