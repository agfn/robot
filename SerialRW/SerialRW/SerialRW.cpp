// SerialRW.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <Windows.h>

#define err_code(condition, ...) \
	if (condition) \
	{ printf(__VA_ARGS__, GetLastError()); exit(1);}

// used for read
// must set 0 after use
#define N_RBUF 0x100
char rbuf[N_RBUF] = { 0 };

// used for write
// set 0 after use
#define N_WBUF 0x100
char wbuf[N_WBUF] = { 0 };

// device to open
char dev[] = "\\\\.\\COM3";
HANDLE serial = NULL;

// write console event
HANDLE w_consol_event = NULL;


void CommLoop(HANDLE serial)
{
	DWORD len = 0, n_rw = 0;

	while (1)
	{
		WaitForSingleObject(w_consol_event, INFINITE);
		printf("> ");
		std::cin >> wbuf;
		len = strnlen(wbuf, 0x100);

		err_code(\
			WriteFile(serial, wbuf, len, &n_rw, NULL) == FALSE, \
			"[WRITE] ERROR %d\n");
		printf("[WRITE] %s (%d)\n", wbuf, n_rw);
		memset(wbuf, 0, N_WBUF);

		err_code(\
			ReadFile(serial, rbuf, N_RBUF, &n_rw, NULL) == FALSE, \
			"[READ] ERROR %d\n");
		printf("[READ] %s (%d)\n", rbuf, n_rw);
		memset(rbuf, 0, N_RBUF);
		SetEvent(w_consol_event);
	}
}

//  stop program
BOOL WINAPI CtrlCHandler(DWORD signal) {

	if (signal == CTRL_C_EVENT)
	{
		WaitForSingleObject(w_consol_event, 1000);
		printf("[CLOSE] %s\n", dev);
		CloseHandle(serial);
		CloseHandle(w_consol_event);
		printf("[EXIT]\n");
		exit(0);
	}
	return TRUE;
}


int main()
{
	serial = CreateFileA(dev, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	err_code (serial == INVALID_HANDLE_VALUE, "[OPEN] fail to open %s %d\n", dev);

	// register ctrl-c handler
	err_code(\
		SetConsoleCtrlHandler(CtrlCHandler, TRUE) == FALSE, \
		"[CTRL-C] register fail %d\n");

	// create write console event
	w_consol_event = CreateEventA(NULL, FALSE, FALSE, "WRITE_CONSOLE_EVENT");
	err_code(w_consol_event == INVALID_HANDLE_VALUE, "[CREATE EVENT] fail %d\n");
	SetEvent(w_consol_event);


	// 
	COMMTIMEOUTS comm_t_o = {-1, 0, 0x12c, 0, 0x12c};

	SetCommTimeouts(serial, &comm_t_o);

	SetCommMask(serial, EV_RXCHAR | EV_RXFLAG);


	// set buffer size
	// SetupComm(serial, 0x400, 0x400);

	// set communicate status
	DCB dcb;
	GetCommState(serial, &dcb);

	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate = CBR_9600;	// HC-02 default baud
	dcb.ByteSize = 8;			// 8 bit / byte
	dcb.fParity = FALSE;		
	dcb.Parity = NOPARITY;		// no  parity
	dcb.StopBits = ONESTOPBIT;	// 1 bit 

	SetCommState(serial, &dcb);

	// clear buf 
	PurgeComm(serial, PURGE_TXCLEAR | PURGE_RXCLEAR);

	// clear err
	ClearCommError(serial, NULL, NULL);

	// communication
	CommLoop(serial);

	CloseHandle(w_consol_event);
	CloseHandle(serial);
}
