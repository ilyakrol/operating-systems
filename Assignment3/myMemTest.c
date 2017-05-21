#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

#define COUNT 26 // maximum for this program is 26

char* m1[COUNT];

int
main(int argc, char *argv[]) {
    int i, j, pid;
    switch (SELECTION) {
        case NONE:
            printf(1,"Seleciton Mode: NONE\n");
            break;
        case LIFO:
            printf(1,"Seleciton Mode: LIFO\n");
            break;
        case SCFIFO:
            printf(1,"Seleciton Mode: SCFIFO\n");
            break;
        case LAP:
            printf(1,"Seleciton Mode: LAP\n");
            break;
    }
    printf(1, "\nGoing to allocate %d new pages\n", COUNT);
    for(i = 0; i < COUNT; i++) {
        m1[i] = sbrk(PGSIZE);
        printf(1, "Allocating page #%d\n", i+1);
    }
    printf(1, "\nGoing to delete %d pages\n",COUNT);
    for(i = 0; i < COUNT; i++) {
        sbrk(-PGSIZE);
        printf(1,"Deleted page #%d\n",i+1);
    }
    printf(1, "\nGoing to allocate %d new pages\n",COUNT);
    for(i = 0; i < COUNT ; ++i) {
        m1[i] = sbrk(PGSIZE);
        printf(1,"Allocating page #%d\n",i+1);
    }
    printf(1, "\n");
    for(i = 0; i < COUNT; i++) {
        printf(1, "Filling page #%d\n",i+1);
        for (j = 0; j < PGSIZE; j++) {
            m1[i][j] = (i + 1);
        }
    }
    printf(1, "\nforking...\n");
    pid = fork();
    if(pid == 0) {
        printf(1, "\nChild:\n");
        for(i = 0; i < COUNT; i++) {
            printf(1, "Filling page #%d\n", i+1);
            for (j = 0; j < PGSIZE; j++) {
                m1[i][j] = -(i + 1);
            }
        }
    }
    else {
        wait();
        printf(1, "\nFather:\n");
    }
    for(i = 0; i < COUNT; i++) {
        for(j = 0; j < PGSIZE; j = j + (PGSIZE / 4)) {
            printf(1, "%d ", m1[i][j]);
        }
        printf(1,"\n\n");
    }
    printf(1, "\n%s Finished Successfuly!!!\n",(pid == 0) ? "Child" : "Father");
    exit();
    return 0;
}
