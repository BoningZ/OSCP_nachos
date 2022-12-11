

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "copyright.h"
#include "system.h"


#include "synch.h"

#define N_THREADS  10    // the number of threads
#define N_TICKS    1000  // the number of ticks to advance simulated time


Semaphore *mutex;          //semaphore for the mutual exclusion
Semaphore *turnstile1,*turnstile2;        //屏障信号量

Thread *threads[N_THREADS];
int count=0;

void MakeTicks(int n)  // advance n ticks of simulated time将模拟时间提前到下一个预定的硬件中断
{
 for (int i = 1; i <= n; ++i) {
      
          interrupt->OneTick();
    }

}


void BarThread(_int which)
{
    MakeTicks(N_TICKS);
    printf("Thread %d rendezvous\n", which);
    

    mutex->P();
    count=count+1;
    if(count==N_THREADS){
         printf("Thread %d is the last\n", which);
         turnstile2->P();   //
         turnstile1->V();   //
    }
    mutex->V();
    
    turnstile1->P();
    turnstile1->V();
    printf("Thread %d critical point\n", which);

    mutex->P();
    count=count-1;
    if(count==0){
         turnstile1->P();
         turnstile2->V();
    }
    mutex->V();

    turnstile2->P();
    turnstile2->V();
}


void ThreadsBarrier()

{
    //printf("enter 1\n");
    mutex=new Semaphore("mutex", 1);
    turnstile1=new Semaphore("turnstile1", 0);
    turnstile2=new Semaphore("turnstile2", 1);
    int i;
    
    // Create and fork N_THREADS threads 
    for(i = 0; i < N_THREADS; i++) {
       // printf("enter for loop\n");
        threads[i]=new Thread(""+i);
        threads[i]->Fork(BarThread, i);
    };
}




