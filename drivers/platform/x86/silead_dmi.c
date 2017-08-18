/*
 * Silead touchscreen driver DMI based configuration code
 *
 * Copyright (c) 2017 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Red Hat authors:
 * Hans de Goede <hdegoede@redhat.com>
 */

#include <linux/acpi.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/i2c.h>
#include <linux/notifier.h>
#include <linux/property.h>
#include <linux/string.h>

struct silead_ts_dmi_data {
	const char *acpi_name;
	const struct property_entry *properties;
};

static const struct property_entry cube_iwork8_air_props[] = {
	PROPERTY_ENTRY_U32("touchscreen-size-x", 1660),
	PROPERTY_ENTRY_U32("touchscreen-size-y", 900),
	PROPERTY_ENTRY_BOOL("touchscreen-swapped-x-y"),
	PROPERTY_ENTRY_STRING("firmware-name", "gsl3670-cube-iwork8-air.fw"),
	PROPERTY_ENTRY_U32("silead,max-fingers", 10),
	{ }
};

static const struct silead_ts_dmi_data cube_iwork8_air_data = {
	.acpi_name	= "MSSL1680:00",
	.properties	= cube_iwork8_air_props,
};

static const struct property_entry jumper_ezpad_mini3_props[] = {
	PROPERTY_ENTRY_U32("touchscreen-size-x", 1700),
	PROPERTY_ENTRY_U32("touchscreen-size-y", 1150),
	PROPERTY_ENTRY_BOOL("touchscreen-swapped-x-y"),
	PROPERTY_ENTRY_STRING("firmware-name", "gsl3676-jumper-ezpad-mini3.fw"),
	PROPERTY_ENTRY_U32("silead,max-fingers", 10),
	{ }
};

static const struct silead_ts_dmi_data jumper_ezpad_mini3_data = {
	.acpi_name	= "MSSL1680:00",
	.properties	= jumper_ezpad_mini3_props,
};

static const struct property_entry dexp_ursus_7w_props[] = {
	PROPERTY_ENTRY_U32("touchscreen-size-x", 890),
	PROPERTY_ENTRY_U32("touchscreen-size-y", 630),
	PROPERTY_ENTRY_STRING("firmware-name", "gsl1686-dexp-ursus-7w.fw"),
	PROPERTY_ENTRY_U32("silead,max-fingers", 10),
	{ }
};

static const struct silead_ts_dmi_data dexp_ursus_7w_data = {
	.acpi_name	= "MSSL1680:00",
	.properties	= dexp_ursus_7w_props,
};

static const struct property_entry surftab_wintron70_st70416_6_props[] = {
	PROPERTY_ENTRY_U32("touchscreen-size-x", 884),
	PROPERTY_ENTRY_U32("touchscreen-size-y", 632),
	PROPERTY_ENTRY_STRING("firmware-name",
			      "gsl1686-surftab-wintron70-st70416-6.fw"),
	PROPERTY_ENTRY_U32("silead,max-fingers", 10),
	{ }
};

static const struct silead_ts_dmi_data surftab_wintron70_st70416_6_data = {
	.acpi_name	= "MSSL1680:00",
	.properties	= surftab_wintron70_st70416_6_props,
};

static const struct dmi_system_id silead_ts_dmi_table[] = {
	{
		/* CUBE iwork8 Air */
		.driver_data = (void *)&cube_iwork8_air_data,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "cube"),
			DMI_MATCH(DMI_PRODUCT_NAME, "i1-TF"),
			DMI_MATCH(DMI_BOARD_NAME, "Cherry Trail CR"),
		},
	},
	{
		/* Jumper EZpad mini3 */
		.driver_data = (void *)&jumper_ezpad_mini3_data,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			/* jumperx.T87.KFBNEEA02 with the version-nr dropped */
			DMI_MATCH(DMI_BIOS_VERSION, "jumperx.T87.KFBNEEA"),
		},
	},
	{
		/* DEXP Ursus 7W */
		.driver_data = (void *)&dexp_ursus_7w_data,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "7W"),
		},
	},
	{
		/* Trekstor Surftab Wintron 7.0 ST70416-6 */
		.driver_data = (void *)&surftab_wintron70_st70416_6_data,
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Insyde"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ST70416-6"),
			/* Exact match, different versions need different fw */
			DMI_MATCH(DMI_BIOS_VERSION, "TREK.G.WI71C.JGBMRBA04"),
		},
	},
	{ },
};

static const struct silead_ts_dmi_data *silead_ts_data;

static void silead_ts_dmi_add_props(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	int error;

	if (has_acpi_companion(dev) &&
	    !strncmp(silead_ts_data->acpi_name, client->name, I2C_NAME_SIZE)) {
		error = device_add_properties(dev, silead_ts_data->properties);
		if (error)
			dev_err(dev, "failed to add properties: %d\n", error);
	}
}

static int silead_ts_dmi_notifier_call(struct notifier_block *nb,
				       unsigned long action, void *data)
{
	struct device *dev = data;
	struct i2c_client *client;

	switch (action) {
	case BUS_NOTIFY_ADD_DEVICE:
		client = i2c_verify_client(dev);
		if (client)
			silead_ts_dmi_add_props(client);
		break;

	default:
		break;
	}

	return 0;
}

static struct notifier_block silead_ts_dmi_notifier = {
	.notifier_call = silead_ts_dmi_notifier_call,
};

static int __init silead_ts_dmi_init(void)
{
	const struct dmi_system_id *dmi_id;
	int error;

	dmi_id = dmi_first_match(silead_ts_dmi_table);
	if (!dmi_id)
		return 0; /* Not an error */

	silead_ts_data = dmi_id->driver_data;

	error = bus_register_notifier(&i2c_bus_type, &silead_ts_dmi_notifier);
	if (error)
		pr_err("%s: failed to register i2c bus notifier: %d\n",
			__func__, error);

	return error;
}

/*
 * We are registering out notifier after i2c core is initialized and i2c bus
 * itself is ready (which happens at postcore initcall level), but before
 * ACPI starts enumerating devices (at subsys initcall level).
 */
arch_initcall(silead_ts_dmi_init);
