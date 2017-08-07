/*
 * Copyright (C) 2016 Philip Voigt <info[at]polyvection[dot]com>
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>
#include <sound/pcm_params.h>
#include <sound/soc-dapm.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mfd/syscon.h>
#include "fsl_sai.h"


struct imx_pcm512x_data {
	struct snd_soc_card card;
	struct clk *codec_clk;
	unsigned int clk_frequency;
	bool is_codec_master;
	bool is_stream_in_use[2];
	bool is_stream_opened[2];
	
};

struct imx_priv {
	struct platform_device *pdev;
	struct platform_device *asrc_pdev;
	struct snd_card *snd_card;
};

static struct imx_priv card_priv;
static bool digital_gain_0db_limit = true;

static int imx_pcm512x_init(struct snd_soc_pcm_runtime *rtd)
{
	if (digital_gain_0db_limit)
	{
		int ret;
		struct snd_soc_card *card = rtd->card;
		struct snd_soc_codec *codec = rtd->codec;

		ret = snd_soc_limit_volume(codec, "Digital Playback Volume", 207);
		if (ret < 0)
			dev_warn(card->dev, "Failed to set volume limit: %d\n", ret);
	}

	return 0;
}

static int imx_pcm512x_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params) {
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
			| SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret) {
		dev_err(cpu_dai->dev, "failed to set dai fmt\n");
		return ret;
	}

	return ret;
}

static struct snd_soc_ops imx_pcm512x_ops = {
	.hw_params = imx_pcm512x_hw_params,
};

static struct snd_soc_dai_link imx_dai[] = {
{
	.name = "imx-pcm512x",
	.stream_name = "imx-pcm512x",
	.codec_dai_name = "pcm512x-hifi",
	.dai_fmt	= SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
			  SND_SOC_DAIFMT_CBS_CFS,
	.ops 		= &imx_pcm512x_ops,
	.init		= imx_pcm512x_init,
},
};

static struct snd_soc_card snd_soc_card_pcm512x = {
	.name         = "imx-audio-pcm512x",
	.owner        = THIS_MODULE,
	.dai_link     = imx_dai,
	.num_links    = ARRAY_SIZE(imx_dai),
};

static int imx_pcm512x_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &snd_soc_card_pcm512x;	

	struct device_node *cpu_np, *codec_np = NULL;
	struct platform_device *cpu_pdev;
	struct imx_priv *priv = &card_priv;
	struct imx_pcm512x_data *data;
	int ret;

	priv->pdev = pdev;

	cpu_np = of_parse_phandle(pdev->dev.of_node, "cpu-dai", 0);
	if (!cpu_np) {
		dev_err(&pdev->dev, "cpu dai phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	codec_np = of_parse_phandle(pdev->dev.of_node, "audio-codec", 0);
	if (!codec_np) {
		dev_err(&pdev->dev, "phandle missing or invalid\n");
		ret = -EINVAL;
		goto fail;
	}

	cpu_pdev = of_find_device_by_node(cpu_np);
	if (!cpu_pdev) {
		dev_err(&pdev->dev, "failed to find SAI platform device\n");
		ret = -EINVAL;
		goto fail;
	}

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto fail;
	}

	if (of_property_read_bool(pdev->dev.of_node, "codec-master"))
		data->is_codec_master = true;


	card->dev = &pdev->dev;
	card->dai_link->cpu_dai_name = dev_name(&cpu_pdev->dev);
	card->dai_link->platform_of_node = cpu_np;
	card->dai_link->codec_of_node = codec_np;
			    
	platform_set_drvdata(pdev, card);

	ret = snd_soc_register_card(card);
	if (ret)
		dev_err(&pdev->dev, "Failed to register card: %d\n", ret);


fail:
	if (cpu_np)
		of_node_put(cpu_np);
	if (codec_np)
		of_node_put(codec_np);

	return ret;
}

static int imx_pcm512x_remove(struct platform_device *pdev)
{

	return snd_soc_unregister_card(&snd_soc_card_pcm512x);

}

static const struct of_device_id imx_pcm512x_dt_ids[] = {
	{ .compatible = "polyvection,imx-audio-pcm512x", },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, imx_pcm512x_dt_ids);


static struct platform_driver imx_pcm512x_driver = {
	.driver = {
		.name = "imx-pcm512x",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = imx_pcm512x_dt_ids,
	},
	.probe = imx_pcm512x_probe,
	.remove = imx_pcm512x_remove,
};

module_platform_driver(imx_pcm512x_driver);

/* Module information */
MODULE_DESCRIPTION("ALSA SoC i.MX SAI for pcm512x");
MODULE_AUTHOR("Philip Voigt <info[at]polyvection[dot]com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:imx-pcm512x");

