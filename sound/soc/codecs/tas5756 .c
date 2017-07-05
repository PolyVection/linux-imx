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
#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/gcd.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>

#include "tas5756.h"

#define DIV_ROUND_DOWN_ULL(ll, d) \
	({ unsigned long long _tmp = (ll); do_div(_tmp, d); _tmp; })

#define TAS5756_NUM_SUPPLIES 3
static const char * const tas5756_supply_names[TAS5756_NUM_SUPPLIES] = {
	"AVDD",
	"DVDD",
	"CPVDD",
};

struct tas5756_priv {
	struct regmap *regmap;
	struct clk *sclk;
	struct regulator_bulk_data supplies[TAS5756_NUM_SUPPLIES];
	struct notifier_block supply_nb[TAS5756_NUM_SUPPLIES];
	int fmt;
	int pll_in;
	int pll_out;
	int pll_r;
	int pll_j;
	int pll_d;
	int pll_p;
	unsigned long real_pll;
	unsigned long overclock_pll;
	unsigned long overclock_dac;
	unsigned long overclock_dsp;
};

/*
 * We can't use the same notifier block for more than one supply and
 * there's no way I can see to get from a callback to the caller
 * except container_of().
 */
#define TAS5756_REGULATOR_EVENT(n) \
static int tas5756_regulator_event_##n(struct notifier_block *nb, \
				      unsigned long event, void *data)    \
{ \
	struct tas5756_priv *tas5756 = container_of(nb, struct tas5756_priv, \
						    supply_nb[n]); \
	if (event & REGULATOR_EVENT_DISABLE) { \
		regcache_mark_dirty(tas5756->regmap);	\
		regcache_cache_only(tas5756->regmap, true);	\
	} \
	return 0; \
}

TAS5756_REGULATOR_EVENT(0)
TAS5756_REGULATOR_EVENT(1)
TAS5756_REGULATOR_EVENT(2)

static const struct reg_default tas5756_reg_defaults[] = {
	{ TAS5756_RESET,             0x00 },
	{ TAS5756_POWER,             0x00 },
	{ TAS5756_MUTE,              0x00 },
	{ TAS5756_DSP,               0x00 },
	{ TAS5756_PLL_REF,           0x00 },
	{ TAS5756_DAC_REF,           0x00 },
	{ TAS5756_DAC_ROUTING,       0x11 },
	{ TAS5756_DSP_PROGRAM,       0x01 },
	{ TAS5756_CLKDET,            0x00 },
	{ TAS5756_AUTO_MUTE,         0x00 },
	{ TAS5756_ERROR_DETECT,      0x00 },
	{ TAS5756_DIGITAL_VOLUME_1,  0x00 },
	{ TAS5756_DIGITAL_VOLUME_2,  0x30 },
	{ TAS5756_DIGITAL_VOLUME_3,  0x30 },
	{ TAS5756_DIGITAL_MUTE_1,    0x22 },
	{ TAS5756_DIGITAL_MUTE_2,    0x00 },
	{ TAS5756_DIGITAL_MUTE_3,    0x07 },
	{ TAS5756_OUTPUT_AMPLITUDE,  0x00 },
	{ TAS5756_ANALOG_GAIN_CTRL,  0x00 },
	{ TAS5756_UNDERVOLTAGE_PROT, 0x00 },
	{ TAS5756_ANALOG_MUTE_CTRL,  0x00 },
	{ TAS5756_ANALOG_GAIN_BOOST, 0x00 },
	{ TAS5756_VCOM_CTRL_1,       0x00 },
	{ TAS5756_VCOM_CTRL_2,       0x01 },
	{ TAS5756_BCLK_LRCLK_CFG,    0x00 },
	{ TAS5756_MASTER_MODE,       0x7c },
	{ TAS5756_GPIO_DACIN,        0x00 },
	{ TAS5756_GPIO_PLLIN,        0x00 },
	{ TAS5756_SYNCHRONIZE,       0x10 },
	{ TAS5756_PLL_COEFF_0,       0x00 },
	{ TAS5756_PLL_COEFF_1,       0x00 },
	{ TAS5756_PLL_COEFF_2,       0x00 },
	{ TAS5756_PLL_COEFF_3,       0x00 },
	{ TAS5756_PLL_COEFF_4,       0x00 },
	{ TAS5756_DSP_CLKDIV,        0x00 },
	{ TAS5756_DAC_CLKDIV,        0x00 },
	{ TAS5756_NCP_CLKDIV,        0x00 },
	{ TAS5756_OSR_CLKDIV,        0x00 },
	{ TAS5756_MASTER_CLKDIV_1,   0x00 },
	{ TAS5756_MASTER_CLKDIV_2,   0x00 },
	{ TAS5756_FS_SPEED_MODE,     0x00 },
	{ TAS5756_IDAC_1,            0x01 },
	{ TAS5756_IDAC_2,            0x00 },
};

static bool tas5756_readable(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TAS5756_RESET:
	case TAS5756_POWER:
	case TAS5756_MUTE:
	case TAS5756_PLL_EN:
	case TAS5756_SPI_MISO_FUNCTION:
	case TAS5756_DSP:
	case TAS5756_GPIO_EN:
	case TAS5756_BCLK_LRCLK_CFG:
	case TAS5756_DSP_GPIO_INPUT:
	case TAS5756_MASTER_MODE:
	case TAS5756_PLL_REF:
	case TAS5756_DAC_REF:
	case TAS5756_GPIO_DACIN:
	case TAS5756_GPIO_PLLIN:
	case TAS5756_SYNCHRONIZE:
	case TAS5756_PLL_COEFF_0:
	case TAS5756_PLL_COEFF_1:
	case TAS5756_PLL_COEFF_2:
	case TAS5756_PLL_COEFF_3:
	case TAS5756_PLL_COEFF_4:
	case TAS5756_DSP_CLKDIV:
	case TAS5756_DAC_CLKDIV:
	case TAS5756_NCP_CLKDIV:
	case TAS5756_OSR_CLKDIV:
	case TAS5756_MASTER_CLKDIV_1:
	case TAS5756_MASTER_CLKDIV_2:
	case TAS5756_FS_SPEED_MODE:
	case TAS5756_IDAC_1:
	case TAS5756_IDAC_2:
	case TAS5756_ERROR_DETECT:
	case TAS5756_I2S_1:
	case TAS5756_I2S_2:
	case TAS5756_DAC_ROUTING:
	case TAS5756_DSP_PROGRAM:
	case TAS5756_CLKDET:
	case TAS5756_AUTO_MUTE:
	case TAS5756_DIGITAL_VOLUME_1:
	case TAS5756_DIGITAL_VOLUME_2:
	case TAS5756_DIGITAL_VOLUME_3:
	case TAS5756_DIGITAL_MUTE_1:
	case TAS5756_DIGITAL_MUTE_2:
	case TAS5756_DIGITAL_MUTE_3:
	case TAS5756_GPIO_OUTPUT_1:
	case TAS5756_GPIO_OUTPUT_2:
	case TAS5756_GPIO_OUTPUT_3:
	case TAS5756_GPIO_OUTPUT_4:
	case TAS5756_GPIO_OUTPUT_5:
	case TAS5756_GPIO_OUTPUT_6:
	case TAS5756_GPIO_CONTROL_1:
	case TAS5756_GPIO_CONTROL_2:
	case TAS5756_OVERFLOW:
	case TAS5756_RATE_DET_1:
	case TAS5756_RATE_DET_2:
	case TAS5756_RATE_DET_3:
	case TAS5756_RATE_DET_4:
	case TAS5756_CLOCK_STATUS:
	case TAS5756_ANALOG_MUTE_DET:
	case TAS5756_GPIN:
	case TAS5756_DIGITAL_MUTE_DET:
	case TAS5756_OUTPUT_AMPLITUDE:
	case TAS5756_ANALOG_GAIN_CTRL:
	case TAS5756_UNDERVOLTAGE_PROT:
	case TAS5756_ANALOG_MUTE_CTRL:
	case TAS5756_ANALOG_GAIN_BOOST:
	case TAS5756_VCOM_CTRL_1:
	case TAS5756_VCOM_CTRL_2:
	case TAS5756_CRAM_CTRL:
	case TAS5756_FLEX_A:
	case TAS5756_FLEX_B:
		return true;
	default:
		/* There are 256 raw register addresses */
		return reg < 0xff;
	}
}

