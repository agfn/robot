#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#include <gpiod.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/*
"prevent nonstandard definitions from being exposed" 
or 
"expose nonstandard definitions that are not exposed by default"

via 
https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xsh_chap02.html#tag_22_02_02
*/
#define _XOPEN_SOURCE

// ***
// utils 
#define set_bit(x, idx, val) \
    (x) = ~(~(x) | (1 << (idx))) | ((val) << (idx));
#define get_bit(x, idx) \
    ((x) >> (idx) & 1)
// ***

// ***
// hardware control 

// gpio dev
char gpio_dev[] = "/dev/gpiochip0";

// need set for different platforms
#define SET_GPIO(pin, val) \
    gpiod_ctxless_set_value(gpio_dev, pin, val, NULL, NULL, NULL, NULL);

// set manually
#define M1_PIN_0    7       
#define M1_PIN_1    8
#define M2_PIN_0    23
#define M2_PIN_1    24

#define M_STOP      0
#define M_FORARD    1
#define M_BACK      2

#define SET_M(MX, val) \
    SET_GPIO(MX##_PIN_0, get_bit(val, 0)) \
    SET_GPIO(MX##_PIN_1, get_bit(val, 1)) 
// ***

// ***
// logic control
#define OP_DO       0xC0
#define DO_FORWARD  0xC0
#define DO_BACK     0xC1
#define DO_STOP     0xC2
#define DO_LEFT     0xC3
#define DO_RIGHT    0xC4
// ***

//// global vars
//
char dev[] = "/dev/ttyS0";
int serial = -1;
struct termios old_tio;

int forward_back = 0;

//// useful macros
// 
#define err_exit(condition, ...) \
    if (condition) \
    {              \
        printf(__VA_ARGS__); \
        exit(1);   \
    }

//// ctrl-c handler (stop program)
// 
void sec_exit(int num)
{
    if (serial >= 0)
    {
        printf("\n[CLOSE] %s\n", dev);
        tcsetattr(serial, TCSANOW, &old_tio);
        close(serial);
    }

    printf("[EXIT]\n");
    exit(1);
}

void register_ctrl_c()
{
    struct sigaction sa_ctrl_c;
    sa_ctrl_c.sa_sigaction = sec_exit;

    err_exit (sigaction(SIGINT, &sa_ctrl_c, NULL) < 0, "[SIG] error\n")
}


//// test
//
void test_serial()
{
    char buf[] = "AT";
    int n_rw = write(serial, buf, 2);
    printf("[TEST WRITE] %s (%d)\n", buf, n_rw);

    memset(buf, 0, 2);

    int n = 0;
    while (n < 2) {
        n_rw = read(serial, &buf[n], 2 - n);
        if (n_rw >= 0) n += n_rw;
    }
    printf("[TEST READ] %s (%d)\n", buf, n_rw);
}


////
//
// initlize serial
void init_serial()
{
    struct termios tios = {0};
    // used for restore
    tcgetattr(serial, &old_tio);

    tios.c_cflag = B9600 | CS8 | CREAD ; 
    
    tcsetattr(serial, TCSANOW, &tios);
}

////  recv and do
//
void set_fb(int fb)
{
    if (fb == DO_FORWARD)
    {
        printf("[FORWARD]\n");
        forward_back = M_FORARD;
        SET_M(M1, M_FORARD)
        SET_M(M2, M_FORARD)
    } else {
        printf("[BACK]\n");
        forward_back = M_BACK;
        SET_M(M1, M_BACK)
        SET_M(M2, M_BACK)
    }
}

void set_direction(int direc)
{
    if (direc == DO_LEFT) 
    {
        printf("[LEFT]\n");
        SET_M(M1, M_STOP)
        usleep(100000);
        SET_M(M1, forward_back)
    } else {
        printf("[RIGHT]\n");
        SET_M(M2, M_STOP)
        usleep(100000);
        SET_M(M2, forward_back)
    } 
}
// ***

// handle loop
void hanlde_recv_loop()
{
    unsigned char op = 0;
    int n_read = 0;
    while ( (n_read = read(serial, &op, 1)) >= 0)
    {
        if (n_read == 1 && (op & OP_DO) == OP_DO)
        {
            printf("[OP] %2X\n", op);
            switch (op)
            {
            case DO_STOP:
                {
                    SET_M(M1, M_STOP)
                    SET_M(M2, M_STOP)
                }
                break;
            case DO_LEFT:
            case DO_RIGHT:
                set_direction(op);
                break;
            case DO_FORWARD:
            case DO_BACK:
                set_fb(op);
                break;
            default:
                break;
            }
        } else {
            usleep(100000);
        }
    }
    
}


//// main
//
int main()
{
    // register ctrl-c
    register_ctrl_c();

    // open dev 
    serial = open(dev, O_RDWR);
    err_exit (serial < 0, "[OPEN] cannot open %s\n", dev)
    printf("[OPEN] %s\n", dev);

    // 
    init_serial();

    // test
    test_serial();

    // handle 
    hanlde_recv_loop();

    sleep(3);

    // exit
    sec_exit(0);

    return 0;
}

