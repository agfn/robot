#ifndef _CONFIG_H
#define _CONFIG_H

#include <reg52.h>
#include <intrins.h>


typedef unsigned char uchar;
typedef unsigned int  uint; 
typedef unsigned long uint32; 

sbit m1 = P1^2; 
sbit m2 = P1^3;
sbit m3 = P1^4;
sbit m4 = P1^5; 
sbit left_motor_ea  = P1^0; 
sbit right_motor_ea = P1^1; 

sbit left_sensor	=	P2^1;
sbit right_sensor	=	P2^0;



#define left_motor_stop	 	m1 = 0, m2 = 0 
#define right_motor_stop	m3 = 0, m4 = 0 
#define left_motor_go		m1 = 1, m2 = 0 
#define left_motor_back		m1 = 0, m2 = 1 
#define right_motor_go		m3 = 1, m4 = 0 
#define right_motor_back	m3 = 0, m4 = 1 

#endif