static bool tas5756_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case TAS5756_PLL_EN:
	case TAS5756_OVERFLOW:
	case TAS5756_RATE_DET_1:
	case TAS5756_RATE_DET_2:
	case TAS5756_RATE_DET_3:
	case TAS5756_RATE_DET_4:
	case TAS5756_CLOCK_STATUS:
	case TAS5756_ANALOG_MUTE_DET:
	case TAS5756_GPIN:
	case TAS5756_DIGITAL_MUTE_DET:
	case TAS5756_CRAM_CTRL:
		return true;
	default:
		/* There are 256 raw register addresses */
		return reg < 0xff;
	}
}

static int tas5756_overclock_pll_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5756->overclock_pll;
	return 0;
}

static int tas5756_overclock_pll_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	switch (codec->dapm.bias_level) {
	case SND_SOC_BIAS_OFF:
	case SND_SOC_BIAS_STANDBY:
		break;
	default:
		return -EBUSY;
	}

	tas5756->overclock_pll = ucontrol->value.integer.value[0];
	return 0;
}

static int tas5756_overclock_dsp_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5756->overclock_dsp;
	return 0;
}

static int tas5756_overclock_dsp_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	switch (codec->dapm.bias_level) {
	case SND_SOC_BIAS_OFF:
	case SND_SOC_BIAS_STANDBY:
		break;
	default:
		return -EBUSY;
	}

	tas5756->overclock_dsp = ucontrol->value.integer.value[0];
	return 0;
}

static int tas5756_overclock_dac_get(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = tas5756->overclock_dac;
	return 0;
}

static int tas5756_overclock_dac_put(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	switch (codec->dapm.bias_level) {
	case SND_SOC_BIAS_OFF:
	case SND_SOC_BIAS_STANDBY:
		break;
	default:
		return -EBUSY;
	}

	tas5756->overclock_dac = ucontrol->value.integer.value[0];
	return 0;
}

static const DECLARE_TLV_DB_SCALE(digital_tlv, -10350, 50, 1);
static const DECLARE_TLV_DB_SCALE(analog_tlv, -600, 600, 0);
static const DECLARE_TLV_DB_SCALE(boost_tlv, 0, 80, 0);

static const char * const tas5756_dsp_program_texts[] = {
	"FIR interpolation with de-emphasis",
	"Low latency IIR with de-emphasis",
	"High attenuation with de-emphasis",
	"Fixed process flow",
	"Ringing-less low latency FIR",
};

static const unsigned int tas5756_dsp_program_values[] = {
	1,
	2,
	3,
	5,
	7,
};

static SOC_VALUE_ENUM_SINGLE_DECL(tas5756_dsp_program,
				  TAS5756_DSP_PROGRAM, 0, 0x1f,
				  tas5756_dsp_program_texts,
				  tas5756_dsp_program_values);

static const char * const tas5756_clk_missing_text[] = {
	"1s", "2s", "3s", "4s", "5s", "6s", "7s", "8s"
};

static const struct soc_enum tas5756_clk_missing =
	SOC_ENUM_SINGLE(TAS5756_CLKDET, 0,  8, tas5756_clk_missing_text);

static const char * const tas5756_autom_text[] = {
	"21ms", "106ms", "213ms", "533ms", "1.07s", "2.13s", "5.33s", "10.66s"
};

static const struct soc_enum tas5756_autom_l =
	SOC_ENUM_SINGLE(TAS5756_AUTO_MUTE, TAS5756_ATML_SHIFT, 8,
			tas5756_autom_text);

static const struct soc_enum tas5756_autom_r =
	SOC_ENUM_SINGLE(TAS5756_AUTO_MUTE, TAS5756_ATMR_SHIFT, 8,
			tas5756_autom_text);

static const char * const tas5756_ramp_rate_text[] = {
	"1 sample/update", "2 samples/update", "4 samples/update",
	"Immediate"
};

static const struct soc_enum tas5756_vndf =
	SOC_ENUM_SINGLE(TAS5756_DIGITAL_MUTE_1, TAS5756_VNDF_SHIFT, 4,
			tas5756_ramp_rate_text);

static const struct soc_enum tas5756_vnuf =
	SOC_ENUM_SINGLE(TAS5756_DIGITAL_MUTE_1, TAS5756_VNUF_SHIFT, 4,
			tas5756_ramp_rate_text);

static const struct soc_enum tas5756_vedf =
	SOC_ENUM_SINGLE(TAS5756_DIGITAL_MUTE_2, TAS5756_VEDF_SHIFT, 4,
			tas5756_ramp_rate_text);

static const char * const tas5756_ramp_step_text[] = {
	"4dB/step", "2dB/step", "1dB/step", "0.5dB/step"
};

static const struct soc_enum tas5756_vnds =
	SOC_ENUM_SINGLE(TAS5756_DIGITAL_MUTE_1, TAS5756_VNDS_SHIFT, 4,
			tas5756_ramp_step_text);

static const struct soc_enum tas5756_vnus =
	SOC_ENUM_SINGLE(TAS5756_DIGITAL_MUTE_1, TAS5756_VNUS_SHIFT, 4,
			tas5756_ramp_step_text);

static const struct soc_enum tas5756_veds =
	SOC_ENUM_SINGLE(TAS5756_DIGITAL_MUTE_2, TAS5756_VEDS_SHIFT, 4,
			tas5756_ramp_step_text);

