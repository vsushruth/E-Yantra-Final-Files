
#define F_CPU 14745600

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "i2c.h"
#include "mpr121.h"
#include "UART.c"
#include "lcd.c"
#include "spi.c"
#include "FAT32.c"
#include "SD.c"

#define MPR121_R 0xB5							// ADD pin is grounded
#define MPR121_W 0xB4							// So address is 0x5A

// global variables

int switch_flag = 0;									//to check boot key press
unsigned char touchstatus,touchstatus1;					//to check if any mpr121 electrodes are touched
														//touchstatus for piano electrodes
														//touchstatus1 for trumpet electrodes

unsigned int config_num[10] = {5,4,3,6,5,6,5,4};					//octaves of notes as given in configuration table
unsigned int config_sharp[10] = {0,0,0,0,1,0,0,0};				//notes are sharp or not, as given in configuration table
unsigned char ans_file[12], send_note[4];				//ans_file contains name of file to be played
														//send_note contains the note details to be sent via UART

int prev_status, prev_status1;							//to store previous touch status of mpr121
int8_t msb,lsb;											//2 bytes read from wav file
int16_t data_read;										//lsb & msb put together and scaled up by 32768  
int count;  											//number of bytes read from sector
int sectors_read = 0;									//number of sectors read from cluster
unsigned long curr_pos;									//position of PC in SD card
int number_of_clusters;									//number of cluster of the file played 
char trumpet_note[9] = "zEDGCAFB\0";					//note configuration for trumpet

unsigned char response, state, address;					//
unsigned int retry = 0, block_addr;						//variable used for i2c communication				       
unsigned char data, data1;								//data & data1 calculate touch status using i2c
		


ISR(TIMER5_OVF_vect)
{
	lsb = spi_receive_data();
	msb = spi_receive_data();

	//put together lsb & msb to make 16bit sample
	data_read = msb << 8;
	data_read |= lsb;
	
	//scale up by 32768 to make the values positive
	int32_t temp_data = (int32_t)data_read;
	temp_data = data_read + 32768;
	data_read = (uint16_t)temp_data;
	
	OCR5A = _BV(data_read);							//write 16bit value to output compare register
	count += 2;										//number of bytes read in sector increases by 2
		
	if(count >= 512)
	{
		spi_receive_data();							// receive incoming CRC (16-bit), CRC is ignored here
		spi_receive_data();
		spi_receive_data();							// extra 8 SCK pulses
		spi_cs_high();								//stop spi communication 
		
		curr_pos = curr_pos + bytes_per_sector;		
		block_addr = cluster_start_sector + 1;		

		spi_read();
		
		count = 0;									
		sectors_read++;
	}

}

void timer5_init()
{
	ICR5 = 0x152C;				//Setting TOP value
								// 0x014F == 335 for sampling frequency 44100 (CPU freq / Sampling freq) 
	TCCR5B = 0x00;				//Stop
	TCNT5 = 0x0000;				//16 bit counter
	OCR5A = 0x0000;				//Output compare register
	
	TIMSK5 = 0x01;				//Overflow interrupt enabled
	TCCR5A = 0xC2;				/*{COM5A1=1, COM5A0=1; COM5B1=0, COM5B0=0; COM5C1=0 COM5C0=0}
 									Set OCR5A only, high on compare match
				  				{WGM51=1, WGM50=0}
									Along With WGM53 & WGM52 in TCCR5B for Selecting FAST PWM with ICR value as TOP*/
	TCCR5B = 0x19;				//WGM53=1 WGM52=1; 
								//CS12=0, CS11=0, CS10=1 (Prescaler=1)
	
}

void check_boot_press()
{
	if(switch_flag != 2){

		// boot switch not pressed
		if ((PIND & 0x40) == 0x40)
		{
			switch_flag = 0;
		}

		// if boot switch pressed, send '#' to Python script
		// this is indicated as Start of Task
		else
		{
			switch_flag = 2;
			uart_tx_string("#");
			uart_tx_string("\n");
		}
	}	
}


