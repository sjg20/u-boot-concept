#include <common.h>
#include <asm/arch/nvboot_bit.h>

const char *boot_dev_type[] = {
    [NvBootDevType_None]		= "None",
    [NvBootDevType_Nand]		= "Nand",
    [NvBootDevType_Snor]		= "Snor",
    [NvBootDevType_Spi]			= "Spi",
    [NvBootDevType_Sdmmc]		= "Sdmmc",
    [NvBootDevType_Irom]		= "Irom",
    [NvBootDevType_Uart]		= "Uart",
    [NvBootDevType_Usb]			= "Usb",
    [NvBootDevType_Nand_x16]		= "Nand_x16",
    [NvBootDevType_MuxOneNand]		= "MuxOneNand",
    [NvBootDevType_MobileLbaNand]	= "MobileLbaNand",
};

const char *boot_type[] = {
    [NvBootType_None]		= "None",
    [NvBootType_Cold]		= "Cold",
    [NvBootType_Recovery]	= "Recovery",
    [NvBootType_Uart]		= "Uart",
};

const char *boot_rdr_status[] = {
    [NvBootRdrStatus_None] = "None",
    [NvBootRdrStatus_Success] = "Success",
    [NvBootRdrStatus_ValidationFailure] = "ValidationFailure",
    [NvBootRdrStatus_DeviceReadError] = "DeviceReadError",
};

static void dump_boot_bl_state(struct NvBootBlStateRec *bl_state)
{
	printf("BootBlState:\n");
	printf("\tStatus = %s\n", boot_rdr_status[bl_state->Status]);
	printf("\tFirstEccBlock = %d\n", bl_state->FirstEccBlock);
	printf("\tFirstEccPage = %d\n", bl_state->FirstEccPage);
	printf("\tFirstCorrectedEccBlock = %d\n", bl_state->FirstCorrectedEccBlock);
	printf("\tFirstCorrectedEccPage = %d\n", bl_state->FirstCorrectedEccPage);
	printf("\tHadEccError = %d\n", bl_state->HadEccError);
	printf("\tHadCrcError = %d\n", bl_state->HadCrcError);
	printf("\tHadCorrectedEccError = %d\n", bl_state->HadCorrectedEccError);
	printf("\tUsedForEccRecovery = %d\n", bl_state->UsedForEccRecovery);
}

static void dump_boot_nand_status(struct NvBootNandStatusRec *nand)
{
	printf("BootNandStatus:\n");
	printf("\tFuseDataWidth = %d\n", nand->FuseDataWidth);
	printf("\tFuseNumAddressCycles = %d\n", nand->FuseNumAddressCycles);
	printf("\tFuseDisableOnfiSupport = %d\n", nand->FuseDisableOnfiSupport);
	printf("\tFuseEccSelection = %d\n", nand->FuseEccSelection);
	printf("\tFusePageSizeOffset = %d\n", nand->FusePageSizeOffset);
	printf("\tFuseBlockSizeOffset = %d\n", nand->FuseBlockSizeOffset);
	printf("\tFusePinmuxSelection = %d\n", nand->FusePinmuxSelection);
	printf("\tFusePinOrder = %d\n", nand->FusePinOrder);
	printf("\tDiscoveredDataWidth = %d\n", nand->DiscoveredDataWidth);
	printf("\tDiscoveredNumAddressCycles = %d\n", nand->DiscoveredNumAddressCycles);
	printf("\tDiscoveredEccSelection = %d\n", nand->DiscoveredEccSelection);
	printf("\tIdRead = %d\n", nand->IdRead);
	printf("\tIsPartOnfi = %d\n", nand->IsPartOnfi);
	printf("\tNumPagesRead = %d\n", nand->NumPagesRead);
	printf("\tNumUncorrectableErrorPages = %d\n", nand->NumUncorrectableErrorPages);
	printf("\tNumCorrectableErrorPages = %d\n", nand->NumCorrectableErrorPages);
	printf("\tMaxCorrectableErrorsEncountered = %d\n", nand->MaxCorrectableErrorsEncountered);
}

static void dump_boot_spi_status(NvBootSpiFlashStatus *spi)
{
	printf("SpiStatus:\n");
	printf("\tClockSource = %d\n", spi->ClockSource);
	printf("\tClockDivider = %d\n", spi->ClockDivider);
	printf("\tIsFastRead = %d\n", spi->IsFastRead);
	printf("\tNumPagesRead = %d\n", spi->NumPagesRead);
	printf("\tLastBlockRead = %d\n", spi->LastBlockRead);
	printf("\tLastPageRead = %d\n", spi->LastPageRead);
	printf("\tBootStatus = %d\n", spi->BootStatus); 
	printf("\tInitStatus = %d\n", spi->InitStatus); 
	printf("\tReadStatus = %d\n", spi->ReadStatus);
	printf("\tParamsValidated = %d\n", spi->ParamsValidated);
}