static const struct snd_kcontrol_new tas5756_controls[] = {
SOC_DOUBLE_R_TLV("Digital Playback Volume", TAS5756_DIGITAL_VOLUME_2,
		 TAS5756_DIGITAL_VOLUME_3, 0, 255, 1, digital_tlv),
SOC_DOUBLE_TLV("Analogue Playback Volume", TAS5756_ANALOG_GAIN_CTRL,
	       TAS5756_LAGN_SHIFT, TAS5756_RAGN_SHIFT, 1, 1, analog_tlv),
SOC_DOUBLE_TLV("Analogue Playback Boost Volume", TAS5756_ANALOG_GAIN_BOOST,
	       TAS5756_AGBL_SHIFT, TAS5756_AGBR_SHIFT, 1, 0, boost_tlv),
SOC_DOUBLE("Digital Playback Switch", TAS5756_MUTE, TAS5756_RQML_SHIFT,
	   TAS5756_RQMR_SHIFT, 1, 1),

SOC_SINGLE("Deemphasis Switch", TAS5756_DSP, TAS5756_DEMP_SHIFT, 1, 1),
SOC_ENUM("DSP Program", tas5756_dsp_program),

SOC_ENUM("Clock Missing Period", tas5756_clk_missing),
SOC_ENUM("Auto Mute Time Left", tas5756_autom_l),
SOC_ENUM("Auto Mute Time Right", tas5756_autom_r),
SOC_SINGLE("Auto Mute Mono Switch", TAS5756_DIGITAL_MUTE_3,
	   TAS5756_ACTL_SHIFT, 1, 0),
SOC_DOUBLE("Auto Mute Switch", TAS5756_DIGITAL_MUTE_3, TAS5756_AMLE_SHIFT,
	   TAS5756_AMRE_SHIFT, 1, 0),

SOC_ENUM("Volume Ramp Down Rate", tas5756_vndf),
SOC_ENUM("Volume Ramp Down Step", tas5756_vnds),
SOC_ENUM("Volume Ramp Up Rate", tas5756_vnuf),
SOC_ENUM("Volume Ramp Up Step", tas5756_vnus),
SOC_ENUM("Volume Ramp Down Emergency Rate", tas5756_vedf),
SOC_ENUM("Volume Ramp Down Emergency Step", tas5756_veds),

SOC_SINGLE_EXT("Max Overclock PLL", SND_SOC_NOPM, 0, 20, 0,
	       tas5756_overclock_pll_get, tas5756_overclock_pll_put),
SOC_SINGLE_EXT("Max Overclock DSP", SND_SOC_NOPM, 0, 40, 0,
	       tas5756_overclock_dsp_get, tas5756_overclock_dsp_put),
SOC_SINGLE_EXT("Max Overclock DAC", SND_SOC_NOPM, 0, 40, 0,
	       tas5756_overclock_dac_get, tas5756_overclock_dac_put),
};

static const struct snd_soc_dapm_widget tas5756_dapm_widgets[] = {
SND_SOC_DAPM_DAC("DACL", NULL, SND_SOC_NOPM, 0, 0),
SND_SOC_DAPM_DAC("DACR", NULL, SND_SOC_NOPM, 0, 0),

SND_SOC_DAPM_OUTPUT("OUTL"),
SND_SOC_DAPM_OUTPUT("OUTR"),
};

static const struct snd_soc_dapm_route tas5756_dapm_routes[] = {
	{ "DACL", NULL, "Playback" },
	{ "DACR", NULL, "Playback" },

	{ "OUTL", NULL, "DACL" },
	{ "OUTR", NULL, "DACR" },
};

static unsigned long tas5756_pll_max(struct tas5756_priv *tas5756)
{
	return 25000000 + 25000000 * tas5756->overclock_pll / 100;
}

static unsigned long tas5756_dsp_max(struct tas5756_priv *tas5756)
{
	return 50000000 + 50000000 * tas5756->overclock_dsp / 100;
}

static unsigned long tas5756_dac_max(struct tas5756_priv *tas5756,
				     unsigned long rate)
{
	return rate + rate * tas5756->overclock_dac / 100;
}

static unsigned long tas5756_sck_max(struct tas5756_priv *tas5756)
{
	if (!tas5756->pll_out)
		return 25000000;
	return tas5756_pll_max(tas5756);
}

static unsigned long tas5756_ncp_target(struct tas5756_priv *tas5756,
					unsigned long dac_rate)
{
	/*
	 * If the DAC is not actually overclocked, use the good old
	 * NCP target rate...
	 */
	if (dac_rate <= 6144000)
		return 1536000;
	/*
	 * ...but if the DAC is in fact overclocked, bump the NCP target
	 * rate to get the recommended dividers even when overclocking.
	 */
	return tas5756_dac_max(tas5756, 1536000);
}

static const u32 tas5756_dai_rates[] = {
	8000, 11025, 16000, 22050, 32000, 44100, 48000, 64000,
	88200, 96000, 176400, 192000, 384000,
};

static const struct snd_pcm_hw_constraint_list constraints_slave = {
	.count = ARRAY_SIZE(tas5756_dai_rates),
	.list  = tas5756_dai_rates,
};

static int tas5756_hw_rule_rate(struct snd_pcm_hw_params *params,
				struct snd_pcm_hw_rule *rule)
{
	struct tas5756_priv *tas5756 = rule->private;
	struct snd_interval ranges[2];
	int frame_size;

	frame_size = snd_soc_params_to_frame_size(params);
	if (frame_size < 0)
		return frame_size;

	switch (frame_size) {
	case 32:
		/* No hole when the frame size is 32. */
		return 0;
	case 48:
	case 64:
		/* There is only one hole in the range of supported
		 * rates, but it moves with the frame size.
		 */
		memset(ranges, 0, sizeof(ranges));
		ranges[0].min = 8000;
		ranges[0].max = tas5756_sck_max(tas5756) / frame_size / 2;
		ranges[1].min = DIV_ROUND_UP(16000000, frame_size);
		ranges[1].max = 384000;
		break;
	default:
		return -EINVAL;
	}

	return snd_interval_ranges(hw_param_interval(params, rule->var),
				   ARRAY_SIZE(ranges), ranges, 0);
}

