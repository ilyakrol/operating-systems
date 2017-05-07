#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "user.h"
#include "proc.h"
#include "uthread.h"

#define N 100
#define MAX 1000

int queue[N];

COUNT_SEMAPHORE* mutex;
COUNT_SEMAPHORE* empty;
COUNT_SEMAPHORE* full;
int insert_index = 0;
int get_index = 0;

void
init_semaphores()
{
    mutex = csem_alloc(1);
    empty = csem_alloc(N);
    full = csem_alloc(0);
}

void
insert_item(int data) {
    queue[insert_index] = data;
    printf(2, "inserted %d to queue\n", data);
    insert_index++;
    if(insert_index == N) {
        insert_index = 0;
    }
}

int
get_item() {
    int data = queue[get_index];
    printf(2, "got %d from queue\n", data);
    get_index++;
    if(get_index == N) {
        get_index = 0;
    }
    return data;
}

void
produce(void *arg) {
    int i;
    for(i = 1; i <= 1000; i++) {
        down(empty);
        down(mutex);
        insert_item(i);
        up(mutex);
        up(full);
    }
}

void consume(void *arg) {
    while(1) {
        down(full);
        down(mutex);
        int data = get_item();
        up(mutex);
        up(empty);
        uthread_sleep(data);
        printf(2, "Thread %d slept for %d ticks\n", uthread_self(), data);
        if(data == MAX) {
            free_csem(mutex);
            free_csem(empty);
            free_csem(full);
            exit();
        }
    }
}

int
main(int argc, char *argv[]) {
    init_semaphores();
    uthread_init();
    uthread_create(produce, (void*) 555);
    uthread_create(consume, (void*) 555);
    uthread_create(consume, (void*) 555);
    uthread_create(consume, (void*) 555);
    while(1);
}
