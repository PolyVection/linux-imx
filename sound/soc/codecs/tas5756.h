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

#ifndef _SND_SOC_TAS5756
#define _SND_SOC_TAS5756

#include <linux/pm.h>
#include <linux/regmap.h>

#define TAS5756_VIRT_BASE 0x100
#define TAS5756_PAGE_LEN  0x100
#define TAS5756_PAGE_BASE(n)  (TAS5756_VIRT_BASE + (TAS5756_PAGE_LEN * n))

#define TAS5756_PAGE              0

#define TAS5756_RESET             (TAS5756_PAGE_BASE(0) +   1)
#define TAS5756_POWER             (TAS5756_PAGE_BASE(0) +   2)
#define TAS5756_MUTE              (TAS5756_PAGE_BASE(0) +   3)
#define TAS5756_PLL_EN            (TAS5756_PAGE_BASE(0) +   4)
#define TAS5756_SPI_MISO_FUNCTION (TAS5756_PAGE_BASE(0) +   6)
#define TAS5756_DSP               (TAS5756_PAGE_BASE(0) +   7)
#define TAS5756_GPIO_EN           (TAS5756_PAGE_BASE(0) +   8)
#define TAS5756_BCLK_LRCLK_CFG    (TAS5756_PAGE_BASE(0) +   9)
#define TAS5756_DSP_GPIO_INPUT    (TAS5756_PAGE_BASE(0) +  10)
#define TAS5756_MASTER_MODE       (TAS5756_PAGE_BASE(0) +  12)
#define TAS5756_PLL_REF           (TAS5756_PAGE_BASE(0) +  13)
#define TAS5756_DAC_REF           (TAS5756_PAGE_BASE(0) +  14)
#define TAS5756_GPIO_DACIN        (TAS5756_PAGE_BASE(0) +  16)
#define TAS5756_GPIO_PLLIN        (TAS5756_PAGE_BASE(0) +  18)
#define TAS5756_SYNCHRONIZE       (TAS5756_PAGE_BASE(0) +  19)
#define TAS5756_PLL_COEFF_0       (TAS5756_PAGE_BASE(0) +  20)
#define TAS5756_PLL_COEFF_1       (TAS5756_PAGE_BASE(0) +  21)
#define TAS5756_PLL_COEFF_2       (TAS5756_PAGE_BASE(0) +  22)
#define TAS5756_PLL_COEFF_3       (TAS5756_PAGE_BASE(0) +  23)
#define TAS5756_PLL_COEFF_4       (TAS5756_PAGE_BASE(0) +  24)
#define TAS5756_DSP_CLKDIV        (TAS5756_PAGE_BASE(0) +  27)
#define TAS5756_DAC_CLKDIV        (TAS5756_PAGE_BASE(0) +  28)
#define TAS5756_NCP_CLKDIV        (TAS5756_PAGE_BASE(0) +  29)
#define TAS5756_OSR_CLKDIV        (TAS5756_PAGE_BASE(0) +  30)
#define TAS5756_MASTER_CLKDIV_1   (TAS5756_PAGE_BASE(0) +  32)
#define TAS5756_MASTER_CLKDIV_2   (TAS5756_PAGE_BASE(0) +  33)
#define TAS5756_FS_SPEED_MODE     (TAS5756_PAGE_BASE(0) +  34)
#define TAS5756_IDAC_1            (TAS5756_PAGE_BASE(0) +  35)
#define TAS5756_IDAC_2            (TAS5756_PAGE_BASE(0) +  36)
#define TAS5756_ERROR_DETECT      (TAS5756_PAGE_BASE(0) +  37)
#define TAS5756_I2S_1             (TAS5756_PAGE_BASE(0) +  40)
#define TAS5756_I2S_2             (TAS5756_PAGE_BASE(0) +  41)
#define TAS5756_DAC_ROUTING       (TAS5756_PAGE_BASE(0) +  42)
#define TAS5756_DSP_PROGRAM       (TAS5756_PAGE_BASE(0) +  43)
#define TAS5756_CLKDET            (TAS5756_PAGE_BASE(0) +  44)
#define TAS5756_AUTO_MUTE         (TAS5756_PAGE_BASE(0) +  59)
#define TAS5756_DIGITAL_VOLUME_1  (TAS5756_PAGE_BASE(0) +  60)
#define TAS5756_DIGITAL_VOLUME_2  (TAS5756_PAGE_BASE(0) +  61)
#define TAS5756_DIGITAL_VOLUME_3  (TAS5756_PAGE_BASE(0) +  62)
#define TAS5756_DIGITAL_MUTE_1    (TAS5756_PAGE_BASE(0) +  63)
#define TAS5756_DIGITAL_MUTE_2    (TAS5756_PAGE_BASE(0) +  64)
#define TAS5756_DIGITAL_MUTE_3    (TAS5756_PAGE_BASE(0) +  65)
#define TAS5756_GPIO_OUTPUT_1     (TAS5756_PAGE_BASE(0) +  80)
#define TAS5756_GPIO_OUTPUT_2     (TAS5756_PAGE_BASE(0) +  81)
#define TAS5756_GPIO_OUTPUT_3     (TAS5756_PAGE_BASE(0) +  82)
#define TAS5756_GPIO_OUTPUT_4     (TAS5756_PAGE_BASE(0) +  83)
#define TAS5756_GPIO_OUTPUT_5     (TAS5756_PAGE_BASE(0) +  84)
#define TAS5756_GPIO_OUTPUT_6     (TAS5756_PAGE_BASE(0) +  85)
#define TAS5756_GPIO_CONTROL_1    (TAS5756_PAGE_BASE(0) +  86)
#define TAS5756_GPIO_CONTROL_2    (TAS5756_PAGE_BASE(0) +  87)
#define TAS5756_OVERFLOW          (TAS5756_PAGE_BASE(0) +  90)
#define TAS5756_RATE_DET_1        (TAS5756_PAGE_BASE(0) +  91)
#define TAS5756_RATE_DET_2        (TAS5756_PAGE_BASE(0) +  92)
#define TAS5756_RATE_DET_3        (TAS5756_PAGE_BASE(0) +  93)
#define TAS5756_RATE_DET_4        (TAS5756_PAGE_BASE(0) +  94)
#define TAS5756_CLOCK_STATUS      (TAS5756_PAGE_BASE(0) +  95)
#define TAS5756_ANALOG_MUTE_DET   (TAS5756_PAGE_BASE(0) + 108)
#define TAS5756_GPIN              (TAS5756_PAGE_BASE(0) + 119)
#define TAS5756_DIGITAL_MUTE_DET  (TAS5756_PAGE_BASE(0) + 120)

