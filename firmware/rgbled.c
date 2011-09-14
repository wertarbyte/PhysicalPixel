/* Teensy RawHID example
 * http://www.pjrc.com/teensy/rawhid.html
 * Copyright (c) 2009 PJRC.COM, LLC
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above description, website URL and copyright notice and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "usb_rawhid.h"

#define CPU_PRESCALE(n)	(CLKPR = 0x80, CLKPR = (n))

volatile uint8_t do_output=0;
uint8_t buffer[64];

uint8_t color[3];

int main(void)
{
	int8_t r;

	color[0] = 255;
	color[1] = 0;
	color[2] = 0;

	// set for 16 MHz clock
	CPU_PRESCALE(0);

	// Initialize the USB, and then wait for the host to set configuration.
	// If the Teensy is powered without a PC connected to the USB port,
	// this will wait forever.
	usb_init();
	while (!usb_configured()) /* wait */ ;

	// Wait an extra second for the PC's operating system to load drivers
	// and do whatever it does to actually be ready for input
	_delay_ms(1000);

        TCCR0B = 1<<CS00;
	OCR0A = 0x10;

	TIMSK0 |= (1 << OCIE0A);
	DDRD = (1<<PD6 | 1<<PD0 | 1<<PD1 | 1<<PD2);
	PORTD = 1<<PD6;
	sei();

	while (1) {
		// if received data, do something with it
		r = usb_rawhid_recv(buffer, 0);
		if (r >= 3) {
			color[0] = buffer[0];
			color[1] = buffer[1];
			color[2] = buffer[2];
		}
	}
}

static void inline set_led(uint8_t cnt, uint8_t setting, volatile uint8_t *port, uint8_t bit) {
	if (cnt < setting) {
		*port |= 1<<bit;
	} else {
		*port &= ~(1<<bit);
	}
}

static void update_leds(void) {
	static volatile uint8_t cnt = 0;
	set_led(cnt, color[0], &PORTD, PD0);
	set_led(cnt, color[1], &PORTD, PD1);
	set_led(cnt, color[2], &PORTD, PD2);
	cnt++;
	cnt %= 256;
}

ISR(TIMER0_COMPA_vect) {
	update_leds();
}
