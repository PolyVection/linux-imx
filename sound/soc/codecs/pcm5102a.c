/*
 * Simple codec for the PCM5102A DAC.
 *
 * Copyright 2016 Philip Voigt <info[at]polyvection[dot]com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/soc.h>

static struct snd_soc_dai_driver pcm5102a_dai = {
	.name = "pcm5102a-hifi",
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_192000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			SNDRV_PCM_FMTBIT_S24_LE
	},
};

static struct snd_soc_codec_driver soc_codec_dev_pcm5102a;

static int pcm5102a_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev, &soc_codec_dev_pcm5102a,
			&pcm5102a_dai, 1);
}

static int pcm5102a_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static const struct of_device_id pcm5102a_of_match[] = {
	{ .compatible = "ti,pcm5102a", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm5102a_of_match);

static struct platform_driver pcm5102a_codec_driver = {
	.probe 		= pcm5102a_probe,
	.remove 	= pcm5102a_remove,
	.driver		= {
		.name	= "pcm5102a-codec",
		.owner	= THIS_MODULE,
		.of_match_table = pcm5102a_of_match,
	},
};

module_platform_driver(pcm5102a_codec_driver);

MODULE_DESCRIPTION("ASoC pcm5102a codec driver");
MODULE_AUTHOR("Philip Voigt <info[at]polyvection[dot]com>");
MODULE_LICENSE("GPL v2");