#define TAS5756_OUTPUT_AMPLITUDE  (TAS5756_PAGE_BASE(1) +   1)
#define TAS5756_ANALOG_GAIN_CTRL  (TAS5756_PAGE_BASE(1) +   2)
#define TAS5756_UNDERVOLTAGE_PROT (TAS5756_PAGE_BASE(1) +   5)
#define TAS5756_ANALOG_MUTE_CTRL  (TAS5756_PAGE_BASE(1) +   6)
#define TAS5756_ANALOG_GAIN_BOOST (TAS5756_PAGE_BASE(1) +   7)
#define TAS5756_VCOM_CTRL_1       (TAS5756_PAGE_BASE(1) +   8)
#define TAS5756_VCOM_CTRL_2       (TAS5756_PAGE_BASE(1) +   9)

#define TAS5756_CRAM_CTRL         (TAS5756_PAGE_BASE(44) +  1)

#define TAS5756_FLEX_A            (TAS5756_PAGE_BASE(253) + 63)
#define TAS5756_FLEX_B            (TAS5756_PAGE_BASE(253) + 64)

#define TAS5756_MAX_REGISTER      (TAS5756_PAGE_BASE(253) + 64)

/* Page 0, Register 1 - reset */
#define TAS5756_RSTR (1 << 0)
#define TAS5756_RSTM (1 << 4)

