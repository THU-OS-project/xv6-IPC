#include "types.h"
#include "stat.h"
#include "user.h"

#define NO_CHILD 8

float avg_global = 0.0;
volatile int flag_handler = 0;

void sig_handler0(void *msg){
	flag_handler  = 10;
	// get avg from msg
	avg_global = *((float *)msg);
	printf(1, "Run sig_handler\n");
	// return 0;
}

void sig_handler(void *msg){
	flag_handler  = 1;
	// get avg from msg
	avg_global = *((float *)msg);
	// printf(1, "Run sig_handler\n");
	// return 0;
}

int
main(int argc, char *argv[])
{

	int tot_sum = 0;
	float variance = 0.0;

	int size=1000;
	short arr[size];

	for(int i=0; i<size; i++){
		arr[i] = i + 1;
	}	

  	// this is to supress warning
  	// printf(1,"first elem %d\n", arr[0]);
  
  	//----FILL THE CODE HERE for unicast sum and multicast variance

	int *cid = (int *)malloc(NO_CHILD * sizeof(int));
  	int ind, par_pid;
  	par_pid = getpid();

	// debug
	// void *p0 = &sig_handler0;
	// printf(1, "sig_handler: %d, %p, %s\n", &sig_handler0, &sig_handler0, &sig_handler0);
	// printf(1, "sig_handler: %d, %p, %s\n", p0, p0, p0);
	// void *p = &sig_handler;
	// printf(1, "sig_handler: %d, %p, %s\n", &sig_handler, &sig_handler, &sig_handler);
	// printf(1, "sig_handler: %d, %p, %s\n", p, p, p);

  	// set the signal handler before forking
  	sig_set(0, &sig_handler);

  	for(ind = 0; ind < NO_CHILD; ind++){
  		cid[ind] = fork();
  		if(cid[ind] == 0) break;
  	}

  	if(ind != NO_CHILD){
  		// All children will come here

  		int start, end, i, pid;
  		pid = getpid();
  		start = ind * (size / NO_CHILD);
  		end = start + (size / NO_CHILD);

  		if(ind == NO_CHILD-1)
  			end = size;

  		int partial_sum = 0;
  		for(i = start; i < end; i++)
  			partial_sum += arr[i];

  		char *msg = (char *)malloc(MSGSIZE);

  		// pack the partial_sum in msg
  		for(i = 0; i < 4; i++)
  			msg[i] = *((char *)&partial_sum + i);

  		// send the partial sum to the co-oridinator process
  		send(pid, par_pid, msg);
		// printf(1, "*** child %d send partial_sum %d ***\n", ind, msg);

  		// pause until msg is received
  		if(flag_handler == 0) sig_pause();
	      // while(flag_handler == 0){
	        
	      // }

  		float partial_var = 0.0;
  		for(i = start; i < end; i++){
  			float diff = (avg_global - (float)arr[i]);
  			partial_var +=  diff * diff;
  		}

  		// pack the partial_var in msg
  		for(i = 0; i < 4; i++)
  			msg[i] = *((char*)&partial_var + i);

  		// send the partial var to the co-oridinator process
  		send(pid, par_pid, msg);
		// printf(1, "*** child %d send partial_var %d ***\n", ind, msg);
  		free(cid);
  		free(msg);
  		exit();
  	}
  	else{
  		// Parent : The coordinator Process
		int i;
		char *msg = (char *)malloc(MSGSIZE);

  		for(i = 0; i < NO_CHILD; i++){
  			recv(msg);
  			tot_sum += *((int *)msg);
			// printf(1, "=== received tot_sum %d: %d ===\n", i, tot_sum);
  		}

  		float avg = (float)tot_sum/size;

  		for(i = 0; i < 4; i++)
  			msg[i] = *((char *)&avg + i);

  		send_multi(par_pid, cid, msg, NO_CHILD);

  		for(i = 0; i < NO_CHILD; i++){
  			recv(msg);
  			variance += *((float *)msg);
			// printf(1, "=== received variance %d: %d ===\n", i, variance);
  		}

  		variance /= (float)size;
  		for(i = 0; i < NO_CHILD; i++)
  			wait();
		free(msg);
  	}

  	//------------------

	printf(1,"Sum of 1..1000 is %d\n", tot_sum);
	printf(1,"Variance of 1..1000 is %d\n", (int)variance);
  	free(cid);
	exit();
}