static int tas5756_dai_startup_master(struct snd_pcm_substream *substream,
				      struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = dai->dev;
	struct snd_pcm_hw_constraint_ratnums *constraints_no_pll;
	struct snd_ratnum *rats_no_pll;

	if (IS_ERR(tas5756->sclk)) {
		dev_err(dev, "Need SCLK for master mode: %ld\n",
			PTR_ERR(tas5756->sclk));
		return PTR_ERR(tas5756->sclk);
	}

	if (tas5756->pll_out)
		return snd_pcm_hw_rule_add(substream->runtime, 0,
					   SNDRV_PCM_HW_PARAM_RATE,
					   tas5756_hw_rule_rate,
					   tas5756,
					   SNDRV_PCM_HW_PARAM_FRAME_BITS,
					   SNDRV_PCM_HW_PARAM_CHANNELS, -1);

	constraints_no_pll = devm_kzalloc(dev, sizeof(*constraints_no_pll),
					  GFP_KERNEL);
	if (!constraints_no_pll)
		return -ENOMEM;
	constraints_no_pll->nrats = 1;
	rats_no_pll = devm_kzalloc(dev, sizeof(*rats_no_pll), GFP_KERNEL);
	if (!rats_no_pll)
		return -ENOMEM;
	constraints_no_pll->rats = rats_no_pll;
	rats_no_pll->num = clk_get_rate(tas5756->sclk) / 64;
	rats_no_pll->den_min = 1;
	rats_no_pll->den_max = 128;
	rats_no_pll->den_step = 1;

	return snd_pcm_hw_constraint_ratnums(substream->runtime, 0,
					     SNDRV_PCM_HW_PARAM_RATE,
					     constraints_no_pll);
}

static int tas5756_dai_startup_slave(struct snd_pcm_substream *substream,
				     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	struct device *dev = dai->dev;
	struct regmap *regmap = tas5756->regmap;

	if (IS_ERR(tas5756->sclk)) {
		dev_info(dev, "No SCLK, using BCLK: %ld\n",
			 PTR_ERR(tas5756->sclk));
	

		/* Disable reporting of missing SCLK as an error */
		regmap_update_bits(regmap, TAS5756_ERROR_DETECT,
				   TAS5756_IDCH, TAS5756_IDCH);

		/* Switch PLL input to BCLK */
		regmap_update_bits(regmap, TAS5756_PLL_REF,
				   TAS5756_SREF, TAS5756_SREF_BCK);
	}

	else dev_info(dev, "SCLK: %ld\n", clk_get_rate(tas5756->sclk));

	return snd_pcm_hw_constraint_list(substream->runtime, 0,
					  SNDRV_PCM_HW_PARAM_RATE,
					  &constraints_slave);
}

static int tas5756_dai_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	switch (tas5756->fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
		return tas5756_dai_startup_master(substream, dai);

	case SND_SOC_DAIFMT_CBS_CFS:
		return tas5756_dai_startup_slave(substream, dai);

	default:
		return -EINVAL;
	}
}

static int tas5756_set_bias_level(struct snd_soc_codec *codec,
				  enum snd_soc_bias_level level)
{
	struct tas5756_priv *tas5756 = dev_get_drvdata(codec->dev);
	int ret;

	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		ret = regmap_update_bits(tas5756->regmap, TAS5756_POWER,
					 TAS5756_RQST, 0);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to remove standby: %d\n",
				ret);
			return ret;
		}
		break;

	case SND_SOC_BIAS_OFF:
		ret = regmap_update_bits(tas5756->regmap, TAS5756_POWER,
					 TAS5756_RQST, TAS5756_RQST);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to request standby: %d\n",
				ret);
			return ret;
		}
		break;
	}

	codec->dapm.bias_level = level;

	return 0;
}

static unsigned long tas5756_find_sck(struct snd_soc_dai *dai,
				      unsigned long bclk_rate)
{
	struct device *dev = dai->dev;
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	unsigned long sck_rate;
	int pow2;

	/* 64 MHz <= pll_rate <= 100 MHz, VREF mode */
	/* 16 MHz <= sck_rate <=  25 MHz, VREF mode */

	/* select sck_rate as a multiple of bclk_rate but still with
	 * as many factors of 2 as possible, as that makes it easier
	 * to find a fast DAC rate
	 */
	pow2 = 1 << fls((tas5756_pll_max(tas5756) - 16000000) / bclk_rate);
	for (; pow2; pow2 >>= 1) {
		sck_rate = rounddown(tas5756_pll_max(tas5756),
				     bclk_rate * pow2);
		if (sck_rate >= 16000000)
			break;
	}
	if (!pow2) {
		dev_err(dev, "Impossible to generate a suitable SCK\n");
		return 0;
	}

	dev_dbg(dev, "sck_rate %lu\n", sck_rate);
	return sck_rate;
}

/* pll_rate = pllin_rate * R * J.D / P
 * 1 <= R <= 16
 * 1 <= J <= 63
 * 0 <= D <= 9999
 * 1 <= P <= 15
 * 64 MHz <= pll_rate <= 100 MHz
 * if D == 0
 *     1 MHz <= pllin_rate / P <= 20 MHz
 * else if D > 0
 *     6.667 MHz <= pllin_rate / P <= 20 MHz
 *     4 <= J <= 11
 *     R = 1
 */
static int tas5756_find_pll_coeff(struct snd_soc_dai *dai,
				  unsigned long pllin_rate,
				  unsigned long pll_rate)
{
	struct device *dev = dai->dev;
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	unsigned long common;
	int R, J, D, P;
	unsigned long K; /* 10000 * J.D */
	unsigned long num;
	unsigned long den;

	common = gcd(pll_rate, pllin_rate);
	dev_dbg(dev, "pll %lu pllin %lu common %lu\n",
		pll_rate, pllin_rate, common);
	num = pll_rate / common;
	den = pllin_rate / common;

	/* pllin_rate / P (or here, den) cannot be greater than 20 MHz */
	if (pllin_rate / den > 20000000 && num < 8) {
		num *= DIV_ROUND_UP(pllin_rate / den, 20000000);
		den *= DIV_ROUND_UP(pllin_rate / den, 20000000);
	}
	dev_dbg(dev, "num / den = %lu / %lu\n", num, den);

	P = den;
	if (den <= 15 && num <= 16 * 63
	    && 1000000 <= pllin_rate / P && pllin_rate / P <= 20000000) {
		/* Try the case with D = 0 */
		D = 0;
		/* factor 'num' into J and R, such that R <= 16 and J <= 63 */
		for (R = 16; R; R--) {
			if (num % R)
				continue;
			J = num / R;
			if (J == 0 || J > 63)
				continue;

			dev_dbg(dev, "R * J / P = %d * %d / %d\n", R, J, P);
			tas5756->real_pll = pll_rate;
			goto done;
		}
		/* no luck */
	}

	R = 1;

	if (num > 0xffffffffUL / 10000)
		goto fallback;

	/* Try to find an exact pll_rate using the D > 0 case */
	common = gcd(10000 * num, den);
	num = 10000 * num / common;
	den /= common;
	dev_dbg(dev, "num %lu den %lu common %lu\n", num, den, common);

	for (P = den; P <= 15; P++) {
		if (pllin_rate / P < 6667000 || 200000000 < pllin_rate / P)
			continue;
		if (num * P % den)
			continue;
		K = num * P / den;
		/* J == 12 is ok if D == 0 */
		if (K < 40000 || K > 120000)
			continue;

		J = K / 10000;
		D = K % 10000;
		dev_dbg(dev, "J.D / P = %d.%04d / %d\n", J, D, P);
		tas5756->real_pll = pll_rate;
		goto done;
	}

	/* Fall back to an approximate pll_rate */

fallback:
	/* find smallest possible P */
	P = DIV_ROUND_UP(pllin_rate, 20000000);
	if (!P)
		P = 1;
	else if (P > 15) {
		dev_err(dev, "Need a slower clock as pll-input\n");
		return -EINVAL;
	}
	if (pllin_rate / P < 6667000) {
		dev_err(dev, "Need a faster clock as pll-input\n");
		return -EINVAL;
	}
	K = DIV_ROUND_CLOSEST_ULL(10000ULL * pll_rate * P, pllin_rate);
	if (K < 40000)
		K = 40000;
	/* J == 12 is ok if D == 0 */
	if (K > 120000)
		K = 120000;
	J = K / 10000;
	D = K % 10000;
	dev_dbg(dev, "J.D / P ~ %d.%04d / %d\n", J, D, P);
	tas5756->real_pll = DIV_ROUND_DOWN_ULL((u64)K * pllin_rate, 10000 * P);

done:
	tas5756->pll_r = R;
	tas5756->pll_j = J;
	tas5756->pll_d = D;
	tas5756->pll_p = P;
	return 0;
}