/* Page 0, Register 2 - power */
#define TAS5756_RQPD       (1 << 0)
#define TAS5756_RQPD_SHIFT 0
#define TAS5756_RQST       (1 << 4)
#define TAS5756_RQST_SHIFT 4

/* Page 0, Register 3 - mute */
#define TAS5756_RQMR_SHIFT 0
#define TAS5756_RQML_SHIFT 4

/* Page 0, Register 4 - PLL */
#define TAS5756_PLLE       (1 << 0)
#define TAS5756_PLLE_SHIFT 0
#define TAS5756_PLCK       (1 << 4)
#define TAS5756_PLCK_SHIFT 4

/* Page 0, Register 7 - DSP */
#define TAS5756_SDSL       (1 << 0)
#define TAS5756_SDSL_SHIFT 0
#define TAS5756_DEMP       (1 << 4)
#define TAS5756_DEMP_SHIFT 4

/* Page 0, Register 8 - GPIO output enable */
#define TAS5756_G1OE       (1 << 0)
#define TAS5756_G2OE       (1 << 1)
#define TAS5756_G3OE       (1 << 2)
#define TAS5756_G4OE       (1 << 3)
#define TAS5756_G5OE       (1 << 4)
#define TAS5756_G6OE       (1 << 5)

/* Page 0, Register 9 - BCK, LRCLK configuration */
#define TAS5756_LRKO       (1 << 0)
#define TAS5756_LRKO_SHIFT 0
#define TAS5756_BCKO       (1 << 4)
#define TAS5756_BCKO_SHIFT 4
#define TAS5756_BCKP       (1 << 5)
#define TAS5756_BCKP_SHIFT 5

/* Page 0, Register 12 - Master mode BCK, LRCLK reset */
#define TAS5756_RLRK       (1 << 0)
#define TAS5756_RLRK_SHIFT 0
#define TAS5756_RBCK       (1 << 1)
#define TAS5756_RBCK_SHIFT 1

/* Page 0, Register 13 - PLL reference */
#define TAS5756_SREF        (7 << 4)
#define TAS5756_SREF_SHIFT  4
#define TAS5756_SREF_SCK    (0 << 4)
#define TAS5756_SREF_BCK    (1 << 4)
#define TAS5756_SREF_GPIO   (3 << 4)

/* Page 0, Register 14 - DAC reference */
#define TAS5756_SDAC        (7 << 4)
#define TAS5756_SDAC_SHIFT  4
#define TAS5756_SDAC_MCK    (0 << 4)
#define TAS5756_SDAC_PLL    (1 << 4)
#define TAS5756_SDAC_SCK    (3 << 4)
#define TAS5756_SDAC_BCK    (4 << 4)
#define TAS5756_SDAC_GPIO   (5 << 4)

/* Page 0, Register 16, 18 - GPIO source for DAC, PLL */
#define TAS5756_GREF        (7 << 0)
#define TAS5756_GREF_SHIFT  0
#define TAS5756_GREF_GPIO1  (0 << 0)
#define TAS5756_GREF_GPIO2  (1 << 0)
#define TAS5756_GREF_GPIO3  (2 << 0)
#define TAS5756_GREF_GPIO4  (3 << 0)
#define TAS5756_GREF_GPIO5  (4 << 0)
#define TAS5756_GREF_GPIO6  (5 << 0)

/* Page 0, Register 19 - synchronize */
#define TAS5756_RQSY        (1 << 0)
#define TAS5756_RQSY_RESUME (0 << 0)
#define TAS5756_RQSY_HALT   (1 << 0)

/* Page 0, Register 34 - fs speed mode */
#define TAS5756_FSSP        (3 << 0)
#define TAS5756_FSSP_SHIFT  0
#define TAS5756_FSSP_48KHZ  (0 << 0)
#define TAS5756_FSSP_96KHZ  (1 << 0)
#define TAS5756_FSSP_192KHZ (2 << 0)
#define TAS5756_FSSP_384KHZ (3 << 0)

