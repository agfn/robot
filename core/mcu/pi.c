#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include <gpiod.h>
#include "protocol.h"


/*
"prevent nonstandard definitions from being exposed" 
or 
"expose nonstandard definitions that are not exposed by default"

via 
https://pubs.opengroup.org/onlinepubs/9699919799/xrat/V4_xsh_chap02.html#tag_22_02_02
*/
#define _XOPEN_SOURCE

////
//
// utils 
#define set_bit(x, idx, val) \
    (x) = ~(~(x) | (1 << (idx))) | ((val) << (idx));
#define get_bit(x, idx) \
    ((x) >> (idx) & 1)

#define err_exit(condition, ...) \
    if (condition) \
    {              \
        printf(__VA_ARGS__); \
        exit(1);   \
    }
//



////
//
// work mode flags
int auto_mode = false;
//



////
//
// hardware control 

// gpio global vars
char gpio_dev[] = "/dev/gpiochip0";
struct gpiod_chip * gchip = NULL;
struct gpiod_line * glines[7] = {0};


// uesd for monitor control 
#define M1_PIN_0    13       
#define M1_PIN_1    19
#define M2_PIN_0    6
#define M2_PIN_1    5

// monitor control code
#define M_STOP      0
#define M_FORWARD   2
#define M_BACK      1

// used for auto control 
#define WL_LEFT_PIN     20
#define WL_RIGHT_PIN    16
#define WL_FRONT_PIN    21

#define WL_FRONT        1
#define WL_LEFT         2
#define WL_RIGHT        4

// turn left(right) time 
#define BTRC_MODE_TURN_TIME     70000
#define AUTO_MODE_TURN_TIME     5000


// initialize gpio input and output
void init_gpio()
{
    int err = 0;

    gchip = gpiod_chip_open(gpio_dev);
    assert (gchip != NULL);
    printf("[GCHIP]\n");

    // open monitor control line
    glines[0] = gpiod_chip_get_line(gchip, M1_PIN_0);
    assert (glines[0] != NULL);
    glines[1] = gpiod_chip_get_line(gchip, M1_PIN_1);
    assert (glines[1] != NULL);
    glines[2] = gpiod_chip_get_line(gchip, M2_PIN_0);
    assert (glines[2] != NULL);
    glines[3] = gpiod_chip_get_line(gchip, M2_PIN_1);
    assert (glines[3] != NULL);

    // setup monitor control (output)
    for (int i = 0; i < 4; i++) 
    {
        err = gpiod_line_request_output(
            glines[i], NULL, GPIOD_LINE_ACTIVE_STATE_LOW);
        assert (err == 0);
    }


    // open input control line
    glines[4] = gpiod_chip_get_line(gchip, WL_FRONT_PIN);
    assert (glines[4] != NULL);
    glines[5] = gpiod_chip_get_line(gchip, WL_LEFT_PIN);
    assert (glines[5] != NULL);
    glines[6] = gpiod_chip_get_line(gchip, WL_RIGHT_PIN);
    assert (glines[6] != NULL);

    // setup input 
    for (int i = 4; i < 7; i++) 
    {
        err = gpiod_line_request_input(
            glines[i], NULL);
        assert (err == 0);
    }
}

// get input 
int get_wl_input()
{
    int res = 0;

    for (int i = 0; i < 3; i++) 
    {
        int val = gpiod_line_get_value(glines[4 + i]);
        if (val == -1) 
        {
            printf("[GET_WL_INPUT] FAIL\n");
            return 0;
        }
        if (val == 0) 
            res |= (1 << i);
    }
    return res;
}

// set monitor state with monitor control code (M_STOP...)
void set_m(int mx, int val)
{
    gpiod_line_set_value(
        glines[mx * 2], get_bit(val, 0));
    gpiod_line_set_value(
        glines[mx * 2 + 1], get_bit(val, 1));
}
//


//// 
//
// serial global vars
char dev[] = "/dev/ttyS0";
int serial = -1;
struct termios old_tio;

int do_forward_back = DO_STOP;

// initlize serial
void init_serial()
{
    struct termios tios = {0};
    // used for restore
    tcgetattr(serial, &old_tio);

    tios.c_cflag = B9600 | CS8 | CREAD ; 
    
    tcsetattr(serial, TCSANOW, &tios);
}

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
//