// boot switch pin (PD.6) configuration
void boot_switch_pin_config()
{
	DDRD  = DDRD & 0xBF;				// set PD.6 as input
	PORTD = PORTD | 0x40;				// set PD.6 HIGH to enable the internal pull-up
}


// LCD display pin configurations
void lcd_port_config(void)
{
	DDRC = DDRC | 0xF7;					// all LCD pins direction set as output
	PORTC = PORTC & 0x08;				// all LCD pins set to logic 0 except PC.3 (Buzzer pin)
}


// initialize all devices
void init_devices()
{
	DDRE = 0x00;
	PORTE = 0x10;
	
	DDRL |= 0x08;					//output pin for Speaker PWM output 
	PORTL |= 0x08;
	
	uart0_init();
	spi_init();
	timer5_init();
	lcd_init();
	
	spi_pin_config();
	lcd_port_config();
	boot_switch_pin_config();
	
	i2cInit();
	mpr121QuickConfig();
	
	cli();

}


//configure SPI communication for reading a 
//ask the SD card to read from block_addr (READ_SINGLE_BLOCK or CMD17)
int spi_read()
{
	spi_cs_low();
	retry = 0;
	do
	{
		response = sd_card_send_command(READ_SINGLE_BLOCK, block_addr);
		retry++;
		if (retry > 0xFE)												// time out, card not detected
		{
			return 0;
		}
		
	} while (response != 0x00);
	
	retry = 0;															// wait for start block token (0xFE)
	while (spi_receive_data() != 0xFE)
	{
		if (retry++ > 0xFE)
		{
			return 0;
		}
	}
}



