#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>


char state;
time_t start_time;
int gate;

void print_state(){
	if(state == 't')
		printf("[GATE=%d/PID=%d/TIME=%lds] The gates are open!\n", gate, getpid(), time(NULL) - start_time);
	
	else
		printf("[GATE=%d/PID=%d/TIME=%lds] The gates are closed!\n", gate, getpid(), time(NULL) - start_time);
}

void handler_SIGUSR1(int signum){
	state = 't' + 'f' - state;
	print_state();
}

void handler_SIGUSR2(int sugnum){
	print_state();
}

void handler_SIGTERM(int signum){
	exit(0);
}


void handler_SIGALRM(int signum){
	print_state();
}


// argv[1]: gate number, argv[2]: gate state
int main(int argc, char **argv){
	
	start_time = time(NULL);
	gate = atoi(argv[1]);
	state = argv[2][0];


	struct sigaction act1, act2, act3, act4; 
    act1.sa_handler = handler_SIGUSR1; 
    act2.sa_handler = handler_SIGUSR2; 
    act3.sa_handler = handler_SIGTERM; 
    act4.sa_handler = handler_SIGALRM;

    if(sigaction(SIGUSR1, &act1, NULL) == -1){
    	perror("Error at sigaction for SIGUSR1");
    	exit(1);
    }
    
    if(sigaction(SIGUSR2, &act2, NULL) == -1){
    	perror("Error at sigaction for SIGUSR2");
    	exit(1);
    }
    
    if(sigaction(SIGTERM, &act3, NULL) == -1){
    	perror("Error at sigaction for SIGTERM");
    	exit(1);
    }

    if(sigaction(SIGALRM, &act4, NULL) == -1){
    	perror("Error at sigaction for SIGALRM");
    }


    //Generates SIGALRM every 15 seconds
  	struct itimerval timer;
    timer.it_value.tv_sec = 15; // initial delay in sec
    timer.it_value.tv_usec = 0; // in microseconds
    timer.it_interval.tv_sec = 15; // repeat interval in sec
    timer.it_interval.tv_usec = 0; //in microseconds
    
    //ITIMER_REAL decrements in real time, and delivers SIGALRM upon expiration. 
    // NULL in old_value argument
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer() failed");
        exit(1);
    }


    //Main loop
	while(1)
		pause();
	




	return 0;
}