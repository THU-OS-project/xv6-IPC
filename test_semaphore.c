#include "types.h"
#include "user.h"

void delay(volatile unsigned long long s)
{
	while (s>0)
		--s;
}

int main(void)
{
	printf(1,"%s\n","semphore test case");
	
	sem_init(0,1);
	int cid = fork();
	if(cid==0){
		// This is child
		printf(1, "This is child\n");
		sem_P(0,(void*)getpid());
		printf(1,"%s\n","child is doing something ...");
		delay(10000000);
		printf(1,"%s\n","child done.");
		sem_V(0);
		exit();
	}else{
		// This is parent
		printf(1, "This is parent\n");
		sem_P(0,(void*)getpid());
		printf(1,"%s\n","parent is doing something ...");
		delay(20000000);
		printf(1,"%s\n","parent done.");
		sem_V(0);
	}
	wait();
	exit();
} 