int main(void)
{	
	
	int i = 0;
	unsigned char c,d;
	char pin_pressed = 'z', pin_inst = 'z';					//instrument played and pin touched
	int pin_count = 0;										//counter of number of pins touched 
	int touchNumber = 0;									//number of pins touched at once
	int t1 = 0, t2 = 0, t3 = 0, trumpet_pin = 0;			//temporary variable to calculate the played trumpet note
							
	int cluster_number = 0;									//current cluster being played
	int cluster_read = 0;									//number of clusters read

	switch_flag = 0;
	
	init_devices();
	lcd_clear();
	lcd_cursor(0,0);
	
	//initializing SD card
	c = sd_card_init();
	if((int)c)
		lcd_string('0','0',"Card INITIALIZED");
	else
		lcd_string('0','0',"Card Some Error");
		

	_delay_ms(1000);
	lcd_clear();

	//setting up SD card for data manipulation
	d = get_boot_sector_data();	
	if((int)d)
		lcd_string('0','0',"Boot Sector Data Read");
	else
		lcd_string('0','0',"Boot Sector Error");


    // check for boot switch press
     while(switch_flag != 2)
 		check_boot_press();

	
	while(1)
	{

		if(state != 0x10 )
        {	
			touchNumber = 0;
			check_touch_status();
            if(touchstatus1 != 0)
            {    
                // small delay to make sure all trumpet arms are touching.
                _delay_ms(100);
                check_touch_status();
            }

			for (int j=0; j<8; j++)  				// Check how many electrodes were pressed in Piano side
			{
				if ((touchstatus & (1<<j)))
				touchNumber++;
			}
			for (int j=0; j<3; j++)  				// Check how many electrodes were pressed in Trumpet side
			{
				if ((touchstatus1 & (1<<j)))
				touchNumber++;
			}

			//execute only if some electrode is touched
            //to find which electrode(s) was touched
			if(touchNumber != 0)
			{

                //piano side electrodes
				if (touchstatus & (1<<0))
 				{
					pin_pressed = '1';
					pin_inst = 'P';
					
					lcd_clear();
 					lcd_string(0,0,"pin 0"); 			
				}
				else if (touchstatus & (1<<1))
				{
					pin_pressed = 'C';
					pin_inst = 'P';
					lcd_clear();
 					lcd_string(0,0,"pin 1");
				}
				else if (touchstatus & (1<<2))
				{
					pin_pressed = 'D';
					pin_inst = 'P';
					lcd_clear();
 					lcd_string(0,0,"pin 2");
				}			
				else if (touchstatus & (1<<3))
					{
					pin_pressed = 'E';
					pin_inst = 'P'; 				
					lcd_clear();
					lcd_string(0,0,"pin 3");
				}
				else if (touchstatus & (1<<4))
				{
					pin_inst = 'P';
					pin_pressed = 'F';
					lcd_clear();
					lcd_string(0,0,"pin 4");
				}
				else if (touchstatus & (1<<5))
				{
					pin_inst = 'P';
					pin_pressed = 'G';
					lcd_clear();
					lcd_string(0,0,"pin 5");
				}
				else if (touchstatus & (1<<6))
				{
					pin_inst = 'P';
					pin_pressed = 'A';
					lcd_clear();
					lcd_string(0,0,"pin 6");
				}
				else if (touchstatus & (1<<7))
				{
					pin_inst = 'P';
					pin_pressed = 'B';
					lcd_clear();
					lcd_string(0,0,"pin 7");
				}
                else				 
                {
                    //trumpet side electrodes
                    //pin 8 in mpr121 is not used
                    if (touchstatus1 & (1<<1))
                    {
                        pin_inst = 'T';
                        t1 = 1;
                        lcd_clear();
                        lcd_string(0,0,"pin 9");
                    }
                    if (touchstatus1 & (1<<2))
                    {
                        pin_inst = 'T';
                        t2 = 1;
                        lcd_clear();
                        lcd_string(0,0,"pin 10");
                    }
                    if (touchstatus1 & (1<<3))
                    {
                        pin_inst = 'T';
                        t3 = 1;
                        lcd_clear();
                        lcd_string(0,0,"pin 11");
                    }

                    trumpet_pin = (t1 << 2) | (t2 << 1) | t3;			//combining t1,t2,t3 to make a value from 0 to 7
                                                                        //each of the value corresponds a note
                    pin_pressed = trumpet_note[trumpet_pin];			//notes stored in trumpet_note array
                }
			}		
		}
		
		//execute only if the electrode touched is valid
		if(pin_pressed != 'z' )
		{

			if(pin_pressed == '1')							//execute if the end note is to be played
			{
				sprintf(ans_file, "end.wav\0");
				switch_flag = 1;
				
				uart_tx_string('$');
				uart_tx_string('\n');								//first key is pressed.
																	//End of task is signaled.
                switch_flag = 1;
				pin_pressed = 'z';							//resetting variable
				pin_count++;
			}			
			else if(pin_inst == 'P')						//execute if one of the piano keys is played
			{
				if(config_sharp[pin_count])
				{
					sprintf(ans_file, "%c#%d_Pia.wav\0", pin_pressed,config_num[pin_count]);
					sprintf(send_note, "%c#%d\0", pin_pressed,config_num[pin_count]);
				}				
				else
				{
					sprintf(ans_file, "%c%d_Pia.wav\0", pin_pressed,config_num[pin_count]);
					sprintf(send_note, "%c%d\0", pin_pressed,config_num[pin_count]);
				}			
				
				prev_status = touchstatus;					//initializing current touch status
				pin_pressed = 'z';							//resetting variable
				pin_count++;					
				
				uart_tx_string("Piano\n");					//sending details of instrument played
				//_delay_ms(250);
				uart_tx_string(send_note);					//sending details of key pressed
				uart_tx_string("\n");
				
			}		
			else if(pin_inst == 'T')						//execute if one of the trumpet keys is played
			{
				if(config_sharp[pin_count])
				{
					sprintf(ans_file, "%c#%d_Tru.wav\0", pin_pressed,config_num[pin_count]);
					sprintf(send_note, "%c#%d\0", pin_pressed,config_num[pin_count]);
				}				
				else
				{
					sprintf(ans_file, "%c%d_Tru.wav\0", pin_pressed,config_num[pin_count]);
					sprintf(send_note, "%c%d\0", pin_pressed,config_num[pin_count]);
				}				
				
				prev_status1 = touchstatus1;				//initializing current touch status
				pin_count++;
				pin_pressed = 'z';							//
				t1 = 0; t2 = 0; t3 = 0; trumpet_pin = 0;	//resetting variables
				
				uart_tx_string("Trumpet\n");				//sending details of instrument played	
				//_delay_ms(250);
				uart_tx_string(send_note);					//sending details of key pressed
				uart_tx_string("\n");
			}	
		


			lcd_clear();
			lcd_string(0,0,ans_file);
			
			_delay_ms(500);
			//lcd_clear();
			
			//execute only if the file exists
			if((int)get_file_info(READ, ans_file))
			{
				
				lcd_string('0','0'," File found");			
				
				lcd_clear();
				lcd_string('0','0',"Reading File");
				//_delay_ms(500);

                //calculating number of clusters in file
				number_of_clusters = file_size / (bytes_per_sector * sectors_per_cluster);		

				//initializing all counters to 0
				cluster_number = 0;			
				cluster_read = 0;
				sectors_read = 0 ;
				
                //finding address of first sector of file in SD card
				cluster_start_sector = get_first_sector(first_cluster);
				block_of_cluster = ((curr_pos >> 9) & (sectors_per_cluster - 1));
				block_addr = cluster_start_sector + block_of_cluster;
				
				spi_read();															
				
				//discard first 44bytes of wav file
				unsigned int dump;
				for (i = 0; i < 44; i++)
				{
					dump = spi_receive_data();
				}

				lcd_clear();
				lcd_string(0,0,"READING");
				//_delay_ms(1000);
				
				//reset timer, enable global interrupts
				TCNT5 = 0x00;																		
				sei();
				
				//loop until all clusters of file are played
				while(cluster_read < number_of_clusters)
				{
					
					check_touch_status();
					if(touchstatus1 != prev_status1 || touchstatus != prev_status)
					{
						cli();
						break;
					}
					
					//get next sector once the current one has been completely played 
					if(sectors_read > sectors_per_cluster)
					{
						sectors_read = 0;
						//check if any new key has been played
						check_touch_status();
						lcd_clear();
						lcd_numeric_value('0','0',touchstatus, 5);
						lcd_numeric_value('2','0',touchstatus1, 5);
					
							
						cluster_read++;
							
						if(cluster_read == 1)
							cluster_number = get_set_next_cluster(GET, first_cluster, 0);
						else
							cluster_number = get_set_next_cluster(GET, cluster_number, 0);
							
							
						cluster_start_sector = get_first_sector(cluster_number);
						block_of_cluster = ((curr_pos >> 9) & (sectors_per_cluster - 1));
						block_addr = cluster_start_sector + block_of_cluster;

						spi_read();
						TCNT5 = 0x00;
						
					}
					
				}
				
				//after reading file, clear global interrupts and stop SPI communication
				cli();
				spi_cs_high();					
				
				lcd_clear();
				lcd_string(0,0,"READ");
			}	
			else
			{
				lcd_string('0','0',"File not found");
			}
		
		}

	}
	
}


