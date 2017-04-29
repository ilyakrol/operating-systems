#include "types.h"
#include "user.h"
#include "perf.h"

int
main(void)
{
    int i;
    int j;
    int pid[30];
    struct perf perfs[30];
    int numberOfStartTicks = 0;
    int res = -1;
    int sumWaitingTime = 0;
    int sumRunningTime = 0;
    int sumTurnaroundTime = 0;

    for (i = 0; i < 10 ; i = i + 1){

        pid[i] = fork();
        if(pid[i] < 0)
            printf(1,"fork failed");

        if(pid[i] == 0) {                                   //CPU only
            numberOfStartTicks = uptime();
            while ((uptime()-numberOfStartTicks) < 30 ){}
            exit(0);
        }

        pid[i+10] = fork();
        if(pid[i+10] < 0)
            printf(1,"fork failed");

        if(pid[i+10] == 0) {                                 //Blocking only
            for (j = 0; j < 30; j++) {
                sleep(1);
            }
            exit(0);
        }

        pid[i+20] = fork();
        if(pid[i+20] < 0)
            printf(1,"fork failed");

        if(pid[i+20] == 0) {                                  //Mixed
            for (j = 0; j < 5; j++){
                numberOfStartTicks = uptime();
                while ((uptime()-numberOfStartTicks) < 5){}
                sleep(1);
            }
            exit(0);
        }
    }

    //print results
    for (i = 0; i < 30; i++) {
        // print details of childs
        if (pid[i]) {
            res = wait_stat(&pid[i], &perfs[i]);
            printf(1,"Child pid : %d\n", res);
            printf(1,"Waiting time : %d\n", perfs[i].retime);
            printf(1,"Running time : %d\n", perfs[i].rutime);
            printf(1,"Turnaround time : %d\n", perfs[i].ttime - perfs[i].ctime);

            sumWaitingTime = sumWaitingTime + perfs[i].retime;
            sumRunningTime = sumRunningTime + perfs[i].rutime;
            sumTurnaroundTime = sumTurnaroundTime + (perfs[i].ttime - perfs[i].ctime);
        }
        printf(1,"\n");
    }
    printf(1,"Average Waiting time: %d\n", sumWaitingTime/30);
    printf(1,"Average Running time: %d\n", sumRunningTime/30);
    printf(1,"Average Turnaround time: %d\n", sumTurnaroundTime/30);
    exit(0);
}
 
