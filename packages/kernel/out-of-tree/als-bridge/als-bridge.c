// SPDX-License-Identifier: GPL-2.0
/*
 * als-bridge - minimal IIO light sensor backed by a value published from
 * userspace.
 *
 * Some Qualcomm ambient light sensors are only reachable through the SSC
 * (iio-sensor-proxy/libssc over QRTR), which serves D-Bus clients fine.
 * Steam, however, reads
 * /sys/bus/iio/devices/iio:deviceN/in_illuminance_raw directly and greys out
 * its adaptive-brightness switch when nothing is there. This bridge exposes
 * exactly that interface; the als-bridge daemon writes lux values read from
 * net.hadess.SensorProxy into the module parameter.
 */

#include <linux/iio/iio.h>
#include <linux/module.h>
#include <linux/platform_device.h>

static unsigned int lux;
module_param(lux, uint, 0644);
MODULE_PARM_DESC(lux, "ambient light level in lux, published by the als-bridge daemon");

static int alsbridge_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val, int *val2, long mask)
{
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*val = READ_ONCE(lux);
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
}

static const struct iio_chan_spec alsbridge_channels[] = {
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
	},
};

static const struct iio_info alsbridge_info = {
	.read_raw = alsbridge_read_raw,
};

/* Steam accepts an IIO device named "als" with in_illuminance_raw in lux. */
static int alsbridge_probe(struct platform_device *pdev)
{
	struct iio_dev *indio_dev;

	indio_dev = devm_iio_device_alloc(&pdev->dev, 0);
	if (!indio_dev)
		return -ENOMEM;

	indio_dev->name = "als";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = alsbridge_channels;
	indio_dev->num_channels = ARRAY_SIZE(alsbridge_channels);
	indio_dev->info = &alsbridge_info;

	return devm_iio_device_register(&pdev->dev, indio_dev);
}

static const struct platform_device_id alsbridge_id[] = {
	{ .name = "als-bridge" },
	{ }
};

static struct platform_driver alsbridge_driver = {
	.driver = { .name = "als-bridge" },
	.probe = alsbridge_probe,
	.id_table = alsbridge_id,
};

static struct platform_device *alsbridge_pdev;

static int __init alsbridge_init(void)
{
	int ret;

	ret = platform_driver_register(&alsbridge_driver);
	if (ret)
		return ret;

	alsbridge_pdev = platform_device_register_simple("als-bridge", -1, NULL, 0);
	if (IS_ERR(alsbridge_pdev)) {
		platform_driver_unregister(&alsbridge_driver);
		return PTR_ERR(alsbridge_pdev);
	}

	return 0;
}

static void __exit alsbridge_exit(void)
{
	platform_device_unregister(alsbridge_pdev);
	platform_driver_unregister(&alsbridge_driver);
}

module_init(alsbridge_init);
module_exit(alsbridge_exit);

MODULE_DESCRIPTION("IIO illuminance bridge fed by the als-bridge userspace daemon");
MODULE_LICENSE("GPL");