//reading data from mpr121 register
unsigned char mpr121Read(unsigned char address)
{
	unsigned char data;
	
	i2cSendStart();
	i2cWaitForComplete();
	
	i2cSendByte(MPR121_W);			// write 0xB4
	i2cWaitForComplete();
	
	i2cSendByte(address);			// write register address
	i2cWaitForComplete();
	
	i2cSendStart();
	
	i2cSendByte(MPR121_R);			// write 0xB5
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	
	data = i2cGetReceivedByte();		// Get MSB result
	i2cWaitForComplete();
	i2cSendStop();
	
	cbi(TWCR, TWEN);			// Disable TWI
	sbi(TWCR, TWEN);			// Enable TWI
	
	return data;
}


//writing data to mpr121 registers
void mpr121Write(unsigned char address, unsigned char data)
{
	i2cSendStart();
	i2cWaitForComplete();
	
	i2cSendByte(MPR121_W);			// write 0xB4
	i2cWaitForComplete();
	
	i2cSendByte(address);			// write register address
	i2cWaitForComplete();
	
	i2cSendByte(data);
	i2cWaitForComplete();
	
	i2cSendStop();
}


//configuratio of mpr121 registers and thresholds
void mpr121QuickConfig(void)
{
  // This group controls filtering when data is > baseline.
  mpr121Write(MHD_R, 0x01);
  mpr121Write(NHD_R, 0x01);
  mpr121Write(NCL_R, 0x00);
  mpr121Write(FDL_R, 0x00);
 
  // This group controls filtering when data is < baseline.
  mpr121Write(MHD_F, 0x01);
  mpr121Write(NHD_F, 0x01);
  mpr121Write(NCL_F, 0xFF);
  mpr121Write(FDL_F, 0x02);
  
  // This group sets touch and release thresholds for each electrode
  mpr121Write(ELE0_T, TOU_THRESH);
  mpr121Write(ELE0_R, REL_THRESH);
  mpr121Write(ELE1_T, TOU_THRESH);
  mpr121Write(ELE1_R, REL_THRESH);
  mpr121Write(ELE2_T, TOU_THRESH);
  mpr121Write(ELE2_R, REL_THRESH);
  mpr121Write(ELE3_T, TOU_THRESH);
  mpr121Write(ELE3_R, REL_THRESH);
  mpr121Write(ELE4_T, TOU_THRESH);
  mpr121Write(ELE4_R, REL_THRESH);
  mpr121Write(ELE5_T, TOU_THRESH);
  mpr121Write(ELE5_R, REL_THRESH);
  mpr121Write(ELE6_T, TOU_THRESH);
  mpr121Write(ELE6_R, REL_THRESH);
  mpr121Write(ELE7_T, TOU_THRESH);
  mpr121Write(ELE7_R, REL_THRESH);
  mpr121Write(ELE8_T, TOU_THRESH);
  mpr121Write(ELE8_R, REL_THRESH);
  mpr121Write(ELE9_T, TOU_THRESH);
  mpr121Write(ELE9_R, REL_THRESH);
  mpr121Write(ELE10_T, TOU_THRESH);
  mpr121Write(ELE10_R, REL_THRESH);
  mpr121Write(ELE11_T, TOU_THRESH);	
  mpr121Write(ELE11_R, REL_THRESH);
  
  // Set the Filter Configuration
  // Set ESI2
  mpr121Write(FIL_CFG, 0x04);
  
  // Electrode Configuration
  // Enable 6 Electrodes and set to run mode
  // Set ELE_CFG to 0x00 to return to standby mode
 
  mpr121Write(ELE_CFG, 0x0C);						// Enables all 12 Electrodes
  
  // Enable Auto Config and auto Reconfig
  
  mpr121Write(ATO_CFG0, 0x0B);
  mpr121Write(ATO_CFGU, 0xC9);	
  mpr121Write(ATO_CFGT, 0xB5);	
}


