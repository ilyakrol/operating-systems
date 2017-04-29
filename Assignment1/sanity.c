
#include "types.h"
#include "stat.h"
#include "user.h"
#include "perf.h"
#include "fcntl.h"

#define CPU 0
#define BLOCKING 1
#define MIXED 2

#define N 30
void makeio()
{
    int fd = open("path", O_RDONLY);
   char buf[1000];  // 1000 chars seems like a reasonable amount to read
   int bytesRead = read(fd, buf, 1000); // read(...) returns the amount of actual bytes read
   int i;
   for (; i < bytesRead*bytesRead; i++)
   {
       
   }
   
}
int main(int argc, char const *argv[])
{
	int i;
	struct perf perf;
	int tick;
	printf(1, "Starting sanity test\n");
	for(i = 0; i < 30; i++) {
		if(fork() == 0) {	// child process
                        makeio();
			policy(3);
			priority(30);
			int j;
			if(i <= 9) {
				tick = uptime();
				while(tick + 30 > uptime()){}
				exit(0);
			}
			else if(i > 9 && i < 19) {
				for(j = 0; j < 30; j++) {
					sleep(1);
				}
				exit(0);
			}
			else {
			//Mixed: the processes will perform 5 sequential iterations of the following steps - cpu-only
			//computation for 5 ticks followed by system call sleep of a single tick.
			for(j=0; j<5; j++)
			{
				int tick1= uptime();
			    while (tick1 + 5 > uptime()){}
			    sleep(1);
			}
			//system call sleep of a single tick
			exit(0);
			}

		}

	}

	int averageCtime=0,averageTtime=0,averageStime=0,averageRetime=0,averageRutime=0;

	for(i = 0; i < 30; i++){
		int pid = wait_stat(0 , &perf);
		averageCtime += perf.ctime;
		averageTtime += perf.ttime;
		averageStime += perf.stime;
		averageRetime += perf.retime; //sum of waiting time
		averageRutime += perf.rutime;  //sum of running time
		printf(1, "[%d] Waiting Time: %d \n Running Time: %d \n Turnaround Time: %d \n",
			pid,perf.retime,perf.rutime,perf.ttime-perf.ctime);
		}
		printf(1,"Average Waiting Time: %d\n",averageRetime/ 30);
		printf(1,"Average Running Time:%d\n",averageRutime/ 30);
		printf(1,"Average Turnaround Time: %d\n",(averageTtime - averageCtime)/ 30 );


	return 0;
}