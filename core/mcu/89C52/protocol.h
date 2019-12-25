#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

////
//
// logic control
#define OP_DO       0xC0
#define DO_FORWARD  0xC0
#define DO_BACK     0xC1
#define DO_STOP     0xC2
#define DO_LEFT     0xC3
#define DO_RIGHT    0xC4
// 

////
//
// work mode
#define MODE_AUTO_ON   0xD0
#define MODE_AUTO_OFF  0xD1

// redefine in 8952
#define MODE_SWITCH     MODE_AUTO_ON

#endif