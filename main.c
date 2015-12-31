/*
 * bherm001_FinalLab.c
 *
 * Created: 11/17/2015 1:52:00 PM
 * Author : Bailey
 */ 

#include <avr/io.h>
#include <util/delay.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdbool.h>

#include "io.c"
#include "timer.h"
#include "keypad.h"
#include "usart_ATmega1284.h"

static unsigned int motion = 0;
#define inc 39
static unsigned int increment = inc;
static unsigned int in_alarm = 0;
static unsigned int out_of_way = 0;
static unsigned int light = 0;
unsigned char going = 1;
#define F_CPU 8000000UL
#define N_BER 65000//66000
#define DIS_PORT PORTD //A3-4
#define DIS_PIN PIND
#define DIS_DDR DDRD

volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;

/*
volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;
*/

unsigned sensor = 0x00;
unsigned trig = 0x00;//C0, output
unsigned echo = 0x10;//A3, input
volatile long avg = 0;
volatile long user_set_time = 0;
volatile uint32_t running = 0;
volatile unsigned up = 0;
volatile uint32_t timercounter = 0;
volatile uint16_t start_time;
volatile uint16_t end_time;
volatile unsigned char set;
uint16_t duration;
int turn = 0;
int error = 1;
unsigned char change_state = 0;
unsigned char menu_item = 0;
unsigned char user_distance1;
unsigned char user_distance2;
unsigned char user_distance3;
int user_distance = 40;
int no_obstacle = 0;
int go_no_obstacle = 0;
int waiting = 0;
unsigned char message = 1;
unsigned char temp_message = 1;

void TimerSet(int ms) {
	TCNT3 = 0;
	OCR3A = ms*31.25;
}

void TimerOn() {
	TCCR3B = (1<<WGM12)|(1<<CS12); //Clear timer on compare. Prescaler = 256
	TIMSK3 = (1<<OCIE3A); //Enables compare match interrupt
	SREG |= 0x80; //Enable global interrupts
}

typedef struct task {
	int state;
	unsigned long period;
	unsigned long elapsedTime;
	int (*TickFct)(int);
} task;

task tasks[5];

const unsigned char tasksNum = 5;
const unsigned long tasksPeriodGCD = 100;

ISR(TIMER3_COMPA_vect) {
	unsigned char i;
	for (i = 0; i < tasksNum; ++i) { // Heart of the scheduler code
		if ( tasks[i].elapsedTime >= tasks[i].period ) { // Ready
			tasks[i].state = tasks[i].TickFct(tasks[i].state);
			tasks[i].elapsedTime = 0;
		}
		tasks[i].elapsedTime += tasksPeriodGCD;
	}
}

void transmit_data(unsigned char data, unsigned char shifter) {

	int i;

	for (i = 7; i >= 0 ; --i) {

		if(shifter == 'C')
		{
			// Sets SRCLR to 1 allowing data to be set

			// Also clears SRCLK in preparation of sending data
			PORTC = 0x08;

			// set SER = next bit of data to be sent.

			PORTC |= ((data >> i) & 0x01);

			// set SRCLK = 1. Rising edge shifts next bit of data into the shift register

			PORTC |= 0x04;
		}
		else
		{
			// Sets SRCLR to 1 allowing data to be set

			// Also clears SRCLK in preparation of sending data
			PORTC = 0x08;
			PORTD = 0x08;

			// set SER = next bit of data to be sent.

			PORTC |= ((data >> i) & 0x01);

			// set SRCLK = 1. Rising edge shifts next bit of data into the shift register

			PORTC |= 0x04;
			
		}
	}


	// set RCLK = 1. Rising edge copies data from the “Shift” register to the

	//“Storage” register

	if(shifter == 'C')
	{
		PORTC |= 0x02;

		// clears all lines in preparation of a new transmission

		PORTC = 0x00;
	}
	else
	{
		PORTD |= 0x02;
		PORTC |= 0x02;
		// clears all lines in preparation of a new transmission

		PORTD = 0x00;
		PORTC = 0x00;
	}

}

