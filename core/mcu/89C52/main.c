#include "config.h"
#include "protocol.h"


/* ************************
	global control var
   ************************ */
#define BT_MODE		1
#define AUTO_MODE	0
uchar mode = BT_MODE;



/* ************************
	PWM control motor speed
   ************************ */
uchar left_motor_speed = 0;
uchar right_motor_speed = 0;
uchar PWM_T;

void global_init()
{
	ET0 = 1; // enable timer interrupt
	EA = 1;  // 
}

void pwm_timer_0_init()
{
	TMOD |= 0x02; 	// 8bit auto reload mode
	TH0 = 0xA4;
	TL0 = 0xA4; 	// 100 us
	TR0 = 1;		// enable timer 0
}

/* timer 0 interrupt routine */
void timer_0_isr() interrupt 1 using 1
{
	if (PWM_T == 255)
	{
		PWM_T = 0;
	}
	if (right_motor_speed >= PWM_T)
	{
		right_motor_ea = 1;
	}
	if (left_motor_speed >= PWM_T)
	{
		left_motor_ea = 1;
	}
	if (left_motor_speed < PWM_T)
	{
		left_motor_ea = 0;
	}
	if (right_motor_speed < PWM_T)
	{
		right_motor_ea = 0;
	}
	PWM_T++;
}

/* ******************
	delay 
   ****************** */
void delay(int ms)
{
	unsigned char i, j;
	while (ms-- != 0)
	{
		_nop_();
		_nop_();
		_nop_();
		i = 11;
		j = 190;
		do
		{
			while (--j);
		} while (--i);
	}
}


/* ******************
	car control
   ****************** */
void car_go(uchar left_speed, uchar right_speed)
{
	left_motor_speed = left_speed;
	right_motor_speed = right_speed;
	left_motor_go;
	right_motor_go;
}

void car_back(uchar left_speed, uchar right_speed)
{
	left_motor_speed = left_speed;
	right_motor_speed = right_speed;
	left_motor_back;
	right_motor_back;
}

void car_stop()
{
	left_motor_stop;
	right_motor_stop;
}

void turn_left(uchar left_speed, uchar right_speed)
{
	left_motor_speed = left_speed;
	right_motor_speed = right_speed;	
	left_motor_stop;	
	right_motor_go;
}

void turn_right(uchar left_speed, uchar right_speed)
{
	left_motor_speed = left_speed;
	right_motor_speed = right_speed;	
	left_motor_go;
	right_motor_stop;
}


/* *****************
	bluetooth remote control
   ***************** */
void serial_timer_1_init()
{
	/* enable timer 1 as baud */ 
	TMOD |= 0x20; 	// 8bit auto reload mode
	TH1 = -3;
	TL1 = -3; 		// 9600 baud
	TR1 = 1;		// enable timer 1

	/* enable serial */
	SCON = 0x52; 
}

void bt_send(uchar byte)
{
	SBUF = byte;
	while (!TI) ;
	TI = 0;
}

uchar bt_recv()
{
	uchar byte = 0;
	while (!RI) ;
	byte = SBUF;
	RI = 0;
	return byte;
}

uchar user_set_left_speed = 100;
uchar user_set_right_speed = 100;

/* *****************
	line tracking 
   ***************** */
void tracking()
{
	if ((left_sensor == 1) && (right_sensor == 1))
	{	
		car_go(user_set_left_speed, user_set_right_speed);
	} else if ((left_sensor == 0) && (right_sensor == 1))
	{
		turn_right(255, user_set_right_speed);
	} else if ((left_sensor == 1) && (right_sensor == 0))
	{
		turn_left(user_set_left_speed, 255);
	}
}


void main()
{
	global_init();
	pwm_timer_0_init();
	serial_timer_1_init();

	// set speed 
	user_set_left_speed = 100;
	user_set_left_speed = 100;

	P1 = 0;
	
	/* application  loop */ 
	while (1)
	{
		uchar op = 0;
		// recieve opcode
		if (RI) op = bt_recv();
	
		if (op == MODE_SWITCH)
		{
			mode = !mode;
			if (mode == BT_MODE)
				car_stop();
		} 

		if (mode == AUTO_MODE)
			tracking();
	
		if (op != 0)
		{
			switch (op)
			{
			case DO_FORWARD:
				car_go(left_motor_speed, right_motor_speed);
				break;
			case DO_BACK:
				car_back(left_motor_speed, right_motor_speed);
				break;
			case DO_STOP:
				car_stop();
				break;
			case DO_LEFT:
				turn_left(left_motor_speed, 255);
				break;
			case DO_RIGHT:
				turn_right(255, right_motor_speed);
				break;
			default:
				break;
			}
			delay(20);
			car_stop();
		}
	}
}