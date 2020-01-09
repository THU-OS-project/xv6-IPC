#include "types.h"
#include "user.h"

void delay(double x)
{
	volatile unsigned long long s = x*10000000;
	while (s>0)
		--s;
}


void test1()
{
	sem_init(0,1);
	int cid = fork();
	if(cid==0){
		// This is child
		printf(1, "    This is child\n");
		sem_P(0,(void*)getpid());
		printf(1,"%s\n","    child is doing something ...");
		delay(1);
		printf(1,"%s\n","    child done.");
		sem_V(0);
		exit();
	}else{
		// This is parent
		printf(1, "    This is parent\n");
		sem_P(0,(void*)getpid());
		printf(1,"%s\n","    parent is doing something ...");
		delay(2);
		printf(1,"%s\n","    parent done.");
		sem_V(0);
	}
	wait();
}

#define BufferNum 0
#define ProductNum 1
#define Mutex 2
double next_pseudo_random_number(double now)
{
	int a = 17;
	int c = 9;
	int m = 1387;
	int x = now*m;
	double y = (x*a+c)%m;
	return y/m;
}
void test2()
{
	sem_init(BufferNum,3);
	sem_init(ProductNum,0);
	sem_init(Mutex,1);
	int cid = fork();
	if(cid==0){
		// This is producer
		double delta = 0.8;
		double total = 0;
		int total_produce = 0;
		while (total<10)
		{
			delta = next_pseudo_random_number(delta);
			delay(delta*5);
			total += delta;
			sem_P(BufferNum,(void*)getpid());
			sem_P(Mutex,(void*)getpid());
			total_produce += 1;
			printf(1,"%s%d\n","    produce a new product. total produce = ",total_produce);
			sem_V(Mutex);
			sem_V(ProductNum);
		}
		exit();
	}else{
		// This is consumer
		double delta = 0.4;
		double total = 0;
		int total_consume = 0;
		while (total<10)
		{
			delta = next_pseudo_random_number(delta);
			delay(delta*5);
			total += delta;
			sem_P(ProductNum,(void*)getpid());
			sem_P(Mutex,(void*)getpid());
			total_consume += 1;
			printf(1,"%s%d\n","    consume a product. total consume = ",total_consume);
			sem_V(Mutex);
			sem_V(BufferNum);
		}
	}
	wait();
}

int main(void)
{
	printf(1,"%s\n","semphore test case--------");

	printf(1,"%s\n","test1 :");
	test1();

	printf(1,"%s\n","test2 :");
	test2();	

	exit();
} 