uint16_t pulse()
{
	//Use trig to get the signal duration
	uint32_t i = 0;//uint32 initially
	going = 1;
	
	//while(i < N_BER)// && going)//wait for high
	for(i = 0; i < N_BER; i++)
	{
		if(GetBit(PIND, 0))//(PINA & (1 << echo)))
			break;
		else
			continue;
		i++;
	}
	
	
	if(i == N_BER)
	{
		return 0xFFFD;//Ends Here Somehow (change to 0xFFFF when fixed)
	}
	//Start Timer
	//TimerOn();
	callTimer();
	
	//wait for falling edge
	i = 0;
	//going = 1;
	//while(i < N_BER)// && going)//wait for low
	for(i = 0; i < N_BER; i++)
	{
		if(GetBit(PIND, 0))//(PINA & (1 << echo)))//(!GetBit(PINA, 4))//
		{
			if(TCNT1 > N_BER)
				break;
			else
				continue;
		}
		else
			break;
		i++;
	}
	
	
	if(i == N_BER)
	{
		return 0xFFFF;
	}
	
	//falling edge found
	
	//Stop Timer
	uint32_t amount = TCNT1;
	TCCR1B 	= 0x00;
	if(amount > N_BER)
		return 0xFFFE;//No obstacle (amount >> 1);//
	else
		return (amount >> 1);

	
}
void callTimer()
{
	TimerOn_dist();
	TCCR1A = 0x00;
}
void setup()
{
	//PORTA &= ~(1 << trig);
	PORTD |= 1<<0X01;//set trig high
	_delay_us(10);
	PORTD &= ~(1<<0x01);//set trig low
	_delay_ms(5);

}

enum main_states {Init_Main, Menu_Main, Distance_Main} main_state;
int main_sm(main_state)
{
	switch(main_state)
	{
		case Init_Main:
			main_state = Menu_Main;
			break;
		case Menu_Main:
			if(GetKeypadKey() == '#')
			{
				main_state = Distance_Main;
			}
			else
			{
				main_state = Menu_Main;
			}
			break;
		case Distance_Main:
			if(GetKeypadKey() == '*')
			{
				main_state = Menu_Main;
			}
			else
			{
				main_state = Distance_Main;
			}
			break;
		default:
			main_state = Init_Main;
			break;
	}
	switch(main_state)
	{
		case Init_Main:
			break;
		case Menu_Main:
			change_state = 1;
			//LCD_ClearScreen();
			//LCD_DisplayString(1, "Menu");
			break;
		case Distance_Main:
			change_state = 2;
			//LCD_ClearScreen();
			//LCD_DisplayString(1, "Distance");
			break;
		default:
			break;
	}
	 return main_state;
}

enum menu_states {Init_Menu, Wait_Menu, ItemA_Menu, ItemB_Menu} menu_state;
int menu_display(menu_state)
{
	switch(menu_state)
	{
		case Init_Menu:
			menu_state = Wait_Menu;
			break;
		case Wait_Menu:
			if(change_state == 1)
			{
				menu_state = ItemA_Menu;
				menu_item = 1;
				LCD_ClearScreen();
				LCD_DisplayString(1, "Set Distance:");
			}
			else
			{
				menu_state = Wait_Menu;
			}
			break;
		case ItemA_Menu:
			if(change_state != 1)
			{
				menu_state = Wait_Menu;
			}
			else if(menu_item == 1)
			{
				menu_state = ItemA_Menu;
			}
			else if(menu_item == 2)
			{
				menu_state = ItemB_Menu;
				LCD_ClearScreen();
				LCD_DisplayString(1, "Final Words:");
			}
			else
			{
				//error
				menu_state = Wait_Menu;
			}
			break;
		case ItemB_Menu:
			if(change_state != 1)
			{
				menu_state = Wait_Menu;
			}
			else if(menu_item == 2)
			{
				menu_state = ItemB_Menu;
			}
			else if(menu_item == 1)
			{
				menu_state = ItemA_Menu;
				LCD_ClearScreen();
				LCD_DisplayString(1, "Set Distance:");
			}
			else
			{
				//error
				menu_state = Wait_Menu;
			}
			break;
		default:
			break;
	}
	switch(menu_state)
	{
		case Init_Menu:
			break;
		case Wait_Menu:
			//LCD_ClearScreen();
			//LCD_DisplayString(1, "Wait");
			break;
		case ItemA_Menu:
			/*
			if(GetKeypadKey() == 'A')
			{
				menu_item = 1;
			}
			else if(GetKeypadKey() == 'B')
			{
				menu_item = 2;
			}*/
			break;
		case ItemB_Menu:
			/*
			if(GetKeypadKey() == 'A')
			{
				menu_item = 1;
			}
			else if(GetKeypadKey() == 'B')
			{
				menu_item = 2;
			}*/
			break;
		default:
			break;
	}
	return menu_state;
}