//to check which electrode of mpr121 is touched
void check_touch_status()
{

	//for trumpet side
	address = 0x01;
	
	i2cSendStart();
	i2cWaitForComplete();
	
	i2cSendByte(MPR121_W);		// write 0xB4
	i2cWaitForComplete();
	
	i2cSendByte(address);		// write register address
	i2cWaitForComplete();
	
	i2cSendStart();
	
	i2cSendByte(MPR121_R);		// write 0xB5
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	
	data1 = i2cGetReceivedByte();	// Get MSB result
	i2cWaitForComplete();
	i2cSendStop();
	
	cbi(TWCR, TWEN);		// Disable TWI
	sbi(TWCR, TWEN);		// Enable TWI
	
	touchstatus1 = data1;

	//for piano side
	address = 0x00;
	
	i2cSendStart();
	i2cWaitForComplete();
	
	i2cSendByte(MPR121_W);		// write 0xB4
	i2cWaitForComplete();
	
	i2cSendByte(address);		// write register address
	i2cWaitForComplete();
		
	i2cSendStart();
	
	i2cSendByte(MPR121_R);		// write 0xB5
	i2cWaitForComplete();
	i2cReceiveByte(TRUE);
	i2cWaitForComplete();
	
	data = i2cGetReceivedByte();	// Get MSB result
	i2cWaitForComplete();
	i2cSendStop();
	
	cbi(TWCR, TWEN);		// Disable TWI
	sbi(TWCR, TWEN);		// Enable TWI
	
	touchstatus = data;

}
