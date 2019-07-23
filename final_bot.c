/*
 *	Team Id : eYRC#1235
 *	Author: K Rahul Reddy
 *	File name : bot.c
 *	Theme : Mocking Bot
 *	Functions : port_init(), timer1_init(), servo1(int), servo2(int), servoL(int), servoM(int), servoR(int), servo_free(), piano_strike(char), start_strike(char ins, char note), stop_strike(char ins), init(), delay(int), myAtoi(char*)
 *	Global variables : switch_flag, servo2_strike_angle, servo2_initial_angle, servoL_strike_angle, servoM_strike_angle, servoR_strike_angle, servoL_initial_angle, servoM_initial_angle, servoR_initial_angle
 */ 

#define F_CPU 14745600
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "lcd.c"
#include "UART.c"

// global variables
int switch_flag;
int servo2_strike_angle, servo2_initial_angle, servoL_strike_angle, servoM_strike_angle, servoR_strike_angle, servoL_initial_angle, servoM_initial_angle, servoR_initial_angle;

/*
	Function name = port_init
	input = void
	output = initializes the pwm ports
*/ 	
void port_init()
{
	DDRB = DDRB | 0x20 | 0x40;		//0x20 ==> pin B5 (piano primary servo ==> servo1)
	PORTB = PORTB | 0x20 | 0x40;		//0x40 ==> pin B6 (piano secondary servo  ==> servo2)
	DDRE = DDRE | 0x08| 0x10 | 0x20;	//0x80 ==> pin E3 (trumpet L)
	PORTE = PORTE | 0x08 | 0x10 | 0x20;	//0x10 ==> pin E4 (trumpet M)
						//0x20 ==> pin E5 (trumpet R)
}

void timer1_init()    //for getting PWM
{
	TCCR1A = 0x00;
	ICR1 = 1023;
	TCNT1H = 0xFC;
	TCNT1L = 0x01;
	OCR1A = 1023;
	OCR1B = 1023;
	TCCR1A = 0xA3;
	TCCR1B = 0x0C;
}

void timer3_init()    //for getting PWM
{
	TCCR3A = 0x00;
	ICR3 = 1023;
	TCNT3H = 0xFC;
	TCNT3L = 0x01;
	OCR3A = 1023;
	OCR3B = 1023;
	OCR3C = 1023;
	TCCR3A = 0xAB;
	TCCR3B = 0x0C;
}

/*
	Function name = servo1
	input = unsigned char
	output = rotates servo by the input angle
	example = servo1(90);
*/ 
void servo1(unsigned char degrees)
{
	float regval = ((float)degrees * 0.521) + 34.56;
	OCR1A = (uint16_t) regval;
}

/*
	Function name = servo2
	input = unsigned char
	output = rotates servo by the input angle
	example = servo2(90);
*/ 
void servo2(unsigned char degrees)
{
	float regval = ((float)degrees * 0.521) + 34.56;
	OCR1B = (uint16_t) regval;
}

/*
	Function name = servoL
	input = unsigned char
	output = rotates servo by the input angle
	example = servoL(20);
*/ 
void servoL(unsigned char degrees)
{
	float regval = ((float)degrees * 0.521) + 34.56;
	OCR3A = (uint16_t) regval;
}

/*
	Function name = servoM
	input = unsigned char
	output = rotates servo by the input angle
	example = servoM(20);
*/ 
void servoM(unsigned char degrees)
{
	float regval = ((float)degrees * 0.521) + 34.56;
	OCR3B = (uint16_t) regval;
}

/*
	Function name = servoR
	input = unsigned char
	output = rotates servo by the input angle
	example = servoR(20);
*/ 
void servoR(unsigned char degrees)
{
	float regval = ((float)degrees * 0.521) + 34.56;
	OCR3C = (uint16_t) regval;
}


void servo_free()
{
	OCR1A = 1023;
	OCR1B = 1023;
	OCR3A = 1023;
	OCR3B = 1023;
	OCR3C = 1023;
}

/*
	Check if boot button has been pressed
*/
void check_boot_press()
{
	// boot switch not pressed
	if ((PIND & 0x40) == 0x40)
	{
		switch_flag = 0;
	}
	// if boot switch pressed
	// this is indicated as Start of Task
	else
		switch_flag = 1;
}


// boot switch pin (PD.6) configuration
void boot_switch_pin_config()
{
	DDRD  = DDRD & 0xBF;		// set PD.6 as input
	PORTD = PORTD | 0x40;		// set PD.6 HIGH to enable the internal pull-up
}


/*
	Function name = init
	input = void 
	output = initializes timer, lcd, uart, boot switch and servo motors
*/ 
void init()
{
	boot_switch_pin_config();
	cli();
	DDRC = DDRC | 0xf7;
	PORTC = PORTC & 0x08;
	sei();

	lcd_init();
	
	cli();
	
	uart0_init();
	
	sei();
	cli();
	port_init();
	timer1_init();
	timer3_init();
	sei();
}
/*
	Function name = stop_strike
	input = ins
	output = lifts the bot arm to stop striking
*/
void stop_strike(char ins)
{
	if(ins == 'P')
		servo2(servo2_initial_angle);
	else
	{
		servoL(servoL_initial_angle);
		servoM(servoM_initial_angle);
		servoR(servoR_initial_angle);
	}
}

/*
	Function name = piano_strike
	input = char
	output = rotate servo1 to correct position and strike servo2 arm
	example = strike('C');
	(proper striking angles for notes have been calibrated)
*/ 
void piano_strike(char note)
{
	int ang;
	switch(note){
		case '1': ang = 0; break;
		case 'C': ang = 5;	break;
		case 'D': ang = 10; break;
		case 'E': ang = 15; break;
		case 'F': ang = 20; break;
		case 'G': ang = 25; break;
		case 'A': ang = 30; break;
		case 'B': ang = 35; break;
	}
	servo1(ang);
	_delay_ms(900);
	servo2(servo2_strike_angle);
}

