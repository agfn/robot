#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define err_exit(cond, ...) \
    if (cond) \ 
    {  printf(__VA_ARGS__); exit(1); }

int main()
{
    char dev[] = "/dev/ttyUSB0";
    int fd = open(dev, O_WRONLY);
    err_exit (fd < 0, "[OPEN DEV] cannot open %s\n", dev);
    printf("[OPEN DEV] %s opened\n", dev);

    char buf[0x100] = {0};
    buf[0] = 0b10101010;
    write(fd, buf, 1);
    
    close(fd);

    return 0;
}