enum menu_ItemA_states {Init_Menu_ItemA, Wait_Menu_ItemA, Get1_ItemA, Get2_ItemA, Get3_ItemA, Process_ItemA} menu_ItemA_state;
int menu_ItemA_display(menu_ItemA_state)
{
	switch(menu_ItemA_state)
	{
		case Init_Menu_ItemA:
			menu_ItemA_state = Wait_Menu_ItemA;
			break;
		case Wait_Menu_ItemA:
			if(change_state != 1 || menu_item != 1)
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else
			{
				menu_ItemA_state = Get1_ItemA;
				
				if(user_distance >= 100)
				{
					user_distance3 = user_distance % 10;
					user_distance2 = (user_distance / 10) % 10;
					user_distance1 = (user_distance / 100);
				}
				else if(user_distance >= 10)
				{
					user_distance3 = user_distance % 10;
					user_distance2 = (user_distance / 10);
					user_distance1 = 0;
				}
				else
				{
					user_distance3 = user_distance;
					user_distance2 = 0;
					user_distance1 = 0;
				}
				user_distance3 += '0';
				user_distance2 += '0';
				user_distance1 += '0';
				LCD_Cursor(14);
				LCD_WriteData(user_distance1);
				LCD_Cursor(15);
				LCD_WriteData(user_distance2);
				LCD_Cursor(16);
				LCD_WriteData(user_distance3);
				LCD_Cursor(14);
			}
			break;
		case Get1_ItemA:
			if(change_state != 1 || menu_item != 1)
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else if(GetKeypadKey() == '\0' || GetKeypadKey() == '*' || GetKeypadKey() == 'A' || GetKeypadKey() == 'D')
			{
				menu_ItemA_state = Get1_ItemA;
			}
			else if(GetKeypadKey() == 'B')
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else if(GetKeypadKey() == 'C')//Confirm
			{
				menu_ItemA_state = Process_ItemA;
			}
			else
			{
				menu_ItemA_state = Get2_ItemA;
				user_distance1 = GetKeypadKey();
				LCD_Cursor(14);
				LCD_WriteData(user_distance1);
			}
			break;
		case Get2_ItemA:
			if(change_state != 1 || menu_item != 1)
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else if(GetKeypadKey() == '\0' || GetKeypadKey() == '*' || GetKeypadKey() == 'A' || GetKeypadKey() == 'D')
			{
				menu_ItemA_state = Get2_ItemA;
			}
			else if(GetKeypadKey() == 'B')
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else if(GetKeypadKey() == 'C')//Confirm
			{
				menu_ItemA_state = Process_ItemA;
			}
			else
			{
				menu_ItemA_state = Get3_ItemA;
				user_distance2 = GetKeypadKey();
				LCD_Cursor(15);
				LCD_WriteData(user_distance2);
			}
			break;
		case Get3_ItemA:
			if(change_state != 1 || menu_item != 1)
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else if(GetKeypadKey() == '\0' || GetKeypadKey() == '*' || GetKeypadKey() == 'A' || GetKeypadKey() == 'B' || GetKeypadKey() == 'D')
			{
				menu_ItemA_state = Get3_ItemA;
			}
			else if(GetKeypadKey() == 'B')
			{
				menu_ItemA_state = Wait_Menu_ItemA;
			}
			else if(GetKeypadKey() == 'C')//Confirm
			{
				menu_ItemA_state = Process_ItemA;
			}
			else
			{
				//Stay in this state and ignore other numbers pressed.
				menu_ItemA_state = Get3_ItemA;
				user_distance3 = GetKeypadKey();
				LCD_Cursor(16);
				LCD_WriteData(user_distance3);
			}
			break;
		case Process_ItemA:
			menu_ItemA_state = Wait_Menu_ItemA;
			break;
		default:
			menu_ItemA_state = Wait_Menu_ItemA;
			break;
	}
	switch(menu_ItemA_state)
	{
		case Init_Menu_ItemA:
			break;
		case Wait_Menu_ItemA:
			break;
		case Get1_ItemA:
			break;
		case Get2_ItemA:
			break;
		case Get3_ItemA:
			break;
		case Process_ItemA:
			user_distance = (user_distance1 - 48) * 100;
			user_distance += (user_distance2 - 48) * 10;
			user_distance += (user_distance3 - 48);
			menu_item = 2;
			break;
		default:
			break;
	}
	return menu_ItemA_state;
}

