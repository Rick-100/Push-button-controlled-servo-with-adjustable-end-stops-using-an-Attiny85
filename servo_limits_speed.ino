/*
* ATTINY85 prog to to toggle servo between 2 end positions
* the end positions are adjustable and saved in eeprom so they are not lost when power is turned off
* the user interface is 3 pushbuttons PLUS   PROG    MINUS and an led
* 
* mode 0 = solid led / normal running mode 
*  pressing PLUS moves the servo end PLUS end position
*  pressing MINUS moves the servo end MINUS end position
* 
* mode 1 = slowest flashing led  
* the prog button was pressed and it's waiting for a press on PLUS or MINUS to select which end to adjust
* the servo will move to the initial position stored in variables plus_limit or minus_limit and enter mode 2
* 
* mode 2 = faster flashing led / the servo positions are adjusted until PROG is pressed and released  
*  then there stored in eeprom and mode 0 is selected
*
*mode 3 = even faster flashing led on power up / the eeprom was read and the data was out of range or bad checksum 
*  the limits were restored to defaults but not written to eeprom
*  when the PROG button is pressed and released the data is written to eeprom and mode is set to 0
*  back to solid led
*
*mode 4 = fastest flashing led / happens after aprox 10 seconds of no buttons pressed 
*  servos pulses have been disabled so the servo is no longer actively driven
*  pressing PLUS or MINUS enables the servo pulses and puts mode back to 0 with solid led
*  pressing and releasing PROG enables servo pulses and puts mode back to 0 with solid led
* 
*
*to reset to values stored in constants MAXPLUS and MAXMINUS power up with PROG button held down
*
*Programming settings  in Arduino IDE ver 1.8.12
*  Attiny is running at 8 MHz internal
*  Timer 1 clock: CPU frequency
*  millis() micos(): disabled
*  Save eeprom: eeprom not detained
*  B.O.D. disabled
*  
*  PB1 (leg 6) PROG button
*  PB2 (leg 7) PLUS
*  PB0 (leg 5) MINUS
*  PB3 (leg 2) servo
*  PB4 (leg 3) led
*
*The interrupt driven servo routines are a modified version of routines from this program
*by Andreas Ennemoser
*https://github.com/chiefenne/ATTINY85-Servo-Control/blob/master/main.c
*
*
*
*
*This version has a servo_speed variable. It will slow the servo down when it's in the mode 0 (running mode)
*The servo still moves at full speed in other modes when your adjusting the end points. I tested the values 1 , 3 , and 50.
*A value of 1 is the slowest. A value of 50 seems to be full speed. A value of 0 results in no movement.
*/

#include <EEPROM.h>

#define SERVO PB3   // define pin where servo is attached (one of PB0, PB1, PB2, PB3, PB4)
#define LED PB4

#define BUTTONPLUS PINB2  //buttons
#define BUTTONMINUS PINB0
#define BUTTONPROG PINB1

#define MODE0RATE 0   // blink rates for the modes lower number = faster blink / 0 = no blink
#define MODE1RATE 20
#define MODE2RATE 10
#define MODE3RATE 5
#define MODE4RATE 1

#define TIMEOUT 500 //.02 * 500 = 10 seconds



//defaults servo positions servo operates between 2 and 187
//pulse lengths between approximately 750uS and 2250 uS
//ex: to convert 187 to pulse length
//each timer tick is 1/8000000 * 64 or .000008 seconds  / 64 is the prescaler
//multiply .000008 by 187 to get .001496 seconds
// add it to the base of .000750 seconds to get .002246 or 2246 uS
#define MAXPLUS 187 
#define MAXMINUS 2  
#define POSMID 93

//#define SELECTPLUS 140  // initialpositions for servo adjust 
//#define SELECTMINUS 46



#define SERVO_PWM_ON PORTB |= (1 << SERVO)
#define SERVO_PWM_OFF PORTB &= ~(1 << SERVO)
//#define LED_ON PORTB |= (1 << LED)
//#define LED_OFF PORTB &= ~(1 << LED)
#define LED_OFF PORTB |= (1 << LED)
#define LED_ON PORTB &= ~(1 << LED)
#define LED_TOGGLE PORTB ^= (1<< LED)

// global variables