////  
// 
// recv and do
void set_fb(int fb)
{
    if (fb == DO_FORWARD)
    {
        // printf("[FORWARD]\n");
        do_forward_back = DO_FORWARD;
        set_m(0, M_FORWARD);
        set_m(1, M_FORWARD);
    } else if(fb == DO_BACK) {
        // printf("[BACK]\n");
        do_forward_back = DO_BACK;
        set_m(0, M_BACK);
        set_m(1, M_BACK);
    } else {
        if (do_forward_back == DO_FORWARD) 
            set_fb(DO_BACK);
        else if (do_forward_back == DO_BACK)
            set_fb(DO_FORWARD);
        usleep(100000);
        do_forward_back = DO_STOP;
        set_m(0, M_STOP);
        set_m(1, M_STOP);
    }
}


int turn_time = BTRC_MODE_TURN_TIME;

void set_direction(int direc)
{
    if (direc == DO_LEFT) 
    {
        // printf("[LEFT]\n");
        set_m(0, M_BACK);
        set_m(1, M_FORWARD);
        usleep(turn_time);
        set_fb(do_forward_back);
    } else {
        // printf("[RIGHT]\n");
        set_m(1, M_BACK);
        set_m(0, M_FORWARD);
        usleep(turn_time);
        set_fb(do_forward_back);
    } 
}
//

// handle loop
void hanlde_recv_loop()
{
    unsigned char op = 0;
    int n_read = 0;
    while ( (n_read = read(serial, &op, 1)) >= 0)
    {
        if (n_read == 1)
        {
            printf("[OP] %2X\n", op);
            switch (op)
            {
            case MODE_AUTO_ON:
                {
                    auto_mode = true;
                    turn_time = AUTO_MODE_TURN_TIME;
                    printf("[AUTO] ON\n");
                }
                break;
            case MODE_AUTO_OFF:
                {
                    auto_mode = false;
                    turn_time = BTRC_MODE_TURN_TIME;
                    set_fb(DO_STOP);
                    printf("[AUTO] OFF\n");
                }
                break;
            case DO_STOP:
                {
                    auto_mode = false;
                    turn_time = BTRC_MODE_TURN_TIME;
                    set_fb(DO_STOP);
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

void auto_mode_loop()
{
    printf("[AUTO_MODE_THREAD] START\n");

    while ( 1 )
    {
        if (auto_mode)
        {
            int op = get_wl_input();
            // printf("[AUTO] %X\n", op);

            if (op == 0) 
            {
                set_fb(DO_FORWARD);
                continue;
            }

            if (op & WL_FRONT 
                || ((op & WL_LEFT != 0) && (op & WL_RIGHT != 0)) ) 
            {
                set_fb(DO_STOP);
                continue;
            }

            if (op & WL_LEFT) {
                printf("[AUTO] TURN RIGHT\n");
                set_direction(DO_RIGHT);
            } else {
                printf("[AUTO] TURN LEFT\n");
                set_direction(DO_LEFT);
            } 
            
            // auto_mode = false;
        }
    }
    
}

void auto_mode_thread()
{
    pthread_t t_auto_mode;
    int err = pthread_create(
        &t_auto_mode, NULL, 
        auto_mode_loop, NULL);
    assert (err == 0);
}


//// 
// 
// ctrl-c handler (stop program)
void sec_exit(int num)
{
    if (serial >= 0)
    {
        printf("\n[CLOSE] %s\n", dev);
        tcsetattr(serial, TCSANOW, &old_tio);
        close(serial);

        gpiod_chip_close(gchip);
        for (int i = 0; i < 4; i++)
            gpiod_line_release(glines[i]);
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
//


//// 
// 
// main
int main()
{
    // 
    init_gpio();

    // open dev 
    serial = open(dev, O_RDWR);
    err_exit (serial < 0, "[OPEN] cannot open %s\n", dev)
    printf("[OPEN] %s\n", dev);

    // 
    init_serial();

    // register ctrl-c
    register_ctrl_c();

    // test
    test_serial();

    auto_mode_thread();

    // handle 
    hanlde_recv_loop();

    sleep(3);

    // exit
    sec_exit(0);

    return 0;
}

