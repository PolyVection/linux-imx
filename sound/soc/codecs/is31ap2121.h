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

#ifndef _IS31AP2121_H
#define _IS31AP2121_H


// IS31AP2121 I2C-bus register addresses

#define AP2121_STATE_CTL_1		0x00
#define AP2121_STATE_CTL_2		0x01
#define AP2121_STATE_CTL_3		0x02
#define AP2121_MASTER_VOL		0x03
#define AP2121_CH1_VOL			0x04
#define AP2121_CH2_VOL			0x05
#define AP2121_CH3_VOL			0x06
#define AP2121_BASS_CTL			0x07
#define AP2121_TREBLE_CTL		0x08
#define AP2121_BASS_XOVER		0x09
#define AP2121_STATE_CTL_4		0x0A
#define AP2121_CH1_CONF			0x0B
#define AP2121_CH2_CONF			0x0C
#define AP2121_CH3_CONF			0x0D
#define AP2121_DRC_LIM_ATT_REL		0x0E
#define AP2121_STATE_CTL_5		0x11
#define AP2121_UNDER_VOLTAGE		0x12
#define AP2121_NOISE_GATE_GAIN		0x13
#define AP2121_COEF_RAM_BASE		0x14
#define AP2121_COEF_CTL			0x24
#define AP2121_PWR_SAVING		0x2A
#define AP2121_VOL_FINE_TUNE		0x2B

#define AP2121_MAX_REGISTER		0x2B


// Bitmasks for registers
#define AP2121_MASTER_MUTE		0x08

#endif  /* _IS31AP2121_H */