volatile uint8_t overflows = 0;              // number of timer1 overflows (0-255)

volatile uint8_t servo_pos;     
volatile bool ok_change; // set when timer starts it's 20 mS phase
volatile bool servo_enable;  //if true drive the servo

uint8_t plus_limit;
uint8_t minus_limit;

uint16_t jiffy_clock; // keeps track of 50 Hertz ticks

uint8_t servo_speed = 3; // number of 'steps' the servo moves on each jiffy clock tick/ 1 - 50 seem to work well /  don't use 0


ISR(TIMER1_OVF_vect){

	// interrupt service routine for timer1 overflow
	// 1 interrupt is approx. 2ms

	overflows++;
	
	if (overflows == 10){                    // corresponds to approx 20ms servo frequency (50Hz) .02048 seconds
		if(servo_enable)
		{
			SERVO_PWM_ON;
		}
		//SERVO_PWM_ON;
		TCNT1 = 162;  // set up base .000750 sec pulse*****  (.000750 / (64 * (1/8000000)) = 93.75) ****** 256 - 94 = 162
	}

	if (overflows == 11){                     // corresponds to 1ms servo pulse minimum
		//flag = 1;
		overflows = 0;
		// adds in the compare B match interrupt ISR the remaining part of servo pulse on top of this minimum 1ms pulse
		// can be anything between 0ms and 1ms (or 0-255 steps according to timer1 setup)
		//OCR1B = servo_pwm_duty_int;          // set Timer/Counter1 Output Compare RegisterB (datasheet page 91)
		OCR1B = servo_pos;          // set Timer/Counter1 Output Compare RegisterB (datasheet page 91)
		//TCNT1 = 0;                           // reset counter for the upcoming match with OCR1B
		TIFR |= (1 << OCF1B); // clear the interrupt flag by writing a one to it
		TIMSK |= (1 << OCIE1B);                  // enable compare match B interrupt of timer1 (page 92)
	}

}

ISR(TIMER1_COMPB_vect){

	// interrupt service routine for timer1 compare match based on OCR1B value
	// this match happens anywhere between 1ms and 2ms after SERVO_PWM_ON
	SERVO_PWM_OFF;
	TIMSK &= ~(1 << OCIE1B);                  // disable compare match B interrupt of timer1 (page 92)
	ok_change = true;    // signal foreground program it's time
}


void Init_PORT(void)
{
	DDRB |= (1 << SERVO);     // set servo pin as output
	DDRB |= (1 << LED);     // set led pin as output
	//PORTB |= (1 << BUTTONPLUS);   //turn on pullups
	//PORTB |= (1 << BUTTONMINUS);
	//PORTB |= (1 << BUTTONPROG);

}

void Init_TIMER()
{

	// this function initializes the timer
	// the timer is set up in order to last approx. 1ms per overflow
	// the CPU runs at 8MHz (requires that CKDIV8 fuse is NOT set)
	// timer 1 is used as it allows for more prescalers; here 32 is used to come from 8MHz to 250kHz

	TCCR1 |= (1 << CS12) | (1 << CS11) | (1 << CS10);      // set prescaler of timer1 to 64


}

void Init_INTERRUPTS()
{
	// this function initializes the interrupts
	TIMSK |= (1 << TOIE1);                   // enable overflow interrupt of timer1 (page 92)
	// did not work with OCIE1A???
	//TIMSK |= (1 << OCIE1B);                  // enable compare match B interrupt of timer1 (page 92)
	sei();                                   // enable interrupts (MANDATORY)

}

void storeValues()
{
	uint8_t checksum;

	checksum = plus_limit + minus_limit;
	EEPROM.write(0,(int)plus_limit);
	//EEPROM.write(0,0);
	EEPROM.write(1,(int)minus_limit);
	//EEPROM.write(1,0);
	EEPROM.write(2,(int)checksum);
}

bool loadValues()
{
	uint8_t checksum;
	uint8_t temp;

	plus_limit = EEPROM.read(0);
	minus_limit = EEPROM.read(1);
	checksum = EEPROM.read(2);
	temp = plus_limit + minus_limit;
	if(checksum == temp)
	{
		return false; //ok
	}
	else
	{
		return true; //error
	}

}

