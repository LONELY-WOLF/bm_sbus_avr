/*
* bm_sbus_avr.c
*
* Created: 24-Sep-16 21:00:52
* Author : Artyom
*/

//#define F_CPU 16000000L
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <util/delay.h>

bool inv_logic = false;
char parity = 'n';
uint8_t stop_bits = 1;

void sync()
{
	while (!(TIFR2 & 2))
	{
	}
	TIFR2 = 2; // Reset OC flag
	//TCNT2 = 0; // Count = 0
}

void init(bool _inv_logic, char _parity, uint8_t _stop_bits)
{
	inv_logic = _inv_logic;
	parity = _parity;
	stop_bits = _stop_bits;
	cli();
	//Port setup
	if(inv_logic)
	{
		PORTB = 0x00; //Set all LOW
	}
	else
	{
		PORTB = 0x0F; //Set 0-3 HIGH
	}
	DDRB = 0x0F; //0-3 as output
	//Timer setup
	TCCR2B = 2; // CLK / 8 (2 MHz)
	TCNT2 = 0; // Count = 0
	OCR2A = 19; // Up to 20
	TIFR2 = 2; // Reset OC flag
	TCCR2A = 2; // Start
}

void write(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
	//uint8_t oldSREG = SREG;
	uint8_t data[4];
	uint8_t i = 0;
	uint8_t mask = 1;
	uint8_t p[4] = {0, 0, 0, 0};

	if (inv_logic)
	{
		data[0] = ~b0;
		data[1] = ~b1;
		data[2] = ~b2;
		data[3] = ~b3;
	}
	else
	{
		data[0] = b0;
		data[1] = b1;
		data[2] = b2;
		data[3] = b3;
	}

	register uint8_t n = 0;

	// Count parity
	for (i = 0; i < 8; i++)
	{
		for (n = 0; n < 4; n++)
		{
			if (data[n] & mask) // choose bit
			{
				p[n]++;
			}
		}
		mask <<= 1;
	}
	mask = 1;
	
	//cli();  // turn off interrupts for a clean txmit

	TCNT2 = 0; // Count = 0
	TIFR2 = 2; // Reset OC flag
	sync();

	// Write the start bit
	if (inv_logic)
	{
		PORTB |= 0x0F;
	}
	else
	{
		PORTB &= ~0x0F;
	}

	// Write each of the 8 bits
	for (i = 0; i < 8; i++)
	{
		uint8_t val = 0;
		for (n = 0; n < 4; n++)
		{
			if (data[n] & mask) // choose bit
			{
				val |= (1 << n); // send 1
			}
			else
			{
				val &= ~(1 << n); // send 0
			}
		}
		sync();
		PORTB = val;
		mask <<= 1;
	}

	if (parity == 'o')
	{
		uint8_t val = 0;
		for (n = 0; n < 4; n++)
		{
			if (p[n] & 1)
			{
				val &= ~(1 << n); // send 0
			}
			else
			{
				val |= (1 << n); // send 1
			}
		}
		if(inv_logic)
		{
			val = (~val) & 0x0F;
		}
		sync();
		PORTB = val;
	}
	if (parity == 'e')
	{
		uint8_t val = 0;
		for (n = 0; n < 4; n++)
		{
			if (p[n] & 1)
			{
				val |= (1 << n); // send 1
			}
			else
			{
				val &= ~(1 << n); // send 0
			}
		}
		if(inv_logic)
		{
			val = (~val) & 0x0F;
		}
		sync();
		PORTB = val;
	}

	// restore pin to natural state
	if (inv_logic)
	{
		sync();
		PORTB &= ~0x0F;
	}
	else
	{
		sync();
		PORTB |= 0x0F;
	}
	
	sync();
	if (stop_bits > 1)
	{
		sync();
	}
	//SREG = oldSREG; // turn interrupts back on
	//sei();
}

int main(void)
{
	init(false, 'e', 1);
	while (1)
	{
		write(0x00, 0xFF, 0x55, 'F');
		_delay_ms(1000);
	}
}

