#include "types.h"
#include "user.h"
#include "stat.h"
#include "uthread.h"


void tthread1(void *arg) {
  int i;
  for(i = 0; i < 150; i++) {
    printf(1, " 11111111111111111111111111111111111111111111111111111 %d \n",i);
  }
}

void tthread2(void *arg) {
  int i;
  for(i = 0; i < 150; i++) {
    printf(1, " 2222222222222222222222222222222222222222222222222222 %d \n",i);
  }
}

void tthread3(void *arg) {
  int i;
  for(i = 0; i < 150; i++) {
    printf(1, " 3333333333333333333333333333333333333333333333333333 %d \n",i);
  }
}

int main(int argc, char *argv[]) {
	printf(1,"-=-=-=-=-=-=-=-=-=-=-TEXT USER LEVEL THREAD-=-=-=-=-=-=-=-=-=-=-\n");
	uthread_init();
	uthread_create(tthread1, (void*)555);
	uthread_create(tthread2, (void*)555);
	uthread_create(tthread3, (void*)555);
	
	while(1);
	
	exit();
}