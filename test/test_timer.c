#include<unistd.h>
#include<stdio.h>
#include<sys/time.h>
#include<signal.h>

void fun(int sig);
int main()
{
    struct itimerval val;
    val.it_value.tv_sec=3;
    val.it_value.tv_usec=0;
    val.it_interval.tv_sec=3;
    val.it_interval.tv_usec=0;
    setitimer(ITIMER_REAL,&val,NULL);
    signal(SIGALRM,fun);

    while(1);
    return 0;
}
void fun(int sig)
{
    printf("ssssssss %d\n", sig);
}


