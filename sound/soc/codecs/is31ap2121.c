/*
 * ASoC Driver for IS31AP2121
 *
 * Author:      Philip Voigt <info at polyvection.com>
 *              Copyright 2017
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "is31ap2121.h"


static struct i2c_client *i2c;

#define IS31AP2121_NUM_SUPPLIES 1
static const char * const is31ap2121_supply_names[IS31AP2121_NUM_SUPPLIES] = {
	"amppd_reg",
};

struct is31ap2121_priv {
	struct regmap *regmap;
	struct snd_soc_codec *codec;
	struct regulator_bulk_data supplies[IS31AP2121_NUM_SUPPLIES];
};

static struct is31ap2121_priv *priv_data;


static const DECLARE_TLV_DB_SCALE(is31ap2121_vol_tlv, -10350, 50, 1);


static const struct snd_kcontrol_new is31ap2121_snd_controls[] = {
	SOC_SINGLE_TLV  ("Digital Playback Volume", AP2121_MASTER_VOL, 0, 255, 1, is31ap2121_vol_tlv),
	SOC_SINGLE_TLV("Channel 1"  , AP2121_CH1_VOL, 0, 255, 1, is31ap2121_vol_tlv),
	SOC_SINGLE_TLV("Channel 2"  , AP2121_CH2_VOL, 0, 255, 1, is31ap2121_vol_tlv)
};


static int is31ap2121_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params,
			    struct snd_soc_dai *dai)
{
	return 0;
}


static int is31ap2121_mute_stream(struct snd_soc_dai *dai, int mute, int stream)
{
	unsigned int val = 0;

	struct is31ap2121_priv *is31ap2121;
	struct snd_soc_codec *codec = dai->codec;
	is31ap2121 = snd_soc_codec_get_drvdata(codec);

	if (mute) {
		val = AP2121_MASTER_MUTE;
	}

	return regmap_write(is31ap2121->regmap, AP2121_STATE_CTL_3, val);
}


static const struct snd_soc_dai_ops is31ap2121_dai_ops = {
	.hw_params 		= is31ap2121_hw_params,
	.mute_stream		= is31ap2121_mute_stream,
};


static struct snd_soc_dai_driver is31ap2121_dai = {
	.name		= "is31ap2121-hifi",
	.playback 	= {
		.stream_name	= "Playback",
		.channels_min	= 2,
		.channels_max	= 2,
		.rates		    = SNDRV_PCM_RATE_8000_192000,
		.formats	    = (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE ),
	},
	.ops        = &is31ap2121_dai_ops,
};


static int is31ap2121_remove(struct snd_soc_codec *codec)
{
	struct is31ap2121_priv *is31ap2121;

	is31ap2121 = snd_soc_codec_get_drvdata(codec);

	return 0;
}


static int is31ap2121_probe(struct snd_soc_codec *codec)
{
	struct is31ap2121_priv *is31ap2121;
	int i, ret;

	i2c = container_of(codec->dev, struct i2c_client, dev);

	is31ap2121 = snd_soc_codec_get_drvdata(codec);

	// Unmute
	ret = snd_soc_write(codec, AP2121_STATE_CTL_3, 0x00);
	if (ret < 0) return ret;

	// Set volume to 0db
	ret = snd_soc_write(codec, AP2121_MASTER_VOL, 0x18);
	if (ret < 0) return ret;


	return 0;
}


static struct snd_soc_codec_driver soc_codec_dev_is31ap2121 = {
	.probe = is31ap2121_probe,
	.remove = is31ap2121_remove,
	.component_driver = {
		.controls = is31ap2121_snd_controls,
		.num_controls = ARRAY_SIZE(is31ap2121_snd_controls),
	},
};


static const struct reg_default is31ap2121_reg_defaults[] = {
	{ 0x03 ,0x60 },     // R3  - VOL_MASTER    - -40dB
	{ 0x04 ,0x18 },     // R4  - VOL_CH1	   -   0dB
	{ 0x05 ,0x18 },     // R5  - VOL_CH2       -   0dB
};


static const struct of_device_id is31ap2121_of_match[] = {
	{ .compatible = "issi,is31ap2121", },
	{ }
};
MODULE_DEVICE_TABLE(of, is31ap2121_of_match);


static struct regmap_config is31ap2121_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AP2121_MAX_REGISTER,

	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = is31ap2121_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(is31ap2121_reg_defaults),
};


static int is31ap2121_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int ret, i;

	struct is31ap2121_priv *is31ap2121;

	is31ap2121 = devm_kzalloc(&i2c->dev, sizeof(struct is31ap2121_priv),
			      GFP_KERNEL);

	is31ap2121->regmap = devm_regmap_init_i2c(i2c, &is31ap2121_regmap_config);
	if (IS_ERR(is31ap2121->regmap)) {
		ret = PTR_ERR(is31ap2121->regmap);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(is31ap2121->supplies); i++)
		is31ap2121->supplies[i].supply = is31ap2121_supply_names[i];

	ret = devm_regulator_bulk_get(&i2c->dev, ARRAY_SIZE(is31ap2121->supplies),
				      is31ap2121->supplies);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to request supplies: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(is31ap2121->supplies),
				    is31ap2121->supplies);
	if (ret != 0) {
		dev_err(&i2c->dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(i2c, is31ap2121);

	ret = snd_soc_register_codec(&i2c->dev,
				     &soc_codec_dev_is31ap2121, &is31ap2121_dai, 1);

	return ret;
}


static int is31ap2121_i2c_remove(struct i2c_client *i2c)
{
	snd_soc_unregister_codec(&i2c->dev);
	i2c_set_clientdata(i2c, NULL);

	kfree(priv_data);

	return 0;
}


static const struct i2c_device_id is31ap2121_i2c_id[] = {
	{ "is31ap2121", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, is31ap2121_i2c_id);


static struct i2c_driver is31ap2121_i2c_driver = {
	.driver = {
		.name = "is31ap2121",
		.owner = THIS_MODULE,
		.of_match_table = is31ap2121_of_match,
	},
	.probe = is31ap2121_i2c_probe,
	.remove = is31ap2121_i2c_remove,
	.id_table = is31ap2121_i2c_id
};


static int __init is31ap2121_modinit(void)
{
	int ret = 0;

	ret = i2c_add_driver(&is31ap2121_i2c_driver);
	if (ret) {
		printk(KERN_ERR "Failed to register is31ap2121 I2C driver: %d\n",
		       ret);
	}

	return ret;
}
module_init(is31ap2121_modinit);


static void __exit is31ap2121_exit(void)
{
	i2c_del_driver(&is31ap2121_i2c_driver);
}
module_exit(is31ap2121_exit);


MODULE_AUTHOR("Philip Voigt <pv at polyvetion.com>");
MODULE_DESCRIPTION("ASoC driver for is31ap2121");
MODULE_LICENSE("GPL v2");
