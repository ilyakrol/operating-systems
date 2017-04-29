#include "types.h"
#include "user.h"
#include "perf.h"
#include "stat.h"

void
cpuAction(void)
{
  int time;
  priority(1); //low priority
  time = uptime();
  while ((uptime()-time) < 5 ){}
  exit(0);
}

void
blockAction(void)
{
  int i;
  priority(200); //highest priority
  for (i = 0; i < 10; i++) {
    sleep(1);
  }
  exit(0);
}

void
mixAction(void)
{
  int time, i;
  priority(100); //high priority
  for (i = 0; i < 10; i++){
    time = uptime();
    while ((uptime()-time) < 5){}
    sleep(1);
  }
  exit(0);
}

int
doFork(void){
  int pid = fork();
  if(pid < 0){
    printf(2,"****Fork is failed, try again later \n");
    exit(-1);
  }
  return pid;
}

void 
sanity(void)
{
  int i,stat;
    int pid[30];
    struct perf perfs[30];
    int sumWaitingTime[4];
    int sumRunningTime[4];
    int sumTurnaroundTime[4];

    //Reset arrays
    for(i = 0 ; i < 4 ; i++){
      sumWaitingTime[i] = 0;
      sumRunningTime[i] = 0;
      sumTurnaroundTime[i] = 0;
    }

    //Create childs and give them work
    for (i = 0; i < 10 ; i++){ //0-9 cpu, 10-19 block, 20-29 mix 
        pid[i] = doFork();
        if(pid[i] == 0)                              
          cpuAction();

        pid[i+10] = doFork();
        if(pid[i+10] == 0)                   
          blockAction();

        pid[i+20] = doFork();
        if(pid[i+20] == 0)
          mixAction();
    }

    //Update time
    for (i = 0; i < 30; i++) {
        if (pid[i]) {
            wait_stat(&pid[i],&perfs[i]);
            sumWaitingTime[0] += perfs[i].retime;
            sumRunningTime[0] += perfs[i].rutime;
            sumTurnaroundTime[0] += perfs[i].ttime - perfs[i].ctime;
            stat = (int) (i / 10); 
            stat += 1;  // 1-cpu, 2-block, 3-mix
            sumWaitingTime[stat] += perfs[i].retime;
            sumRunningTime[stat] += perfs[i].rutime;
            sumTurnaroundTime[stat] += perfs[i].ttime - perfs[i].ctime;          
        }
    }

    //Print summery
    printf(1,"****%s****\n","Total Actions:");
    printf(1,"Avg Waiting time: %d\n", sumWaitingTime[0]/30);
    printf(1,"Avg Running time: %d\n", sumRunningTime[0]/30);
    printf(1,"Avg Turnaround time: %d\n", sumTurnaroundTime[0]/30);
    printf(1,"Sum Turnaround time: %d\n", sumTurnaroundTime[0]);
    printf(1,"%s\n","\n");
    printf(1,"****%s****\n","Cpu Actions:");
    printf(1,"Cpu Avg Waiting time: %d\n", sumWaitingTime[1]/10);
    printf(1,"Cpu Avg Running time: %d\n", sumRunningTime[1]/10);
    printf(1,"Cpu Avg Turnaround time: %d\n", sumTurnaroundTime[1]/10);
    printf(1,"%s\n","\n");
    printf(1,"****%s****\n","Blocking Actions:");
    printf(1,"Blocking Avg Waiting time: %d\n", sumWaitingTime[2]/10);
    printf(1,"Blocking Avg Running time: %d\n", sumRunningTime[2]/10);
    printf(1,"Blocking Avg Turnaround time: %d\n", sumTurnaroundTime[2]/10);
    printf(1,"%s\n","\n");
    printf(1,"****%s****\n","Mixed Actions:");
    printf(1,"Mixed Avg Waiting time: %d\n", sumWaitingTime[3]/10);
    printf(1,"Mixed Avg Running time: %d\n", sumRunningTime[3]/10);
    printf(1,"Mixed Avg Turnaround time: %d\n", sumTurnaroundTime[3]/10);
    printf(1,"%s\n","\n");
}

int
main(void)
{
    printf(1,"Running sanity on %s policy\n", "Uniform time distribution");
    policy(0);
    sanity();
    printf(1,"Running sanity on %s policy\n", "Priority scheduling");
    policy(1);
    sanity();
    printf(1,"Running sanity on %s policy\n", "Dynamic tickets allocation");
    policy(2);
    sanity();
    policy(0); //return to default policy
    return(0);
}