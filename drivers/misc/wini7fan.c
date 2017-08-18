#include <linux/module.h>
#include <linux/platform_device.h>


#include <linux/irq.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include<asm/io.h>


#define GPIOFANADR1      0xfed8c400
#define GPIOFANADR2      0xfed8c408
struct gpio_fan_data {
	struct platform_device	*pdev;
	struct device		*hwmon_dev;
	
};


static struct acpi_device_id wini7fan_acpi_match[] = {
	{"FAN02501",0},
	{ },
};

static int wini7fan_probe(struct platform_device *pdev)
{	
	/*
	struct gpio_desc *gpiod;
	struct device *dev = &pdev->dev;
		if (!ACPI_HANDLE(dev))
		return -ENODEV;
		const struct acpi_device_id *acpi_id;
	acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
	if (!acpi_id) {
		dev_err(dev, "failed to get ACPI info\n");
		return -ENODEV;
	}
		gpiod = devm_gpiod_get_index(dev, "fan_1", 0);
	if (IS_ERR(gpiod)) {
		int err = PTR_ERR(gpiod);
		printk("get fan_1 failed: %d\n", err);
		return err;
	}
	gpiod_direction_input(gpiod);
	gpiod_direction_output(gpiod, 1);
	gpiod_direction_input(gpiod);
	*/
	static unsigned int *gpd0con;  
	static unsigned int *gpd1con;  
	gpd0con = ioremap(GPIOFANADR1,4); 
	gpd1con = ioremap(GPIOFANADR2,4);
	writel(readl(gpd0con)+2,gpd0con);
	writel(readl(gpd1con)+2,gpd1con);
	return 0;
	
}

static int wini7fan_remove(struct platform_device *pdev)
{
	return 0;
}

MODULE_DEVICE_TABLE(acpi, wini7fan_acpi_match);
static struct platform_driver wini7fan_driver = {
	.probe		= wini7fan_probe,
	.remove		= wini7fan_remove,
	.driver	= {
		.name	= "FAN0",
		.acpi_match_table = ACPI_PTR(wini7fan_acpi_match),
	},
};

module_platform_driver(wini7fan_driver);

MODULE_AUTHOR("yangweili@wisky.com.cn");
MODULE_DESCRIPTION("wini7fan_driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wini7fan_driver");
