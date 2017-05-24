/* Copyright 2012
 * Jaap de Jong, jaap.dejong@nedap.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <asm/arch/at91_pio.h>
#include <watchdog.h>

#ifdef CONFIG_CMD_RC4

typedef struct rc4_state {
	u_char	perm[256];
	u_char	index1;
	u_char	index2;
} rc4_state;

#define swap_bytes(a, b) { temp = *a; *a = *b; *b = temp; }

/*
 * Initialize an RC4 state buffer using the supplied key,
 * which can have arbitrary length.
 */
void rc4_init(rc4_state *state, const u_char *key, int keylen)
{
	int i;
	u_char j = 0;
	u_char temp;

	/* Initialize state with identity permutation */
	for (i = 0; i < 256; i++)
		state->perm[i] = (u_char) i;
	state->index1 = 0;
	state->index2 = 0;

	/* Randomize the permutation using key data */
	for (i = 0; i < 256; i++) {
		j += state->perm[i] + key[i % keylen];
		swap_bytes(&state->perm[i], &state->perm[j]);
	}
}

/*
 * Encrypt some data using the supplied RC4 state buffer.
 * The input and output buffers may be the same buffer.
 * Since RC4 is a stream cypher, this function is used
 * for both encryption and decryption.
 */
void rc4_crypt(rc4_state *state, u_char *buf, long buflen)
{
	u_char	j;
	u_char temp;

	while (buflen--) {

		/* Update modification indicies */
		state->index1++;
		state->index2 += state->perm[state->index1];

		/* Modify permutation */
		swap_bytes(&state->perm[state->index1], &state->perm[state->index2]);

		/* Encrypt/decrypt next byte */
		j = state->perm[state->index1] + state->perm[state->index2];
		*(buf++) ^= state->perm[j];

		/* watchdog... */
		if ((buflen & 0x7fffff) == 0) {
			WATCHDOG_RESET();
		}
	}
}

/*
 * handle the command.
 */
int cmd_rc4 (cmd_tbl_t * cmdtp, int flag, int argc, char * const argv[])
{
	unsigned long size = 0;
	u_char *addr;
	rc4_state state;
	u_char key[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

	if (argc != 3) return cmd_usage(cmdtp);

	addr = (u_char *)simple_strtoul(argv[1], NULL, 16);
	size = simple_strtoul(argv[2], NULL, 16);

	rc4_init(&state, key, sizeof(key));
	rc4_crypt(&state, addr, size);

	return 0;
}

U_BOOT_CMD (rc4, 3, 1, cmd_rc4,
	"encrypt/decrypt memory",
	"rc4 <addr> <size>\n"
	"	- example:\n"
	"	-	run kernel_ubi_read\n"
	"	-	ubifsload 0x70000000 /flash/ubifilesystem.bin\n"
	"	-	rc4 0x70000000 ${filesize}\n"
	"	-	nand erase 0x00000000 0x00020000\n"
	"	-	nand write 0x70000600 0x00000000 0x00020000");

#endif