//Phone Message
enum menu_ItemB_states {Init_Menu_ItemB, Wait_Menu_ItemB, Msg1_ItemB, Msg2_ItemB, Msg3_ItemB, Msg4_ItemB, Msg5_ItemB, Process_ItemB} menu_ItemB_state;
int menu_ItemB_display(menu_ItemB_state)
{
	switch(menu_ItemB_state)
	{
		case Init_Menu_ItemB:
		menu_ItemB_state = Wait_Menu_ItemB;
		break;
		case Wait_Menu_ItemB:
		if(change_state != 1 || menu_item != 2)
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(message == 1)
		{
			menu_ItemB_state = Msg1_ItemB;
		}
		else if(message == 2)
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		else if(message == 3)
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		else if(message == 4)
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		else if(message == 5)
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		break;
		case Msg1_ItemB:
		if(change_state != 1 || menu_item != 2)
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() != '#' && GetKeypadKey() != 'C' && GetKeypadKey() != '2' && GetKeypadKey() != '3' && GetKeypadKey() != '4' && GetKeypadKey() != '5')
		{
			menu_ItemB_state = Msg1_ItemB;
			//LCD_DisplayString(1, "Set Message:");
			//LCD_DisplayStringNoClear(17, "Message 1");
		}
		else if(GetKeypadKey() == '#')
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() == 'C')//Confirm
		{
			menu_ItemB_state = Process_ItemB;
		}
		else if(GetKeypadKey() == '2')
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		else if(GetKeypadKey() == '3')
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		else if(GetKeypadKey() == '4')
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		else if(GetKeypadKey() == '5')
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		else
		{
			menu_ItemB_state = Msg1_ItemB;
		}
		break;
		case Msg2_ItemB:
		if(change_state != 1 || menu_item != 2)
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() != '#' && GetKeypadKey() != 'C' && GetKeypadKey() != '1' && GetKeypadKey() != '3' && GetKeypadKey() != '4' && GetKeypadKey() != '5')
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		else if(GetKeypadKey() == '#')
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() == 'C')//Confirm
		{
			menu_ItemB_state = Process_ItemB;
		}
		else if(GetKeypadKey() == '1')
		{
			menu_ItemB_state = Msg1_ItemB;
		}
		else if(GetKeypadKey() == '3')
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		else if(GetKeypadKey() == '4')
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		else if(GetKeypadKey() == '5')
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		else
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		break;
		case Msg3_ItemB:
		if(change_state != 1 || menu_item != 2)
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() != '#' && GetKeypadKey() != 'C' && GetKeypadKey() != '1' && GetKeypadKey() != '2' && GetKeypadKey() != '4' && GetKeypadKey() != '5')
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		else if(GetKeypadKey() == '#')
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() == 'C')//Confirm
		{
			menu_ItemB_state = Process_ItemB;
		}
		else if(GetKeypadKey() == '1')
		{
			menu_ItemB_state = Msg1_ItemB;
		}
		else if(GetKeypadKey() == '2')
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		else if(GetKeypadKey() == '4')
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		else if(GetKeypadKey() == '5')
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		else
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		break;
		case Msg4_ItemB:
		if(change_state != 1 || menu_item != 2)
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() != '#' && GetKeypadKey() != 'C' && GetKeypadKey() != '1' && GetKeypadKey() != '2' && GetKeypadKey() != '3' && GetKeypadKey() != '5')
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		else if(GetKeypadKey() == '#')
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() == 'C')//Confirm
		{
			menu_ItemB_state = Process_ItemB;
		}
		else if(GetKeypadKey() == '1')
		{
			menu_ItemB_state = Msg1_ItemB;
		}
		else if(GetKeypadKey() == '2')
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		else if(GetKeypadKey() == '3')
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		else if(GetKeypadKey() == '5')
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		else
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		break;
		case Msg5_ItemB:
		if(change_state != 1 || menu_item != 2)
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() != '#' && GetKeypadKey() != 'C' && GetKeypadKey() != '1' && GetKeypadKey() != '2' && GetKeypadKey() != '3' && GetKeypadKey() != '4')
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		else if(GetKeypadKey() == '#')
		{
			menu_ItemB_state = Wait_Menu_ItemB;
		}
		else if(GetKeypadKey() == 'C')//Confirm
		{
			menu_ItemB_state = Process_ItemB;
		}
		else if(GetKeypadKey() == '1')
		{
			menu_ItemB_state = Msg1_ItemB;
		}
		else if(GetKeypadKey() == '2')
		{
			menu_ItemB_state = Msg2_ItemB;
		}
		else if(GetKeypadKey() == '3')
		{
			menu_ItemB_state = Msg3_ItemB;
		}
		else if(GetKeypadKey() == '4')
		{
			menu_ItemB_state = Msg4_ItemB;
		}
		else
		{
			menu_ItemB_state = Msg5_ItemB;
		}
		break;
		case Process_ItemB:
			menu_ItemB_state = Wait_Menu_ItemB;
		break;
		default:
		menu_ItemB_state = Wait_Menu_ItemB;
		break;
	}
	switch(menu_ItemB_state)
	{
		case Init_Menu_ItemB:
			break;
		case Wait_Menu_ItemB:
			break;
		case Msg1_ItemB:
			LCD_DisplayStringNoClear(17, "Send Help       ");//Message 1
			temp_message = 1;
			break;
		case Msg2_ItemB:
			LCD_DisplayStringNoClear(17, "Call Police     ");//Message 2
			temp_message = 2;
			break;
		case Msg3_ItemB:
			LCD_DisplayStringNoClear(17, "I am not okay   ");//Message 3
			temp_message = 3;
			break;
		case Msg4_ItemB:
			LCD_DisplayStringNoClear(17, "Call My Mom     ");//Message 4
			temp_message = 4;
			break;
		case Msg5_ItemB:
			LCD_DisplayStringNoClear(17, "Is My Phone Okay");//Message 5
			temp_message = 5;
			break;
		case Process_ItemB:
			menu_ItemB_state = Wait_Menu_ItemB;
			message = temp_message;
			menu_item = 1;
			break;
		default:
			break;
	}
	return menu_ItemB_state;
}