static unsigned long tas5756_pllin_dac_rate(struct snd_soc_dai *dai,
					    unsigned long osr_rate,
					    unsigned long pllin_rate)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	unsigned long dac_rate;

	if (!tas5756->pll_out)
		return 0; /* no PLL to bypass, force SCK as DAC input */

	if (pllin_rate % osr_rate)
		return 0; /* futile, quit early */

	/* run DAC no faster than 6144000 Hz */
	for (dac_rate = rounddown(tas5756_dac_max(tas5756, 6144000), osr_rate);
	     dac_rate;
	     dac_rate -= osr_rate) {

		if (pllin_rate / dac_rate > 128)
			return 0; /* DAC divider would be too big */

		if (!(pllin_rate % dac_rate))
			return dac_rate;

		dac_rate -= osr_rate;
	}

	return 0;
}

static int tas5756_set_dividers(struct snd_soc_dai *dai,
				struct snd_pcm_hw_params *params)
{
	struct device *dev = dai->dev;
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	unsigned long pllin_rate = 0;
	unsigned long pll_rate;
	unsigned long sck_rate;
	unsigned long mck_rate;
	unsigned long bclk_rate;
	unsigned long sample_rate;
	unsigned long osr_rate;
	unsigned long dacsrc_rate;
	int bclk_div;
	int lrclk_div;
	int dsp_div;
	int dac_div;
	unsigned long dac_rate;
	int ncp_div;
	int osr_div;
	int ret;
	int idac;
	int fssp;
	int gpio;

	lrclk_div = snd_soc_params_to_frame_size(params);
	if (lrclk_div == 0) {
		dev_err(dev, "No LRCLK?\n");
		return -EINVAL;
	}

	if (!tas5756->pll_out) {
		sck_rate = clk_get_rate(tas5756->sclk);
		bclk_div = params->rate_den * 64 / lrclk_div;
		bclk_rate = DIV_ROUND_CLOSEST(sck_rate, bclk_div);

		mck_rate = sck_rate;
	} else {
		ret = snd_soc_params_to_bclk(params);
		if (ret < 0) {
			dev_err(dev, "Failed to find suitable BCLK: %d\n", ret);
			return ret;
		}
		if (ret == 0) {
			dev_err(dev, "No BCLK?\n");
			return -EINVAL;
		}
		bclk_rate = ret;

		pllin_rate = clk_get_rate(tas5756->sclk);

		sck_rate = tas5756_find_sck(dai, bclk_rate);
		if (!sck_rate)
			return -EINVAL;
		pll_rate = 4 * sck_rate;

		ret = tas5756_find_pll_coeff(dai, pllin_rate, pll_rate);
		if (ret != 0)
			return ret;

		ret = regmap_write(tas5756->regmap,
				   TAS5756_PLL_COEFF_0, tas5756->pll_p - 1);
		if (ret != 0) {
			dev_err(dev, "Failed to write PLL P: %d\n", ret);
			return ret;
		}

		ret = regmap_write(tas5756->regmap,
				   TAS5756_PLL_COEFF_1, tas5756->pll_j);
		if (ret != 0) {
			dev_err(dev, "Failed to write PLL J: %d\n", ret);
			return ret;
		}

		ret = regmap_write(tas5756->regmap,
				   TAS5756_PLL_COEFF_2, tas5756->pll_d >> 8);
		if (ret != 0) {
			dev_err(dev, "Failed to write PLL D msb: %d\n", ret);
			return ret;
		}

		ret = regmap_write(tas5756->regmap,
				   TAS5756_PLL_COEFF_3, tas5756->pll_d & 0xff);
		if (ret != 0) {
			dev_err(dev, "Failed to write PLL D lsb: %d\n", ret);
			return ret;
		}

		ret = regmap_write(tas5756->regmap,
				   TAS5756_PLL_COEFF_4, tas5756->pll_r - 1);
		if (ret != 0) {
			dev_err(dev, "Failed to write PLL R: %d\n", ret);
			return ret;
		}

		mck_rate = tas5756->real_pll;

		bclk_div = DIV_ROUND_CLOSEST(sck_rate, bclk_rate);
	}

	if (bclk_div > 128) {
		dev_err(dev, "Failed to find BCLK divider\n");
		return -EINVAL;
	}

	/* the actual rate */
	sample_rate = sck_rate / bclk_div / lrclk_div;
	osr_rate = 16 * sample_rate;

	/* run DSP no faster than 50 MHz */
	dsp_div = mck_rate > tas5756_dsp_max(tas5756) ? 2 : 1;

	dac_rate = tas5756_pllin_dac_rate(dai, osr_rate, pllin_rate);
	if (dac_rate) {
		/* the desired clock rate is "compatible" with the pll input
		 * clock, so use that clock as dac input instead of the pll
		 * output clock since the pll will introduce jitter and thus
		 * noise.
		 */
		dev_dbg(dev, "using pll input as dac input\n");
		ret = regmap_update_bits(tas5756->regmap, TAS5756_DAC_REF,
					 TAS5756_SDAC, TAS5756_SDAC_GPIO);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to set gpio as dacref: %d\n", ret);
			return ret;
		}

