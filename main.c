/*
 * avrcanbridge - NMEA to CAN bus bridge using AT90CAN128
 * Copyright (C) 2013 Aleksi Pihkanen.  All rights reserved.
 *
 * This file is part of avrcanbridge.
 *
 * avrcanbridge is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avrcanbridge is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avrcanbridge.  If not, see <http://www.gnu.org/licenses/>.
 * See file LICENSE for further informations on licensing terms.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/iocan128.h>
#include <util/delay.h>

#include "uart.h"
#include "can.h"
#include "numberutil.h"

#define LED_PIN PB7

void blink(void)
{
	PORTB |= _BV(LED_PIN);
	_delay_ms(200);
	PORTB &= ~_BV(LED_PIN);
	_delay_ms(100);
}

/* Eight bytes for data to be sent over CAN */
union msg_t {
	uint8_t b[8];
	uint16_t us[4];
	int16_t s[4];
	int32_t l[2];
} msg;

/* Buffer for incoming NMEA sentences */
struct NMEA_buffer {
	char b[83];
	char *f[83];
	uint8_t offset;
	uint8_t checksum;	// *FIXME* not used yet
} buffer;

/* Feed the buffer */
int buffer_append(struct NMEA_buffer *buffer, char c)
{
	switch (c) {
	case '\r':
		buffer->b[buffer->offset] = '\0';
		return 1;
	case '$':
		buffer->offset = 0;
	}
	buffer->b[buffer->offset] = c;
	buffer->offset++;
	if (buffer->offset > 82)
		buffer->offset = 0;
	return 0;
}

/* Set pointers to each data field in a message */
void buffer_split(struct NMEA_buffer *buffer)
{
	uint8_t i = 0;
	uint8_t j = 0;
	while (buffer->b[i]) {
		if (buffer->b[i] == ',')
			buffer->f[j++] = &buffer->b[i + 1];
		i++;
	}
	buffer->f[j] = NULL;
}

int main(void)
{
	char c;
	uint8_t i;
	for (i = 0; i < 8; i++)
		msg.b[i] = i + 3;
	can_init();
	DDRB = _BV(LED_PIN);
	uart_init(9600);
	while (1) {
		c = uart_getchar();
		if (buffer_append(&buffer, c)) {
			buffer_split(&buffer);
			if (strncmp(buffer.b + 3, "GLL", 3) == 0) {
				msg.l[0] = dmmtoint(buffer.f[0], buffer.f[1]);
				msg.l[1] = dmmtoint(buffer.f[2], buffer.f[3]);
				can_send(0x201, msg.b, 8);
			} else if (strncmp(buffer.b + 3, "RMC", 3) == 0) {
				msg.l[0] = dmmtoint(buffer.f[2], buffer.f[3]);
				msg.l[1] = dmmtoint(buffer.f[4], buffer.f[5]);
				can_send(0x201, msg.b, 8);
			} else if (strncmp(buffer.b + 3, "HDT", 3) == 0) {
				msg.s[0] = atof(buffer.f[0]) * 100;
				can_send(0x202, msg.b, 2);
			} else if (strncmp(buffer.b + 3, "ROT", 3) == 0) {
				msg.s[0] = atof(buffer.f[0]) * 100;
				can_send(0x203, msg.b, 2);
			} else if (strncmp(buffer.b + 3, "VTG", 3) == 0) {
				msg.us[0] = atof(buffer.f[0]) * 100;
				msg.us[1] = atof(buffer.f[4]) * 100;
				if (*buffer.f[5] == 'M')
					msg.us[2] = 1000;
				else
					msg.us[2] = 1852;
				can_send(0x204, msg.b, 6);
			} else if (strncmp(buffer.b + 3, "DBT", 3) == 0) {
				msg.s[0] = atof(buffer.f[2]) * 100;
				can_send(0x205, msg.b, 2);
			} else if (strncmp(buffer.b + 3, "VBW", 3) == 0) {
				msg.s[0] = atof(buffer.f[0]) * 100;
				msg.s[1] = atof(buffer.f[1]) * 100;
				can_send(0x206, msg.b, 4);
			} else if (strncmp(buffer.b + 3, "MWV", 3) == 0) {
				msg.us[0] = atof(buffer.f[0]) * 100;
				msg.us[1] = atof(buffer.f[2]) * 100;
				if (*buffer.f[3] == 'M')
					msg.us[2] = 1000;
				else
					msg.us[2] = 1852;
				can_send(0x208, msg.b, 6);
			}
		}
	}
}
