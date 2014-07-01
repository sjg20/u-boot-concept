/*
 * Fixed PHY driver
 *
 * SPDX-License-Identifier:	GPL-2.0+
 *
 */
#include <miiphy.h>

static int fixed_startup(struct phy_device *phydev)
{
	phydev->speed = CONFIG_SYS_FIXED_PHY_SPEED;
	phydev->duplex = CONFIG_SYS_FIXED_PHY_DUPLEX;
	phydev->autoneg = AUTONEG_DISABLE;
	return 0;
}

static struct phy_driver fixed_driver = {
	.name = "Fixed PHY",
	.uid = 0xffffffff,
	.mask = 0xffffffff,
	.features = PHY_GBIT_FEATURES,
	.startup = &fixed_startup,
	.shutdown = &genphy_shutdown,
};

int phy_fixed_init(void)
{
	phy_register(&fixed_driver);

	return 0;
}
