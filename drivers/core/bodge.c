// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Google LLC
 *
 * Written by Simon Glass <sjg@chromium.org>
 */

#include <dm.h>
#include <dm/root.h>
#include <dm/device-internal.h>

/*
 * dm_bodge_probe() - handle auto-probing of particular devices
 *
 * The code in here should be removed in favour of using a sysinfo driver to
 * probe required devices.
 *
 * See the probe-devices property or manually probe devices in the sysinfo
 * probe() method.
 *
 * For other users please send an email to the mailing list and cc the
 * driver model maintainer
 */
static int dm_bodge_probe(bool pre_reloc_only)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	/* probe all the GPIO hogs that were bound  */
	if (CONFIG_IS_ENABLED(GPIO_HOG)) {
		uclass_id_foreach_dev(UCLASS_NOP, dev, uc) {
			if (!strcmp(dev->driver->name, "gpio_hog")) {
				ret = device_probe(dev);
				/* ignore the error as per previous code */
			}
		}
	}

	/*
	 * From PSCI v1.0 onward we can discover services through
	 * ARM_SMCCC_FEATURE
	 *
	 * Unfortunately this does not have its own uclass so we need to search
	 * for it.
	 */
	if (CONFIG_IS_ENABLED(ARM_PSCI_FW)) {
		uclass_id_foreach_dev(UCLASS_FIRMWARE, dev, uc) {
			if (!strcmp(dev->driver->name, "pcsi") &&
			    device_is_compatible(dev, "arm,psci-1.0")) {
				ret = device_probe(dev);
				/* ignore the error as per previous code */
			}
		}
	}

	if (CONFIG_IS_ENABLED(LED)) {
		/*
		 * In case the LED has default-state DT property, trigger
		 * probe() to configure its default state during startup.
		 */
		ret = uclass_probe_all(UCLASS_LED);
		/* ignore the error as per previous code */
	}

	/*
	 * According to the Hardware Design Guide, IO-domain configuration must
	 * be consistent with the power supply voltage (1.8V or 3.3V).
	 * Probe after bind to configure IO-domain voltage early during boot.
	 *
	 * Unfortunately this does not have its own uclass so we need to search
	 * for it.
	 */
	if (CONFIG_IS_ENABLED(ROCKCHIP_IODOMAIN)) {
		uclass_id_foreach_dev(UCLASS_NOP, dev, uc) {
			if (!strcmp(dev->driver->name, "rockchip_iodomain")) {
				ret = device_probe(dev);
				/* ignore the error as per previous code */
			}
		}
	}

	if (CONFIG_IS_ENABLED(NVMXIP)) {
		/* The original code has no comment */
		ret = uclass_probe_all(UCLASS_NVMXIP);
		/* ignore the error as per previous code */
	}

	if (CONFIG_IS_ENABLED(PINCTRL_ARMADA_37XX)) {
		/*
		 * Make sure that the pinctrl driver gets probed after binding
		 * as on A37XX the pinctrl driver is the one that is also
		 * registering the GPIO one during probe, so if its not probed
		 * GPIO-s are not registered as well.
		 *
		 * Assume that there is only one pinctrl driver in use
		 */
		ret = uclass_probe_all(UCLASS_PINCTRL);
		/* ignore the error as per previous code */
	}

	if (CONFIG_IS_ENABLED(PMIC_RK8XX) && CONFIG_IS_ENABLED(PMIC_CHILDREN) &&
	    IS_ENABLED(CONFIG_SPL_BUILD) &&
	    IS_ENABLED(CONFIG_ROCKCHIP_RK8XX_DISABLE_BOOT_ON_POWERON)) {
		/*
		 * The original code has no comment
		 *
		 * Assume that there is only one PMIC in the system
		 */
		ret = uclass_probe_all(UCLASS_PMIC);
		/* ignore the error as per previous code */
	}

	if (CONFIG_IS_ENABLED(OPTEE_SERVICE_DISCOVERY)) {
		/*
		 * The original code has no comment
		 *
		 * Assume that there is only one TEE in the system
		 */
		ret = uclass_probe_all(UCLASS_TEE);
		/* ignore the error as per previous code */
	}

	return 0;
}

int dm_bodge(bool pre_reloc_only)
{
	int ret;

	if (IS_ENABLED(CONFIG_DM_BODGE_AUTO_PROBE)) {
		ret = dm_bodge_probe(pre_reloc_only);
		if (ret)
			return ret;
	}

	return 0;
}
