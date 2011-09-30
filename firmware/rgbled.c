#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "usbdrv.h"
#include "requests.h" /* custom requests used */

PROGMEM char usbHidReportDescriptor[22] = {    /* USB report descriptor */
	0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
	0x09, 0x01,                    // USAGE (Vendor Usage 1)
	0xa1, 0x01,                    // COLLECTION (Application)
	0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
	0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
	0x75, 0x08,                    //   REPORT_SIZE (8)
	0x95, 0x01,                    //   REPORT_COUNT (1)
	0x09, 0x00,                    //   USAGE (Undefined)
	0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
	0xc0                           // END_COLLECTION
};

static uint8_t color[3] = {255,0,0};

usbMsgLen_t usbFunctionSetup(uchar data[8]) {
	usbRequest_t    *rq = (usbRequest_t *)data;

	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR) {
		switch(rq->bRequest) {
			case CUSTOM_RQ_SET_RGB:
				return USB_NO_MSG;
		}
	} else {
		/* calls requests USBRQ_HID_GET_REPORT and USBRQ_HID_SET_REPORT are
		* not implemented since we never call them. The operating system
		* won't call them either because our descriptor defines no meaning.
		*/
	}
	return 0;   /* default for not implemented requests: return no data back to host */
}

uchar usbFunctionWrite(uchar *data, uchar len) {
	color[0] = data[0];
	color[1] = data[1];
	color[2] = data[2];
	return 1;
}

static void calibrateOscillator(void) {
	uchar       step = 128;
	uchar       trialValue = 0, optimumValue;
	int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

	/* do a binary search: */
	do {
		OSCCAL = trialValue + step;
		x = usbMeasureFrameLength();    // proportional to current real frequency
		if(x < targetValue)             // frequency still too low
			trialValue += step;
		step >>= 1;
	} while (step > 0);
	/* We have a precision of +/- 1 for optimum OSCCAL here */
	/* now do a neighborhood search for optimum value */
	optimumValue = trialValue;
	optimumDev = x; // this is certainly far away from optimum
	for (OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++) {
		x = usbMeasureFrameLength() - targetValue;
		if(x < 0)
			x = -x;
		if(x < optimumDev){
			optimumDev = x;
			optimumValue = OSCCAL;
		}
	}
	OSCCAL = optimumValue;
}

void usbEventResetReady(void) {
	cli();  // usbMeasureFrameLength() counts CPU cycles, so disable interrupts.
	calibrateOscillator();
	sei();
	eeprom_write_byte(0, OSCCAL);   // store the calibrated value in EEPROM
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
	set_led(cnt, color[0], &PORTB, PB4);
	set_led(cnt, color[1], &PORTB, PB3);
	set_led(cnt, color[2], &PORTB, PB1);
	cnt++;
	cnt %= 256;
}

int main(void) {
#if USE_TIMER
	/* configure timer */
        TCCR0B = 1<<CS00;
	OCR0A = 0x10;
	TIMSK0 |= (1 << OCIE0A);
#endif
	/* configure outputs */
	DDRB = (1<<PB1 | 1<<PB3 | 1<<PB4);

	wdt_enable(WDTO_1S);

	/* prepare USB */
	usbInit();
	usbDeviceDisconnect();
	/* fake USB disconnect for >250ms */
	uint8_t i = 255;
	while (i--) {
		wdt_reset();
		_delay_ms(1);
	}
	usbDeviceConnect();

	sei();

	while (1) {
#if !USE_TIMER
		update_leds();
#endif
		wdt_reset();
		usbPoll();
	}
}

#if USE_TIMER
ISR(TIMER0_COMPA_vect) {
	update_leds();
}
#endif