/* Page 0, Register 37 - Error detection */
#define TAS5756_IPLK (1 << 0)
#define TAS5756_DCAS (1 << 1)
#define TAS5756_IDCM (1 << 2)
#define TAS5756_IDCH (1 << 3)
#define TAS5756_IDSK (1 << 4)
#define TAS5756_IDBK (1 << 5)
#define TAS5756_IDFS (1 << 6)

/* Page 0, Register 40 - I2S configuration */
#define TAS5756_ALEN       (3 << 0)
#define TAS5756_ALEN_SHIFT 0
#define TAS5756_ALEN_16    (0 << 0)
#define TAS5756_ALEN_20    (1 << 0)
#define TAS5756_ALEN_24    (2 << 0)
#define TAS5756_ALEN_32    (3 << 0)
#define TAS5756_AFMT       (3 << 4)
#define TAS5756_AFMT_SHIFT 4
#define TAS5756_AFMT_I2S   (0 << 4)
#define TAS5756_AFMT_DSP   (1 << 4)
#define TAS5756_AFMT_RTJ   (2 << 4)
#define TAS5756_AFMT_LTJ   (3 << 4)

/* Page 0, Register 42 - DAC routing */
#define TAS5756_AUPR_SHIFT 0
#define TAS5756_AUPL_SHIFT 4

/* Page 0, Register 59 - auto mute */
#define TAS5756_ATMR_SHIFT 0
#define TAS5756_ATML_SHIFT 4

/* Page 0, Register 63 - ramp rates */
#define TAS5756_VNDF_SHIFT 6
#define TAS5756_VNDS_SHIFT 4
#define TAS5756_VNUF_SHIFT 2
#define TAS5756_VNUS_SHIFT 0

/* Page 0, Register 64 - emergency ramp rates */
#define TAS5756_VEDF_SHIFT 6
#define TAS5756_VEDS_SHIFT 4

/* Page 0, Register 65 - Digital mute enables */
#define TAS5756_ACTL_SHIFT 2
#define TAS5756_AMLE_SHIFT 1
#define TAS5756_AMRE_SHIFT 0

/* Page 0, Register 80-85, GPIO output selection */
#define TAS5756_GxSL       (31 << 0)
#define TAS5756_GxSL_SHIFT 0
#define TAS5756_GxSL_OFF   (0 << 0)
#define TAS5756_GxSL_DSP   (1 << 0)
#define TAS5756_GxSL_REG   (2 << 0)
#define TAS5756_GxSL_AMUTB (3 << 0)
#define TAS5756_GxSL_AMUTL (4 << 0)
#define TAS5756_GxSL_AMUTR (5 << 0)
#define TAS5756_GxSL_CLKI  (6 << 0)
#define TAS5756_GxSL_SDOUT (7 << 0)
#define TAS5756_GxSL_ANMUL (8 << 0)
#define TAS5756_GxSL_ANMUR (9 << 0)
#define TAS5756_GxSL_PLLLK (10 << 0)
#define TAS5756_GxSL_CPCLK (11 << 0)
#define TAS5756_GxSL_UV0_7 (14 << 0)
#define TAS5756_GxSL_UV0_3 (15 << 0)
#define TAS5756_GxSL_PLLCK (16 << 0)

/* Page 1, Register 2 - analog volume control */
#define TAS5756_RAGN_SHIFT 0
#define TAS5756_LAGN_SHIFT 4

/* Page 1, Register 7 - analog boost control */
#define TAS5756_AGBR_SHIFT 0
#define TAS5756_AGBL_SHIFT 4

extern const struct dev_pm_ops tas5756_pm_ops;
extern const struct regmap_config tas5756_regmap;

int tas5756_probe(struct device *dev, struct regmap *regmap);
void tas5756_remove(struct device *dev);

#endif