enum dist_states {Init_Dist, Setup_Dist, Pulse_Dist /*, Send_Dist, Convert_Dist, Display_Dist*/} dist_state;
int dist_sensor(dist_state)
{
	uint16_t dist;
	int convert;
	int digit1;
	int digit2;
	int digit3;
	int digit4;
	int convert1 = 0;
	int no_obstacle = 0;
	int go_no_obstacle = 0;
	int waiting = 0;
	char send;
	switch(dist_state)
	{
		case Init_Dist:
			if(change_state == 2)
			{
				dist_state = Setup_Dist;
				LCD_ClearScreen();
			}
			else
				dist_state = Init_Dist;
			break;
		case Setup_Dist:
			if(change_state == 2)
			{
				dist_state = Pulse_Dist;
				//LCD_ClearScreen();
			}
			else
				dist_state = Init_Dist;
			break;
		case Pulse_Dist:
			if(change_state == 2)
			{
				dist_state = Setup_Dist;
				//LCD_ClearScreen();
			}
			else
				dist_state = Init_Dist;
			break;
		/*
		case Convert_Dist:
			dist_state = Display_Dist;
			break;
		case Display_Dist:
			dist_state = Setup_Dist;
			break;
		*/
		default:
			dist_state = Setup_Dist;
			break;
	}
	
	switch(dist_state)
	{
		case Init_Dist:
			PORTB = SetBit(PORTB, 2, 0);
			break;
		case Setup_Dist:
			_delay_ms(100);
			setup();
			//break;
		case Pulse_Dist:
			dist = pulse();
			if(!GetBit(PINB, 4))
			{
				PORTB = SetBit(PORTB, 2, 1);
			}
			if(dist == 0xFFFF)//Error
			{
				convert1 = 0;
				LCD_DisplayString(1, "Error");
				//Wait();
				go_no_obstacle = 0;
			}
			else if((dist == 0xFFFE || dist >= 0x21FC) && go_no_obstacle >= 6)//No Obstacle
			{
				convert1 = 0;
				//go_no_obstacle++;
				LCD_DisplayString(1, "No Obstacle");
				no_obstacle = 1;
				//Wait();
			}
			else if((dist == 0xFFFE || dist >= 0x21FC) && go_no_obstacle < 6)//8700 = 150in
			{
				//LCD_DisplayString(1, "Error2");
				convert1 = 0;
				go_no_obstacle++;
			}
			else if(dist == 0xFFFD)
			{
				convert1 = 0;
				LCD_DisplayString(1, "Error1");
				//Wait();
			}
			else
			{
				go_no_obstacle = 0;
				if(no_obstacle == 1)
				{
					LCD_ClearScreen();
					no_obstacle = 0;
				}
				error = 0;
				no_obstacle = 0;
			}
			//break;
		//case Convert_Dist:
			convert = (dist/58.0);
			
			convert1 = 1;
			if(convert >= 1000 || convert <= 1)
			{
				/*
				digit4 = convert % 10;
				digit3 = (convert / 10) % 10;
				digit2 = (convert / 100) % 10;
				digit1 = (convert / 1000);
				*/
				break;
			}
			else if(convert >= 100)
			{
				if(convert <= 200){
					send = convert + '0';
					if(USART_IsSendReady(1))
					{
						//for(int i = 0; i < )
						USART_Send(send, 1);
						USART_Flush(1);
						PORTB = SetBit(PORTB, 2, 1);
					}
				}
				if(convert <= user_distance)// && waiting >= 1)
				{
					PORTB = SetBit(PORTB, 3, 1);
				}
				else
				{
					PORTB = SetBit(PORTB, 3, 0);
					waiting = 0;
				}
				digit4 = convert % 10;
				digit3 = (convert / 10) % 10;
				digit2 = (convert / 100);
				digit1 = 0;
			}
			else if(convert >= 10)
			{
				if(convert <= 200){
					send = convert + '0';
					if(USART_IsSendReady(1))
					{
						//for(int i = 0; i < )
						USART_Send(send, 1);
						USART_Flush(1);
						PORTB = SetBit(PORTB, 2, 1);
					}
				}
				if(convert <= user_distance)// && waiting >= 1)
				{
					PORTB = SetBit(PORTB, 3, 1);
				}
				else
				{
					PORTB = SetBit(PORTB, 3, 0);
					waiting = 0;
				}
				digit4 = convert % 10;
				digit3 = (convert / 10);
				digit2 = 0;
				digit1 = 0;
			}
			else
			{
				if(convert <= 200){
					send = convert + '0';
					if(USART_IsSendReady(1))
					{
						//for(int i = 0; i < )
						USART_Send(send, 1);
						USART_Flush(1);
						PORTB = SetBit(PORTB, 2, 1);
					}
				}
				if(convert <= user_distance)// && waiting >= 0)
				{
					PORTB = SetBit(PORTB, 3, 1);
				}
				else
				{
					PORTB = SetBit(PORTB, 3, 0);
					waiting = 0;
				}
				digit4 = convert;
				digit3 = 0;
				digit2 = 0;
				digit1 = 0;
			}
			//break;
		//case Display_Dist:
			LCD_Cursor(1);
			LCD_WriteData('0' + digit1);
			LCD_Cursor(2);
			LCD_WriteData('0' + digit2);
			LCD_Cursor(3);
			LCD_WriteData('0' + digit3);
			LCD_Cursor(4);
			LCD_WriteData('0' + digit4);
			LCD_Cursor(5);
			LCD_WriteData('i');
			LCD_Cursor(6);
			LCD_WriteData('n');
			LCD_Cursor(0);
			_delay_ms(100);
			break;
		default:
			break;
	}
	
	return dist_state;
}

