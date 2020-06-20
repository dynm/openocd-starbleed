/***************************************************************************
 *   Copyright (C) 2006 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef OPENOCD_PLD_VIRTEX2_H
#define OPENOCD_PLD_VIRTEX2_H

#include <jtag/jtag.h>

struct virtex2_pld_device
{
	struct jtag_tap *tap;
	int no_jstart;
};

uint8_t write_wbstar_bitstream[384] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0xBB, 0x11, 0x22, 0x00, 0x44, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xAA, 0x99, 0x55, 0x66, 0x20, 0x00, 0x00, 0x00, 0x30, 0x03, 0xE0, 0x01, 0x00, 0x00, 0x02, 0x6B,
	0x30, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x12, 0x20, 0x00, 0x00, 0x00, 0x30, 0x02, 0x20, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x30, 0x02, 0x00, 0x01, 0x89, 0xAB, 0xCD, 0x00, 0x30, 0x00, 0x80, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x30, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x07,
	0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x30, 0x02, 0x60, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x30, 0x01, 0x20, 0x01, 0x02, 0x00, 0x3F, 0xE5, 0x30, 0x01, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x30, 0x01, 0x80, 0x01, 0x03, 0x75, 0x20, 0x93, 0x30, 0x00, 0x80, 0x01, 0x00, 0x00, 0x00, 0x09,
	0x20, 0x00, 0x00, 0x00, 0x30, 0x00, 0xC0, 0x01, 0x00, 0x00, 0x04, 0x01, 0x30, 0x00, 0xA0, 0x01,
	0x00, 0x00, 0x05, 0x01, 0x30, 0x00, 0xC0, 0x01, 0x00, 0x00, 0x10, 0x00, 0x30, 0x03, 0x00, 0x01,
	0x00, 0x00, 0x10, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x30, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x80, 0x01,
	0x00, 0x00, 0x00, 0x01, 0x20, 0x00, 0x00, 0x00, 0x30, 0x00, 0x40, 0x65, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#endif /* OPENOCD_PLD_VIRTEX2_H */
