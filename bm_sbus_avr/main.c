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

uint8_t sbuf[5][25];
uint8_t cam_to_buf[5] = {0, 1, 2, 3, 4};
//uint8_t sbus_data[25] = {0x0f,0x01,0x04,0x20,0x00,0xff,0x07,0x40,0x00,0x02,0x10,0x80,0x2c,0x64,0x21,0x0b,0x59,0x08,0x40,0x00,0x02,0x10,0x80,0x00,0x00};

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
	for(uint8_t n = 0; n < 5; n++)
	{
		sbuf[n][0] = 0x0F;
		for(uint8_t i = 1; i < 25; i++)
		{
			sbuf[n][i] = 0x00;
		}
	}
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

void write_frame()
{
	for(uint8_t i = 0; i < 25; i++)
	{
		write(sbuf[cam_to_buf[0]][i], sbuf[cam_to_buf[1]][i], sbuf[cam_to_buf[2]][i], sbuf[cam_to_buf[3]][i]);
	}
}

void write_byte(uint8_t b)
{
	while(!(UCSR0A & (1<<UDRE0)))
	{

	}
	UDR0 = b;
}

uint8_t read_byte()
{
	while(!(UCSR0A & (1<<RXC0)))
	{
	
	}
	return UDR0;
}

int main(void)
{
	//Init USART
	UCSR0C |= (1 << USBS0); //2 stop bits
	UBRR0L = 103; //9600
	UCSR0B |= (1<<RXEN0) | (1<<TXEN0);

	//Init SW USART
	init(true, 'e', 2);
	//while (1)
	//{
		//write_byte('0');
		//_delay_ms(1000);
	//}
	while(1)
	{
		write_byte('2');
		while(read_byte() != 0x0F)
		{
			
		};
		write_byte('3');
		uint8_t cam = read_byte();
		uint8_t ctb = cam_to_buf[4]; //buffer
		sbuf[ctb][0] = 0x0F;
		sbuf[ctb][24] = 0xFF;
		for(uint8_t i = 1; i < 25; i++)
		{
			sbuf[ctb][i] = read_byte();
		}
		if(sbuf[ctb][24] == 0)
		{
			cam_to_buf[4] = cam_to_buf[cam];
			cam_to_buf[cam] = ctb;
			write_frame();
			write_byte('0');
		}
		write_byte('1');
	}
	while (1)
	{
		for(uint8_t i = 0; i < 100; i++)
		{
			write_frame();
			_delay_ms(7);
		}
		sbuf[0][2] = 0x00;
		for(uint8_t i = 0; i < 100; i++)
		{
			write_frame();
			_delay_ms(7);
		}
		sbuf[0][2] = 0xFF;
	}
}