int main() {
	DDRA = 0xFF; PORTA = 0x00; //output lcd screen (temp)
	DDRB = 0x0F; PORTB = 0xF0; //output lcd screen (temp 0-1);// output lcds (temp 2-3); input button (temp 4); wireless transceiver (3-7)
	DDRC = 0xF0; PORTC = 0x0F; //input/output keypad
	DDRD = 0x0A; PORTD = 0xF5; //output distance sensor (trig 1) input (0; echo 0); output bluetooth (RX 3), input (TX 2, EN 4)
	
	unsigned char i = 0;
	
	tasks[i].state = Init_Main;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &main_sm;
	
	++i;
	tasks[i].state = Init_Menu;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &menu_display;
	
	++i;
	tasks[i].state = Init_Menu_ItemA;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &menu_ItemA_display;
	
	++i;
	tasks[i].state = Init_Menu_ItemB;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &menu_ItemB_display;
	
	//NEED HELP CONVERTING the scheduler's TIMER1 TO TIMER3
	//Change tasksNum = 3;
	
	++i;
	tasks[i].state = Init_Dist;
	tasks[i].period = 100;
	tasks[i].elapsedTime = tasks[i].period;
	tasks[i].TickFct = &dist_sensor;
	TimerSet_dist(100);
	
	TimerSet(100);
	TimerOn();
	sei();
	initUSART(1);
	
	LCD_init();
	while(1) {
		
		while(!TimerFlag){};
		TimerFlag = 0;
		
	}
}