		gpio = TAS5756_GREF_GPIO1 + tas5756->pll_in - 1;
		ret = regmap_update_bits(tas5756->regmap, TAS5756_GPIO_DACIN,
					 TAS5756_GREF, gpio);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to set gpio %d as dacin: %d\n",
				tas5756->pll_in, ret);
			return ret;
		}

		dacsrc_rate = pllin_rate;
	} else {
		/* run DAC no faster than 6144000 Hz */
		unsigned long dac_mul = tas5756_dac_max(tas5756, 6144000)
			/ osr_rate;
		unsigned long sck_mul = sck_rate / osr_rate;

		for (; dac_mul; dac_mul--) {
			if (!(sck_mul % dac_mul))
				break;
		}
		if (!dac_mul) {
			dev_err(dev, "Failed to find DAC rate\n");
			return -EINVAL;
		}

		dac_rate = dac_mul * osr_rate;
		dev_dbg(dev, "dac_rate %lu sample_rate %lu\n",
			dac_rate, sample_rate);

		ret = regmap_update_bits(tas5756->regmap, TAS5756_DAC_REF,
					 TAS5756_SDAC, TAS5756_SDAC_SCK);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to set sck as dacref: %d\n", ret);
			return ret;
		}

		dacsrc_rate = sck_rate;
	}

	osr_div = DIV_ROUND_CLOSEST(dac_rate, osr_rate);
	if (osr_div > 128) {
		dev_err(dev, "Failed to find OSR divider\n");
		return -EINVAL;
	}

	dac_div = DIV_ROUND_CLOSEST(dacsrc_rate, dac_rate);
	if (dac_div > 128) {
		dev_err(dev, "Failed to find DAC divider\n");
		return -EINVAL;
	}
	dac_rate = dacsrc_rate / dac_div;

	ncp_div = DIV_ROUND_CLOSEST(dac_rate,
				    tas5756_ncp_target(tas5756, dac_rate));
	if (ncp_div > 128 || dac_rate / ncp_div > 2048000) {
		/* run NCP no faster than 2048000 Hz, but why? */
		ncp_div = DIV_ROUND_UP(dac_rate, 2048000);
		if (ncp_div > 128) {
			dev_err(dev, "Failed to find NCP divider\n");
			return -EINVAL;
		}
	}

	idac = mck_rate / (dsp_div * sample_rate);

	ret = regmap_write(tas5756->regmap, TAS5756_DSP_CLKDIV, dsp_div - 1);
	if (ret != 0) {
		dev_err(dev, "Failed to write DSP divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap, TAS5756_DAC_CLKDIV, dac_div - 1);
	if (ret != 0) {
		dev_err(dev, "Failed to write DAC divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap, TAS5756_NCP_CLKDIV, ncp_div - 1);
	if (ret != 0) {
		dev_err(dev, "Failed to write NCP divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap, TAS5756_OSR_CLKDIV, osr_div - 1);
	if (ret != 0) {
		dev_err(dev, "Failed to write OSR divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap,
			   TAS5756_MASTER_CLKDIV_1, bclk_div - 1);
	if (ret != 0) {
		dev_err(dev, "Failed to write BCLK divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap,
			   TAS5756_MASTER_CLKDIV_2, lrclk_div - 1);
	if (ret != 0) {
		dev_err(dev, "Failed to write LRCLK divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap, TAS5756_IDAC_1, idac >> 8);
	if (ret != 0) {
		dev_err(dev, "Failed to write IDAC msb divider: %d\n", ret);
		return ret;
	}

	ret = regmap_write(tas5756->regmap, TAS5756_IDAC_2, idac & 0xff);
	if (ret != 0) {
		dev_err(dev, "Failed to write IDAC lsb divider: %d\n", ret);
		return ret;
	}

	if (sample_rate <= tas5756_dac_max(tas5756, 48000))
		fssp = TAS5756_FSSP_48KHZ;
	else if (sample_rate <= tas5756_dac_max(tas5756, 96000))
		fssp = TAS5756_FSSP_96KHZ;
	else if (sample_rate <= tas5756_dac_max(tas5756, 192000))
		fssp = TAS5756_FSSP_192KHZ;
	else
		fssp = TAS5756_FSSP_384KHZ;
	ret = regmap_update_bits(tas5756->regmap, TAS5756_FS_SPEED_MODE,
				 TAS5756_FSSP, fssp);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set fs speed: %d\n", ret);
		return ret;
	}

	dev_dbg(codec->dev, "DSP divider %d\n", dsp_div);
	dev_dbg(codec->dev, "DAC divider %d\n", dac_div);
	dev_dbg(codec->dev, "NCP divider %d\n", ncp_div);
	dev_dbg(codec->dev, "OSR divider %d\n", osr_div);
	dev_dbg(codec->dev, "BCK divider %d\n", bclk_div);
	dev_dbg(codec->dev, "LRCK divider %d\n", lrclk_div);
	dev_dbg(codec->dev, "IDAC %d\n", idac);
	dev_dbg(codec->dev, "1<<FSSP %d\n", 1 << fssp);

	return 0;
}

static int tas5756_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);
	int alen;
	int gpio;
	int clock_output;
	int master_mode;
	int ret;

	dev_dbg(codec->dev, "hw_params %u Hz, %u channels\n",
		params_rate(params),
		params_channels(params));

	switch (snd_pcm_format_width(params_format(params))) {
	case 16:
		alen = TAS5756_ALEN_16;
		break;
	case 20:
		alen = TAS5756_ALEN_20;
		break;
	case 24:
		alen = TAS5756_ALEN_24;
		break;
	case 32:
		alen = TAS5756_ALEN_32;
		break;
	default:
		dev_err(codec->dev, "Bad frame size: %d\n",
			snd_pcm_format_width(params_format(params)));
		return -EINVAL;
	}

	switch (tas5756->fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		ret = regmap_update_bits(tas5756->regmap,
					 TAS5756_BCLK_LRCLK_CFG,
					 TAS5756_BCKP
					 | TAS5756_BCKO | TAS5756_LRKO,
					 0);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to enable slave mode: %d\n", ret);
			return ret;
		}

		ret = regmap_update_bits(tas5756->regmap, TAS5756_ERROR_DETECT,
					 TAS5756_DCAS, 0);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to enable clock divider autoset: %d\n",
				ret);
			return ret;
		}
		return 0;
	case SND_SOC_DAIFMT_CBM_CFM:
		clock_output = TAS5756_BCKO | TAS5756_LRKO;
		master_mode = TAS5756_RLRK | TAS5756_RBCK;
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		clock_output = TAS5756_BCKO;
		master_mode = TAS5756_RBCK;
		break;
	default:
		return -EINVAL;
	}

	ret = regmap_update_bits(tas5756->regmap, TAS5756_I2S_1,
				 TAS5756_ALEN, alen);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to set frame size: %d\n", ret);
		return ret;
	}

	if (tas5756->pll_out) {
		ret = regmap_write(tas5756->regmap, TAS5756_FLEX_A, 0x11);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to set FLEX_A: %d\n", ret);
			return ret;
		}

		ret = regmap_write(tas5756->regmap, TAS5756_FLEX_B, 0xff);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to set FLEX_B: %d\n", ret);
			return ret;
		}

		ret = regmap_update_bits(tas5756->regmap, TAS5756_ERROR_DETECT,
					 TAS5756_IDFS | TAS5756_IDBK
					 | TAS5756_IDSK | TAS5756_IDCH
					 | TAS5756_IDCM | TAS5756_DCAS
					 | TAS5756_IPLK,
					 TAS5756_IDFS | TAS5756_IDBK
					 | TAS5756_IDSK | TAS5756_IDCH
					 | TAS5756_DCAS);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to ignore auto-clock failures: %d\n",
				ret);
			return ret;
		}
	} else {
		ret = regmap_update_bits(tas5756->regmap, TAS5756_ERROR_DETECT,
					 TAS5756_IDFS | TAS5756_IDBK
					 | TAS5756_IDSK | TAS5756_IDCH
					 | TAS5756_IDCM | TAS5756_DCAS
					 | TAS5756_IPLK,
					 TAS5756_IDFS | TAS5756_IDBK
					 | TAS5756_IDSK | TAS5756_IDCH
					 | TAS5756_DCAS | TAS5756_IPLK);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to ignore auto-clock failures: %d\n",
				ret);
			return ret;
		}

		ret = regmap_update_bits(tas5756->regmap, TAS5756_PLL_EN,
					 TAS5756_PLLE, 0);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to disable pll: %d\n", ret);
			return ret;
		}
	}

	ret = tas5756_set_dividers(dai, params);
	if (ret != 0)
		return ret;

	if (tas5756->pll_out) {
		ret = regmap_update_bits(tas5756->regmap, TAS5756_PLL_REF,
					 TAS5756_SREF, TAS5756_SREF_GPIO);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to set gpio as pllref: %d\n", ret);
			return ret;
		}

		gpio = TAS5756_GREF_GPIO1 + tas5756->pll_in - 1;
		ret = regmap_update_bits(tas5756->regmap, TAS5756_GPIO_PLLIN,
					 TAS5756_GREF, gpio);
		if (ret != 0) {
			dev_err(codec->dev,
				"Failed to set gpio %d as pllin: %d\n",
				tas5756->pll_in, ret);
			return ret;
		}

		ret = regmap_update_bits(tas5756->regmap, TAS5756_PLL_EN,
					 TAS5756_PLLE, TAS5756_PLLE);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to enable pll: %d\n", ret);
			return ret;
		}
	}

	ret = regmap_update_bits(tas5756->regmap, TAS5756_BCLK_LRCLK_CFG,
				 TAS5756_BCKP | TAS5756_BCKO | TAS5756_LRKO,
				 clock_output);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to enable clock output: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tas5756->regmap, TAS5756_MASTER_MODE,
				 TAS5756_RLRK | TAS5756_RBCK,
				 master_mode);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to enable master mode: %d\n", ret);
		return ret;
	}

	if (tas5756->pll_out) {
		gpio = TAS5756_G1OE << (tas5756->pll_out - 1);
		ret = regmap_update_bits(tas5756->regmap, TAS5756_GPIO_EN,
					 gpio, gpio);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to enable gpio %d: %d\n",
				tas5756->pll_out, ret);
			return ret;
		}

		gpio = TAS5756_GPIO_OUTPUT_1 + tas5756->pll_out - 1;
		ret = regmap_update_bits(tas5756->regmap, gpio,
					 TAS5756_GxSL, TAS5756_GxSL_PLLCK);
		if (ret != 0) {
			dev_err(codec->dev, "Failed to output pll on %d: %d\n",
				ret, tas5756->pll_out);
			return ret;
		}
	}

	ret = regmap_update_bits(tas5756->regmap, TAS5756_SYNCHRONIZE,
				 TAS5756_RQSY, TAS5756_RQSY_HALT);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to halt clocks: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tas5756->regmap, TAS5756_SYNCHRONIZE,
				 TAS5756_RQSY, TAS5756_RQSY_RESUME);
	if (ret != 0) {
		dev_err(codec->dev, "Failed to resume clocks: %d\n", ret);
		return ret;
	}

	return 0;
}