static void dump_boot_sdmmc_status(NvBootSdmmcStatus *sdmmc)
{
	int i;

	printf("SdmmcStatus:\n");
	printf("\tFuseDataWidth = %d\n", sdmmc->FuseDataWidth);
	printf("\tFuseCardType = %d\n", sdmmc->FuseCardType);
	printf("\tFuseVoltageRange = %d\n", sdmmc->FuseVoltageRange);
	printf("\tFuseDisableBootMode = %d\n", sdmmc->FuseDisableBootMode);
	printf("\tFusePinmuxSelection = %d\n", sdmmc->FusePinmuxSelection);
	printf("\tDiscoveredCardType = %d\n", sdmmc->DiscoveredCardType);
	printf("\tDiscoveredVoltageRange = %d\n", sdmmc->DiscoveredVoltageRange);
	printf("\tDataWidthUnderUse = %d\n", sdmmc->DataWidthUnderUse);
	printf("\tPowerClassUnderUse = %d\n", sdmmc->PowerClassUnderUse);
	
	printf("\tCid = ");
	for (i = 0; i < 4; i++)
		printf("%08x ", sdmmc->Cid[i]);
	printf("\n");

	printf("\tNumPagesRead = %d\n", sdmmc->NumPagesRead);
	printf("\tNumCrcErrors = %d\n", sdmmc->NumCrcErrors);
	printf("\tBootFromBootPartition = %d\n", sdmmc->BootFromBootPartition);
	printf("\tBootModeReadSuccessful = %d\n", sdmmc->BootModeReadSuccessful);
}

#define DEBUG

void dump_boot_info_table(void *_bit)
{
#ifdef DEBUG
	NvBootInfoTable *bit = _bit;
	struct NvBootNandStatusRec *nand;
	NvBootSpiFlashStatus *spi;
	NvBootSdmmcStatus *sdmmc;
	int i;

	printf("BootInfoTable:\n");
	printf("\tBootRomVersion = %x\n", bit->BootRomVersion);;
	printf("\tDataVersion = %x\n", bit->DataVersion);;
	printf("\tRcmVersion = %x\n", bit->RcmVersion);;

	printf("\tBootType = %s\n", boot_type[bit->BootType]);;
	printf("\tPrimaryDevice = %s\n", boot_dev_type[bit->PrimaryDevice]);;
	printf("\tSecondaryDevice = %s\n", boot_dev_type[bit->SecondaryDevice]);;

	printf("\tDevInitialized = %d\n", bit->DevInitialized);;
	printf("\tSdramInitialized = %d\n", bit->SdramInitialized);;
	printf("\tClearedForceRecovery = %d\n", bit->ClearedForceRecovery);;
	printf("\tClearedFailBack = %d\n", bit->ClearedFailBack);;
	printf("\tInvokedFailBack = %d\n", bit->InvokedFailBack);;
	printf("\tBctValid = %d\n", bit->BctValid);;

	printf("bct status:");
	for (i = 0; i < NVBOOT_BCT_STATUS_BYTES; i++)
		printf("%d ", bit->BctStatus[i]);
	printf("\n");

	printf("\tBctLastJournalRead = %s\n", boot_rdr_status[bit->BctLastJournalRead]);

	printf("\tBctBlock = %d\n", bit->BctBlock);;
	printf("\tBctPage = %d\n", bit->BctPage);; 
	printf("\tBctSize = %d\n", bit->BctSize);;  /* 0 until BCT loading is attempted */

	for (i = 0; i < NVBOOT_MAX_BOOTLOADERS; i++) {
		printf("BlState (%d)\n", i);
		dump_boot_bl_state(&bit->BlState[i]);
	}

	nand = &bit->SecondaryDevStatus.NandStatus;
	dump_boot_nand_status(nand);
	
	spi = &bit->SecondaryDevStatus.SpiStatus;
	dump_boot_spi_status(spi);

	sdmmc = &bit->SecondaryDevStatus.SdmmcStatus;
	dump_boot_sdmmc_status(sdmmc);

	printf("\tSafeStartAddr = %x\n", bit->SafeStartAddr);;
#endif
}