/*
	Function to convert string to number
	Similar to function atoi()
*/
int myAtoi(char *str)
{
	int res = 0;
	for (int i = 0; str[i] != '\0'; i++)
	res = res * 10 + (str[i] - '0');
	return res;
}

/*
	Function name = start_strike(char ins, char note)
	input = Note to be played and instrument to play it on.
	output = starts playing the required note on instrument
*/
void start_strike(char ins, char note)
{
	if(ins == 'P')
		piano_strike(note);
	else
	{
		char config[4];
		switch(note){
			case 'C':	servoL(servoL_strike_angle);
						config = "100"; break;
			case 'D':	servoL(servoM_strike_angle);
						config = "010"; break;
			case 'E':	servoL(servoR_strike_angle);
						config = "001"; break;
			case 'F':	servoL(servoL_strike_angle);
						servoL(servoM_strike_angle);
						config = "110"; break;
			case 'G':	servoL(servoM_strike_angle);
						servoL(servoR_strike_angle);
						config = "011"; break;
			case 'A':	servoL(servoL_strike_angle);
						servoL(servoR_strike_angle);
						config = "101"; break;
			case 'B':	servoL(servoL_strike_angle);
						servoL(servoM_strike_angle);
						servoL(servoR_strike_angle);
						config = "111"; break;
			default :	config = "000";
		}
		
	}
}



int main(void)
{
	init();
	while(!switch_flag)
	{
		check_boot_press();
	}
	lcd_string(0, 0, "Waiting for transmission");
	uart_tx_string("#");				//Request Transmission
	char ins_data[100], x = uart_rx();
	char ins[100];
	int len = 0;
	while(x != '$')						//Receive bytes with 200ms delay between them
	{
		if(x == 'P' || x == 'T')
		{
			ins[len] = x;
			len++;
		}
		_delay_ms(200);
		x = uart_rx();
	}
	ins[len] == '\0';
	
	lcd_clear();
	lcd_string(0, 0, ins);
	int notes = len;
	uart_tx('#');
	char note[100];
	x = uart_rx();
	len = 0;
	while(x != '$')						//Receive bytes with 200ms delay between them
	{
		if(x == 'C' || x == 'D' || x == 'E' || x == 'F' || x == 'G' || x == 'A' || x == 'B')
		{
			note[len] = x;
			len++;
		}
		_delay_ms(200);
		x = uart_rx();
	}
	note[len] == '\0';
	lcd_clear();
	lcd_string(0, 0, note);
	_delay_ms(1000);
	uart_tx('#');
	char start_data[100];
	len = 0;
	x = uart_rx();
	while(x != '$')						//Receive bytes with 200ms delay between them
	{
		if(x == ' ' || (x <= '9' && x >= '0'))
		{
			start_data[len] = x;
			len++;
		}
		_delay_ms(200);
		x = uart_rx();
	}
	start_data[len] = '\0';
	lcd_clear();
	lcd_string(0, 0, start_data);
	//_delay_ms(5000);
	lcd_clear();
	int p = 0, start[100];
	for(int i = 0; i < strlen(start_data); i++)
	{
		char tmp[10], t = 0;
		int temp = 0;
		while(start_data[i] != ' ' && start_data[i] != '\n')
		{
			tmp[t++] = start_data[i++];						// onset is converted from string to integer.
		}
		tmp[t] = '\0';
		start[p++] = myAtoi(tmp);
	}
	//_delay_ms(1000);
	uart_tx('#');
	char end_data[100];
	len = 0;
	x = uart_rx();
	while(x != '$')						//Receive bytes with 200ms delay between them
	{
		if(x == ' ' || (x <= '9' && x >= '0'))
		{
			end_data[len] = x;
			len++;
		}
		_delay_ms(200);
		x = uart_rx();
	}
	
	end_data[len] = '\0';
	lcd_clear();
	lcd_string(0, 0, end_data);
	_delay_ms(5000);
	lcd_clear();
	p = 0;
	int end[100];
	for(int i = 0; i < strlen(end_data); i++)
	{
		char tmp[10], t = 0;
		int temp = 0;
		while(end_data[i] != ' ' && end_data[i] != '\n')
		{
			tmp[t++] = end_data[i++];						// onset is converted from string to integer.
		}
		tmp[t] = '\0';
		if(tmp[0] != '\0')
			end[p++] = myAtoi(tmp);
	}
	lcd_clear();
	lcd_string(0, 0, "Received!");
	_delay_ms(500);
	uart_tx('$');
	lcd_clear();
	for(int i = 0; i < notes; i++)
	{
		char tmp[20];
		sprintf(tmp, "%d %d %c %c", start[i], end[i], ins[i], note[i]);		
		lcd_string(0, 0, tmp);
		_delay_ms(5000);
		lcd_clear();
	}
	_delay_ms(200);
	
	while(!switch_flag)
	{
		check_boot_press();
	}
	int played = 0;
	for(i = 0; i < notes; i++)
	{
		delay(start[i] * 10 - played);
		start_strike(ins[i], note[i]);
		delay(end[i] - start[i]);
		stop_strike(ins[i]);
	}
	start_strike("P", '1');
}


/*
	sets a delay of 'time' milliseconds with 250 as window
*/ 
void delay(int time)		//we cannot set dynamic delay with _delay_ms(), so we use this.
{
	while(time > 0)
	{
		_delay_ms(250);
		time = time - 250;
	}
}