static int tas5756_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = dai->codec;
	struct tas5756_priv *tas5756 = snd_soc_codec_get_drvdata(codec);

	tas5756->fmt = fmt;

	return 0;
}

static const struct snd_soc_dai_ops tas5756_dai_ops = {
	.startup = tas5756_dai_startup,
	.hw_params = tas5756_hw_params,
	.set_fmt = tas5756_set_fmt,
};

static struct snd_soc_dai_driver tas5756_dai = {
	.name = "tas5756-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_CONTINUOUS,
		.rate_min = 8000,
		.rate_max = 384000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE |
			   SNDRV_PCM_FMTBIT_S24_LE |
			   SNDRV_PCM_FMTBIT_S32_LE
	},
	.ops = &tas5756_dai_ops,
};

static struct snd_soc_codec_driver tas5756_codec_driver = {
	.set_bias_level = tas5756_set_bias_level,
	.idle_bias_off = true,

	.controls = tas5756_controls,
	.num_controls = ARRAY_SIZE(tas5756_controls),
	.dapm_widgets = tas5756_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(tas5756_dapm_widgets),
	.dapm_routes = tas5756_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(tas5756_dapm_routes),
};

static const struct regmap_range_cfg tas5756_range = {
	.name = "Pages", .range_min = TAS5756_VIRT_BASE,
	.range_max = TAS5756_MAX_REGISTER,
	.selector_reg = TAS5756_PAGE,
	.selector_mask = 0xff,
	.window_start = 0, .window_len = 0x100,
};

const struct regmap_config tas5756_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.readable_reg = tas5756_readable,
	.volatile_reg = tas5756_volatile,

	.ranges = &tas5756_range,
	.num_ranges = 1,

	.max_register = TAS5756_MAX_REGISTER,
	.reg_defaults = tas5756_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(tas5756_reg_defaults),
	.cache_type = REGCACHE_RBTREE,
};
EXPORT_SYMBOL_GPL(tas5756_regmap);

