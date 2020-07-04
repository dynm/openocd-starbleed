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

#ifndef OPENOCD_PLD_XILINX_BIT_H
#define OPENOCD_PLD_XILINX_BIT_H

#define MAL_ENC_DWC_WORD_LEN_MAX 0x00000000fc
#define MAL_ENC_DWC_WORD_LEN_MIN 0x0000000098

struct xilinx_bit_file
{
	uint8_t unknown_header[13];
	uint8_t *source_file;
	uint8_t *part_name;
	uint8_t *date;
	uint8_t *time;
	uint32_t length;
	uint8_t *data;
	uint8_t rolling_delta;
	uint32_t dwc_length_offset;
	uint32_t dwc_length;
	uint32_t cipher_start;
};

struct malicious_bitstream
{
	uint8_t *header;
	uint32_t header_len;
	uint32_t wbstar_cipher[8];
	uint32_t fabric_cipher[8];
	uint32_t *fabric_footer;
};

int xilinx_read_bit_file(struct xilinx_bit_file *bit_file, const char *filename);
int generate_malicious_bit_template(struct xilinx_bit_file *bit_file, struct xilinx_bit_file *bit_file_mod);
int change_dwc_len(struct xilinx_bit_file *bit_file_mod);
extern uint32_t bswap32(const uint32_t input);
#endif /* OPENOCD_PLD_XILINX_BIT_H */
