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


////
//
// hardware control 

// gpio global vars
char gpio_dev[] = "/dev/gpiochip0";
struct gpiod_chip * gchip = NULL;
struct gpiod_line * glines[8] = {0};


// uesd for motor control 
#define M1_PIN_0    13       
#define M1_PIN_1    19
#define M2_PIN_0    6
#define M2_PIN_1    5

// motor control code
#define M_STOP      0
#define M_FORWARD   2
#define M_BACK      1

// used for auto control 
#define IR_PIN1     12
#define IR_PIN2     21
#define IR_PIN3     20
#define IR_PIN4     16

#define IR_LEFT_GUESS       0b0001
#define IR_LEFT             0b0010   
#define IR_RIGHT            0b0100
#define IR_RIGHT_GUESS      0b1000

// turn left/right delay time 
#define BTRC_MODE_TURN_TIME     70000
#define AUTO_MODE_TURN_TIME     10000


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
    glines[4] = gpiod_chip_get_line(gchip, IR_PIN1);
    assert (glines[4] != NULL);
    glines[5] = gpiod_chip_get_line(gchip, IR_PIN2);
    assert (glines[5] != NULL);
    glines[6] = gpiod_chip_get_line(gchip, IR_PIN3);
    assert (glines[6] != NULL);
    glines[7] = gpiod_chip_get_line(gchip, IR_PIN4);
    assert (glines[7] != NULL);


    // setup input 
    for (int i = 4; i < 8; i++) 
    {
        err = gpiod_line_request_input(
            glines[i], NULL);
        assert (err == 0);
    }
}


/*
return current state of infrared sensors 
(combination of IR_LEFT ...)
*/
int get_ir_input()
{
    int res = 0;

    for (int i = 0; i < 4; i++) 
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

// set motor state with motor control code (M_STOP...)
void set_m(int mx, int val)
{
    gpiod_line_set_value(
        glines[mx * 2], get_bit(val, 0));
    gpiod_line_set_value(
        glines[mx * 2 + 1], get_bit(val, 1));
}


//// 
//
// serial global vars
char dev[] = "/dev/ttyS0";
int serial = -1;
struct termios old_tio;

int do_forward_back = DO_STOP;

// set tty attr (baud...)
void init_serial()
{
    struct termios tios = {0};
    // used for restore
    tcgetattr(serial, &old_tio);

    tios.c_cflag = B9600 | CS8 | CREAD ; 
    
    tcsetattr(serial, TCSANOW, &tios);
}

// test serial connection between bluetooth module and pi
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
// let car forward or back, may stop
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

// let car turn left or right 
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


// IS BIT ZERO
#define IBZ(op, mask) ( !((op) & mask) ) 

#define ON_LINE(op) ( IBZ(op, IR_LEFT) || IBZ(op, IR_RIGHT) )

void auto_mode_loop()
{
    printf("[AUTO_MODE_THREAD] START\n");

    while ( 1 )
    {
        if (auto_mode)
        {
            int op = get_ir_input();
            /*
            printf("[AUTO] LG L R RG : %d %d %d %d\n", \
                get_bit(op, 0), get_bit(op, 1), 
                get_bit(op, 2), get_bit(op, 3));
            auto_mode = false;
            */
            if ( IBZ(op, IR_LEFT_GUESS) )
            {
                printf("[AUTO] TURN LEFT\n");
                set_m(0, M_BACK);
                set_m(1, M_FORWARD);
            } else if ( IBZ(op, IR_RIGHT_GUESS) ) {
                printf("[AUTO] TURN RIGHT\n");
                set_m(1, M_BACK);
                set_m(0, M_FORWARD);
            } else if (ON_LINE(op)){
                printf("[AUTO] RUN\n");
                set_m(1, M_FORWARD);
                set_m(0, M_FORWARD);
            }
            usleep(500);
            set_m(1, M_STOP);
            set_m(0, M_STOP);
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



int main()
{
    init_gpio();

    serial = open(dev, O_RDWR);
    err_exit (serial < 0, "[OPEN] cannot open %s\n", dev)
    printf("[OPEN] %s\n", dev);

    init_serial();

    register_ctrl_c();

    test_serial();

    auto_mode_thread();

    hanlde_recv_loop();

    sleep(3);

    sec_exit(0);

    return 0;
}