void startServoPulses()
{
	Init_TIMER();                            // initialize timer
	Init_INTERRUPTS();                       // initialize interrupts

}

void stopServoPulses()
{
	cli();
}

void setup() {
	// put your setup code here, to run once:

}

void loop() {

	uint8_t mode;
	uint8_t blink_rate;
	uint8_t blink_count;
	uint8_t limit_sel;


	uint16_t current_servo_pos; //
	uint8_t target_servo_pos;  //

	bool prog_pushed;      //for detecting when PROG button has been pressed and released
	bool prog_released;
	bool moving = false;    // true if servo is in process of moving between positions

	prog_pushed = false;
	prog_released = false;

	mode = 0;   //normal mode
	blink_rate = 0;
	blink_count = 0;
	limit_sel = 0;
	jiffy_clock = 0;
	LED_ON;   //turn on led

	ok_change = false;  //set at start of timer 20 mS timer phase to allow foreground task to chnage both bytes of servo_pos

	Init_PORT();

	if((PINB & (1 << BUTTONPROG)) == 0) // restore default limits if powered up with PROG button down
	{
		plus_limit = MAXPLUS;
		minus_limit = MAXMINUS;
		storeValues();
		while ((PINB & (1 << BUTTONPROG)) == 0);  //wait until PROG button released
	}
	else
	{
		if(loadValues())  //load data from eeprom
		{
			plus_limit = MAXPLUS;   //error
			minus_limit = MAXMINUS;
			mode = 3;
			blink_rate = MODE3RATE;      
		}
		if((plus_limit > MAXPLUS) || (plus_limit < MAXMINUS))  //if either value is out of range restore both and put in error mode 3
		{
			plus_limit = MAXPLUS;
			minus_limit = MAXMINUS;
			mode = 3;
			blink_rate = MODE3RATE;
		}
		if((minus_limit < MAXMINUS) || (minus_limit > MAXPLUS))
		{
			plus_limit = MAXPLUS;
			minus_limit = MAXMINUS;
			mode = 3;
			blink_rate = MODE3RATE;
		}
		
	}
	//-----------------------------------------------------------------------------------
	servo_enable = true;
	//servo_pos = minus_limit;
	servo_pos = POSMID;
	current_servo_pos = POSMID;
	target_servo_pos = servo_pos;
	startServoPulses();

	while(1)
	{
		if(ok_change) //wait until interrupt flags that the 19mS low period of the servopulse has started approx 50 Hz
		{
			ok_change = false;  //reset the flag
			jiffy_clock++;
			if(jiffy_clock > TIMEOUT)
			{
				mode = 4;
				blink_rate = MODE4RATE;
				blink_count = 0;
				servo_enable = false;
			}
			if((PINB & (1 << BUTTONPROG)) == 0) //is PROG button down
			{
				prog_pushed = true;  // flag that it's been down
			}
			else
			{
				if(prog_pushed == true)  // if button was pressed down and is now up
				{
					prog_released = true;  // flag that it has been released
					prog_pushed = false;  //reset the flag
					jiffy_clock = 0;  //reset it
					servo_enable = true;
				}
			}

			
			if(blink_rate)  // blink the led if blink_rate is not 0
			{
				blink_count++;
				if(blink_count == blink_rate)
				{
					//LED = !LED;
					LED_TOGGLE;
					blink_count = 0;
				}
			}
			else  // if blink_rate = 0
			{
				LED_ON;
			}
			
			//--------------------------------------------------------------------------
			
			if(prog_released == true)  // was the PROG button pressed and released
			{
				prog_released = false;
				
				switch(mode)
				{
				case 0:   // put in mode 1 to wait for PLUS or MINUS to be selected
					mode = 1;
					blink_rate = MODE1RATE;
					blink_count = 0;
					break;

				case 2:   //update the variable /store the data in eeprom / go back to mode 0
					if(limit_sel == 0)
					{
						plus_limit = servo_pos;
					}
					else
					{
						minus_limit = servo_pos;
					}

					stopServoPulses();
					storeValues();
					startServoPulses();
					
					mode = 0;
					blink_rate = MODE0RATE;
					blink_count = 0;
					servo_pos = POSMID;
					current_servo_pos = POSMID;
					target_servo_pos = POSMID;
					break;
					
				case 3:       //eeprom read out of range numbers and variables have been restored to defaults
					stopServoPulses();
					storeValues();
					startServoPulses();
					mode = 0;
					blink_rate = MODE0RATE;
					blink_count = 0;
					//LED_ON;
					break;
					
				case 4: //no servo drive mode
					mode = 0;
					blink_rate = MODE0RATE;
					blink_count = 0;
					servo_enable = true;
					//LED_ON;
					break;
					
				default:  //go back to mode 0
					mode = 0;
					blink_rate = MODE0RATE;
					blink_count = 0;
					//LED_ON;
				}
			}
			//------------------------------------------------------------------------------------------     
			// now look at PLUS MINUS buttons 
			switch (mode)
			{
			case 4: //sleep mode so just go back into mode 0 (normal run mode)
				{
					if(((PINB & (1 << BUTTONPLUS)) == 0) || ((PINB & (1 << BUTTONMINUS)) == 0))
					{
						jiffy_clock = 0; //reset on button press
						servo_enable = true;
						mode = 0;
						blink_rate = MODE0RATE;
						blink_count = 0;
					}
					//break;
				}
				
			case 2: //adjust the position
				{ 
					moving = false;
					if((PINB & (1 << BUTTONPLUS)) == 0)         //if PLUS button is pressed and its not moving
					{
						jiffy_clock = 0; //reset
						servo_enable = true;  //make sure output is enabled
						servo_pos++;
						if(servo_pos > MAXPLUS) // constrain to limits
						{
							servo_pos = MAXPLUS;
						}
					}
					else if((PINB & (1 << BUTTONMINUS)) == 0)
					{
						jiffy_clock = 0; //reset
						servo_enable = true;  //make sure output is enabled
						servo_pos--;
						if(servo_pos < MAXMINUS)// constrain
						{
							servo_pos = MAXMINUS;
						}
					}
					break;
				}

			case 1: //select which limit were adjusting and put in mode 2
				{
					moving = false;
					if((PINB & (1 << BUTTONPLUS)) == 0)       //if PLUS button is pressed
					{
						mode = 2;
						blink_rate = MODE2RATE;
						blink_count = 0;
						limit_sel = 0;
						servo_pos = plus_limit;
						//servo_pos = SELECTPLUS; // move to initial adjust position

					}
					else if((PINB & (1 << BUTTONMINUS)) == 0) //if MINUS button is pressed
					{
						mode = 2;
						blink_rate = MODE2RATE;
						blink_count = 0;
						limit_sel = 1;
						servo_pos = minus_limit;
						//servo_pos = SELECTMINUS; // move to initial adjust position
					}
					break;
				}
				

			case 0: // normal running mode
				{

					if((PINB & (1 << BUTTONPLUS)) == 0)       //if PLUS button is pressed
					{
						jiffy_clock = 0; //reset
						servo_enable = true;  //make sure output is enabled
						current_servo_pos = servo_pos;
						target_servo_pos = plus_limit;
						
					}
					else if((PINB & (1 << BUTTONMINUS)) == 0) //if MINUS button is pressed
					{
						jiffy_clock = 0; //reset
						servo_enable = true;  //make sure output is enabled
						current_servo_pos = servo_pos;
						target_servo_pos = minus_limit;
					}
					
					if(servo_pos != target_servo_pos)
					{
						moving = true;
						if(current_servo_pos < target_servo_pos)  // do we need to move servo in plus direction
						{
							//current_servo_pos++;
              if((target_servo_pos - current_servo_pos) >= servo_speed) // make sure we don't overflow or underflow
              {
							  current_servo_pos = current_servo_pos + servo_speed;
              }
              else
              {
                current_servo_pos = target_servo_pos;
              }
						}
           
						if(current_servo_pos > target_servo_pos)
						{
							//current_servo_pos--;
              if((current_servo_pos - target_servo_pos) >= servo_speed) // make sure we don't overflow or underflow
              {
                current_servo_pos = current_servo_pos - servo_speed;
              }
							else
              {
                current_servo_pos = target_servo_pos;
              }
						}
					}
					else
					{
						moving = false;  
					}
					servo_pos = current_servo_pos;
					

				}
			}
			

		}

	}


}