int tas5756_probe(struct device *dev, struct regmap *regmap)
{
	struct tas5756_priv *tas5756;
	int i, ret;

	tas5756 = devm_kzalloc(dev, sizeof(struct tas5756_priv), GFP_KERNEL);
	if (!tas5756)
		return -ENOMEM;

	dev_set_drvdata(dev, tas5756);
	tas5756->regmap = regmap;

	for (i = 0; i < ARRAY_SIZE(tas5756->supplies); i++)
		tas5756->supplies[i].supply = tas5756_supply_names[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(tas5756->supplies),
				      tas5756->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to get supplies: %d\n", ret);
		return ret;
	}

	tas5756->supply_nb[0].notifier_call = tas5756_regulator_event_0;
	tas5756->supply_nb[1].notifier_call = tas5756_regulator_event_1;
	tas5756->supply_nb[2].notifier_call = tas5756_regulator_event_2;

	for (i = 0; i < ARRAY_SIZE(tas5756->supplies); i++) {
		ret = regulator_register_notifier(tas5756->supplies[i].consumer,
						  &tas5756->supply_nb[i]);
		if (ret != 0) {
			dev_err(dev,
				"Failed to register regulator notifier: %d\n",
				ret);
		}
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(tas5756->supplies),
				    tas5756->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}

	/* Reset the device, verifying I/O in the process for I2C */
	ret = regmap_write(regmap, TAS5756_RESET,
			   TAS5756_RSTM | TAS5756_RSTR);
	if (ret != 0) {
		dev_err(dev, "Failed to reset device: %d\n", ret);
		goto err;
	}

	ret = regmap_write(regmap, TAS5756_RESET, 0);
	if (ret != 0) {
		dev_err(dev, "Failed to reset device: %d\n", ret);
		goto err;
	}

	tas5756->sclk = devm_clk_get(dev, NULL);
	if (PTR_ERR(tas5756->sclk) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if (!IS_ERR(tas5756->sclk)) {
		ret = clk_prepare_enable(tas5756->sclk);
		if (ret != 0) {
			dev_err(dev, "Failed to enable SCLK: %d\n", ret);
			return ret;
		}
	}

	/* Default to standby mode */
	ret = regmap_update_bits(tas5756->regmap, TAS5756_POWER,
				 TAS5756_RQST, TAS5756_RQST);
	if (ret != 0) {
		dev_err(dev, "Failed to request standby: %d\n",
			ret);
		goto err_clk;
	}

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);
	pm_runtime_idle(dev);

#ifdef CONFIG_OF
	if (dev->of_node) {
		const struct device_node *np = dev->of_node;
		u32 val;

		if (of_property_read_u32(np, "pll-in", &val) >= 0) {
			if (val > 6) {
				dev_err(dev, "Invalid pll-in\n");
				ret = -EINVAL;
				goto err_clk;
			}
			tas5756->pll_in = val;
		}

		if (of_property_read_u32(np, "pll-out", &val) >= 0) {
			if (val > 6) {
				dev_err(dev, "Invalid pll-out\n");
				ret = -EINVAL;
				goto err_clk;
			}
			tas5756->pll_out = val;
		}

		if (!tas5756->pll_in != !tas5756->pll_out) {
			dev_err(dev,
				"Error: both pll-in and pll-out, or none\n");
			ret = -EINVAL;
			goto err_clk;
		}
		if (tas5756->pll_in && tas5756->pll_in == tas5756->pll_out) {
			dev_err(dev, "Error: pll-in == pll-out\n");
			ret = -EINVAL;
			goto err_clk;
		}
	}
#endif

	ret = snd_soc_register_codec(dev, &tas5756_codec_driver,
				    &tas5756_dai, 1);
	if (ret != 0) {
		dev_err(dev, "Failed to register CODEC: %d\n", ret);
		goto err_pm;
	}

	return 0;

err_pm:
	pm_runtime_disable(dev);
err_clk:
	if (!IS_ERR(tas5756->sclk))
		clk_disable_unprepare(tas5756->sclk);
err:
	regulator_bulk_disable(ARRAY_SIZE(tas5756->supplies),
				     tas5756->supplies);
	return ret;
}
EXPORT_SYMBOL_GPL(tas5756_probe);

void tas5756_remove(struct device *dev)
{
	struct tas5756_priv *tas5756 = dev_get_drvdata(dev);

	snd_soc_unregister_codec(dev);
	pm_runtime_disable(dev);
	if (!IS_ERR(tas5756->sclk))
		clk_disable_unprepare(tas5756->sclk);
	regulator_bulk_disable(ARRAY_SIZE(tas5756->supplies),
			       tas5756->supplies);
}
EXPORT_SYMBOL_GPL(tas5756_remove);

#ifdef CONFIG_PM
static int tas5756_suspend(struct device *dev)
{
	struct tas5756_priv *tas5756 = dev_get_drvdata(dev);
	int ret;

	ret = regmap_update_bits(tas5756->regmap, TAS5756_POWER,
				 TAS5756_RQPD, TAS5756_RQPD);
	if (ret != 0) {
		dev_err(dev, "Failed to request power down: %d\n", ret);
		return ret;
	}

	ret = regulator_bulk_disable(ARRAY_SIZE(tas5756->supplies),
				     tas5756->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to disable supplies: %d\n", ret);
		return ret;
	}

	if (!IS_ERR(tas5756->sclk))
		clk_disable_unprepare(tas5756->sclk);

	return 0;
}

static int tas5756_resume(struct device *dev)
{
	struct tas5756_priv *tas5756 = dev_get_drvdata(dev);
	int ret;

	if (!IS_ERR(tas5756->sclk)) {
		ret = clk_prepare_enable(tas5756->sclk);
		if (ret != 0) {
			dev_err(dev, "Failed to enable SCLK: %d\n", ret);
			return ret;
		}
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(tas5756->supplies),
				    tas5756->supplies);
	if (ret != 0) {
		dev_err(dev, "Failed to enable supplies: %d\n", ret);
		return ret;
	}

	regcache_cache_only(tas5756->regmap, false);
	ret = regcache_sync(tas5756->regmap);
	if (ret != 0) {
		dev_err(dev, "Failed to sync cache: %d\n", ret);
		return ret;
	}

	ret = regmap_update_bits(tas5756->regmap, TAS5756_POWER,
				 TAS5756_RQPD, 0);
	if (ret != 0) {
		dev_err(dev, "Failed to remove power down: %d\n", ret);
		return ret;
	}

	return 0;
}
#endif

const struct dev_pm_ops tas5756_pm_ops = {
	SET_RUNTIME_PM_OPS(tas5756_suspend, tas5756_resume, NULL)
};
EXPORT_SYMBOL_GPL(tas5756_pm_ops);

MODULE_DESCRIPTION("ASoC TAS5756 codec driver");
MODULE_AUTHOR("Mark Brown <broonie@linaro.org>");
MODULE_LICENSE("GPL v2");頵倀餵倀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵伀餵㨀䘏䜀刞退閉꼀놭蜀讀䜀刞㰀䜑愀欣挀氢挀氢挀氢挀氢挀氢挀氢挀氢挀氢挀氢挀氡开栞崀昝崀昝崀昝崀朝崀昝崀昝崀朝崀朝崀昝崀昝崀朝崀昝崀昝崀昝崀昝崀昝崀昝崀昝崀昝崀昝崀昝崀昝崀昝崀朝崀朝崀朝崀朝崀朝崀朝崀朝崀朝崀朝崀朝崀昝崀朝崀昝崀昝崀昝崀昝崀朝崀朝崀朝崀朝崀朝尀栝尀栞尀栞尀椞尀椞尀椞嬀椞嬀樞嬀樞嬀樞嬀樟娀欟娀欟娀欟娀氟夀氟夀氠夀洠夀洠夀洠堀渠堀渠
