#include <syscall.h>
#include <stdio.h>
#include <unistd.h> 

int main(void)
{
    unsigned long ret = syscall(__NR_perf_event_open, NULL, 0, 0, 0, 0);
    sleep(5);
    printf("%d\n", (int)ret);
    return 0;
}