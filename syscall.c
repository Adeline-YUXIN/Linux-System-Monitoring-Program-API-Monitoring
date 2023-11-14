#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/time.h>
#include <linux/tty.h>
#include <linux/pid.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/unistd.h>
#include <asm/uaccess.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kallsyms.h>
#include <linux/kprobes.h>
#include <linux/timer.h>
#include <linux/timex.h>

#define TASK_PATH_MAX_LENGTH 512

unsigned long *sys_call_table=0;
//sys_call_table 是系统内核的一块区间，用来将调用号和服务连接起来
//系统调用某一个进程时，就会通过sys_call_table ,来查找到该程序，sys_call_table是一个数组。

unsigned int clear_and_return_cr0(void);
//设置cr0寄存器的第16位为0，如果为1就不能改sys_call_table

void setback_cr0(unsigned int val);
//还原cr0寄存器的值为val

unsigned long orig_syscall_saved = 0;
//定义一个参数来保留函数原始的调用地址

asmlinkage long (*orig_syscall)(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd,unsigned long flags);
//定义一个函数指针，用来保存原来的系统调用


//注册模块用到的一个结构体
static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};


// 将进程信息输出到终端xxx文件，与shell脚本配合进行告警
static int test_usrmodehelper(void){
    
    int result = 0;

    //定义从内核函数获取进程文件地址的参数
    char buf_1[TASK_PATH_MAX_LENGTH] = {0};
    char *task_path = NULL;
    task_path = dentry_path_raw(current->mm->exe_file->f_path.dentry, buf_1, TASK_PATH_MAX_LENGTH); //拿进程执行的文件路径放到buf_1
    char shell[52];
    sprintf(shell,"echo %d >> /home/demo/桌面/xxx",current->pid);//把pid替换%d，再把字符串放到shell

    char cmdPath[] = "/bin/bash";//命令路径
    char* cmdArgv[] = {cmdPath, "-c", shell, NULL};//命令参数
    char* cmdEnvp[] = {"HOME=/", "PATH=/sbin:/bin:/usr/bin", NULL};//命令环境
    result = call_usermodehelper(cmdPath, cmdArgv, cmdEnvp, UMH_WAIT_PROC);//拼起来执行一下；调用callusermodehelper执行shell命令
    printk("=========================================================");
    printk(KERN_DEBUG"shell is: %s",shell);
    printk(KERN_DEBUG"test_usrmodehelper exec! The process is \"%s\",pid is %d\n, pidtask_file_com is \"%s\"",current->comm,current->pid,task_path);

    return 0;
}

/*
 * 获取系统时间
 */
static int gettime(void){
    ktime_t time = 0;   
    time = ktime_get_real(); // 获取到的是纳秒级
    printk("time : %lld s \n", time/1000000000);

    return 0;
}

/**
 *  打印进程调用api函数返回进程pid信息
 */
static int find_get_pid_init(void)
{
    printk("into find_get_pid_init.\n");
    struct pid * kpid=find_get_pid(current->pid); //是个系统函数: 根据进程号，调用函数获取进程描述符信息
    printk("the count of the pid is :%d\n", kpid->count);//显示进程描述符信息
    printk("the level of the pid is :%d\n", kpid->level);
    // 显示进程号

    test_usrmodehelper();

    printk("the pid of the find_get_pid is :%d\n", kpid->numbers[kpid->level].nr);
    printk("out find_get_pid_init.\n");   
    return 0;
}

/**
 * 设置cr0寄存器的第16位为0，如果为1就不能改sys_call_table
 */
unsigned int clear_and_return_cr0(void){
    unsigned int cr0 = 0;
    unsigned int ret;

    /* 将cr0寄存器的值移动到rax寄存器中，同时输出到cr0变量中 */
    asm volatile ("movq %%cr0, %%rax" : "=a"(cr0));
    //带有C/C++表达式的内联汇编格式为：
   //asm volatile("InSTructiON List" : Output: Input: Clobber / Modify);

    ret = cr0;
    cr0 &= 0xfffeffff; /* 将cr0变量值的第16位清0,将修改后的值写入到cr0寄存器 */


    /* 读取cr0的值到人rax寄存器，再将rax寄存器的值放入cr0中 */
    asm volatile ("movq %%rax, %%cr0" :: "a"(cr0));

    return ret;
}


/**,
 * 还原cr0寄存器的值为val，卸载模块用到
 */
void setback_cr0(unsigned int val){
    asm volatile ("movq %%rax, %%cr0" :: "a"(val));
}

/**
 * 自己编写的系统调用函数
 */
static long sys_hackcall(struct perf_event_attr *attr, pid_t pid, int cpu, int group_fd,unsigned long flags){
    /* 打印当前调用该API的PID和当前时间戳 */
    find_get_pid_init();
    gettime();
    printk("capted is successful!!!\n");
    // 调用原来的API函数
    int ret =  orig_syscall(attr,  pid,  cpu, group_fd, flags);
    printk("----------------------------------------------------------");
    printk("orig is running!!!");
    return ret;
}

/**
 *  模块的初始化函数，模块的入口函数，加载模块时调用
 */
static int __init init_hack_module(void){
    int orig_cr0;

    printk("Looking is starting...\n");

    typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
    kallsyms_lookup_name_t kallsyms_lookup_name;
    register_kprobe(&kp);
    kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp);
    /**
     * 获取sys_call_table 虚拟内存地址
    */
   sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");

   /* 保存原始系统调用 */
   // orig_syscall_saved = (int(*)(void))(sys_call_table[__NR_perf_event_open]);
   orig_syscall_saved = sys_call_table[__NR_perf_event_open];
   orig_syscall = (long(*)(struct perf_event_attr *, pid_t , int , int ,unsigned long))sys_call_table[__NR_perf_event_open];

   orig_cr0 = clear_and_return_cr0(); /* 设置cr0寄存器的第16位为0 */


   sys_call_table[__NR_perf_event_open] = (unsigned long)&sys_hackcall; /*  替换成我们编写的函数 */

   setback_cr0(orig_cr0); /* 还原cr0寄存器的值 */

    return 0;
}

/**
 * 模块退出函数，卸载模块时调用
 */
static void __exit exit_hack_module(void){
    int orig_cr0;

    orig_cr0 = clear_and_return_cr0();
    sys_call_table[__NR_perf_event_open] = orig_syscall_saved; /* 设置为原来的系统调用 */
    setback_cr0(orig_cr0);

    printk("Looking is exited...\n");
}

module_init(init_hack_module);
module_exit(exit_hack_module);
MODULE_LICENSE("GPL");