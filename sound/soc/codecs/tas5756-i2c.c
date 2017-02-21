/*
 * Driver for the TAS5756 CODECs
 *
 * Author:	Mark Brown <broonie@linaro.org>
 *		Copyright 2014 Linaro Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

#include "tas5756.h"

static int tas5756_i2c_probe(struct i2c_client *i2c,
			     const struct i2c_device_id *id)
{
	struct regmap *regmap;
	struct regmap_config config = tas5756_regmap;

	/* msb needs to be set to enable auto-increment of addresses */
	config.read_flag_mask = 0x80;
	config.write_flag_mask = 0x80;

	regmap = devm_regmap_init_i2c(i2c, &config);
	if (IS_ERR(regmap))
		return PTR_ERR(regmap);

	return tas5756_probe(&i2c->dev, regmap);
}

static int tas5756_i2c_remove(struct i2c_client *i2c)
{
	tas5756_remove(&i2c->dev);
	return 0;
}

static const struct i2c_device_id tas5756_i2c_id[] = {
	{ "tas5756", },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tas5756_i2c_id);

static const struct of_device_id tas5756_of_match[] = {
	{ .compatible = "ti,tas5756", },
	{ }
};
MODULE_DEVICE_TABLE(of, tas5756_of_match);

static struct i2c_driver tas5756_i2c_driver = {
	.probe 		= tas5756_i2c_probe,
	.remove 	= tas5756_i2c_remove,
	.id_table	= tas5756_i2c_id,
	.driver		= {
		.name	= "tas5756",
		.owner	= THIS_MODULE,
		.of_match_table = tas5756_of_match,
		.pm     = &tas5756_pm_ops,
	},
};

module_i2c_driver(tas5756_i2c_driver);

MODULE_DESCRIPTION("ASoC TAS5756 codec driver - I2C");
MODULE_AUTHOR("Mark Brown <broonie@linaro.org>");
MODULE_LICENSE("GPL v2");
