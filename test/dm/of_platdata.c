// SPDX-License-Identifier: GPL-2.0+

#include <common.h>
#include <dm.h>
#include <dm/test.h>
#include <test/test.h>
#include <test/ut.h>

/* Test that we can find a device using of-platdata */
static int dm_test_of_platdata_base(struct unit_test_state *uts)
{
	struct udevice *dev;

	ut_assertok(uclass_first_device_err(UCLASS_SERIAL, &dev));
	ut_asserteq_str("sandbox_serial", dev->name);

	return 0;
}
DM_TEST(dm_test_of_platdata_base, UT_TESTF_SCAN_PDATA);

/**
 * find_driver_info - recursively find the driver_info for a device
 *
 * This sets found[idx] to true when it finds the driver_info record for a
 * device, where idx is the index in the driver_info linker list.
 *
 * @uts: Test state
 * @parent: Parent to search
 * @found: bool array to update
 * @return 0 if OK, non-zero on error
 */
static int find_driver_info(struct unit_test_state *uts, struct udevice *parent,
			    bool found[])
{
	struct udevice *dev;

	/* If not the root device, find the entry that caused it to be bound */
	if (parent->parent) {
		const int n_ents =
			ll_entry_count(struct driver_info, driver_info);
		int idx = -1;
		int i;

		for (i = 0; i < n_ents; i++) {
			const struct driver_rt *drt = gd_dm_driver_rt() + i;

			if (drt->dev == parent) {
				idx = i;
				found[idx] = true;
				break;
			}
		}

		ut_assert(idx != -1);
	}

	device_foreach_child(dev, parent) {
		int ret;

		ret = find_driver_info(uts, dev, found);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/* Check that every device is recorded in its driver_info struct */
static int dm_test_of_platdata_dev(struct unit_test_state *uts)
{
	const struct driver_info *info =
		ll_entry_start(struct driver_info, driver_info);
	const int n_ents = ll_entry_count(struct driver_info, driver_info);
	bool found[n_ents];
	uint i;

	/* Record the indexes that are found */
	memset(found, '\0', sizeof(found));
	ut_assertok(find_driver_info(uts, gd->dm_root, found));

	/* Make sure that the driver entries without devices have no ->dev */
	for (i = 0; i < n_ents; i++) {
		const struct driver_rt *drt = gd_dm_driver_rt() + i;
		const struct driver_info *entry = info + i;
		struct udevice *dev;

		if (found[i]) {
			/* Make sure we can find it */
			ut_assertnonnull(drt->dev);
			ut_assertok(device_get_by_driver_info(entry, &dev));
			ut_asserteq_ptr(dev, drt->dev);
		} else {
			ut_assertnull(drt->dev);
			ut_asserteq(-ENOENT,
				    device_get_by_driver_info(entry, &dev));
		}
	}

	return 0;
}
DM_TEST(dm_test_of_platdata_dev, UT_TESTF_SCAN_PDATA);
