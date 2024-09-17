#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>


unsigned int N;//Number of children
pid_t  *children; //Store PIDs of childen
char **my_argv;// global variable for argv

void create_child(int *gate){

	children[*gate] = fork();
	
	if(children[*gate] < 0){
  		perror("Failed to create child");
  		exit(1);
  	}
  	
  	else if(children[*gate] == 0){

  		//converts gate state to string for execv			  		
  		char arg1[8], arg2[2] = {my_argv[1][*gate], '\0'};
  		
  		//Converts gate number to string for execv
  		if(sprintf(arg1, "%d", *gate) < 0){
  			printf("sprintf failed\n");
  			exit(1);
  		}
  			
  		char *args[] = {"child", arg1, arg2, NULL};
  		execv(args[0], args);

  		// If execv() returned then it has failed
  		perror("execv failed");
  		exit(1);
  	}
}


void handler_SIGUSR1(int signum){
	
	for(pid_t *p = &children[0]; p < &children[N]; p++){
		if(kill(*p, SIGUSR1) == -1){
			perror("Failed to send signal");
			exit(1);
		}
	}
}

void handler_SIGUSR2(int signum){
	
	for(pid_t *p = &children[0]; p < &children[N]; p++){
		if(kill(*p, SIGUSR2) == -1){
			perror("Failed to send signal");
			exit(1);
		}
	}
}

void handler_SIGTERM(int signum){
	
	//Blocks SIGCHLD signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
		perror("Failed to block SIGCHLD signal");
		exit(1);
	}

	//Main Loop of the handler
	for(int gate = 0; gate < N; gate++){
		printf("Wating for child %d to exit\n", gate);
		
		if(kill(children[gate], SIGTERM) == -1){
			perror("Failed to send signal");
			exit(1);
		}
		
		if(waitpid(children[gate], NULL, 0) == -1){
			perror("waitpid failed");
			exit(1);
		}

		printf("Child %d exited\n", gate);

	}
	
	printf("All children exiting, terminating as well\n");
	
	free(children);
	
	exit(0);
}

void handler_SIGCHLD(int signum){
	
	pid_t child_pid;
	int status;

	//WNOHANG: return immediately if no child has exited
	//WUNTRACED: return if a child has stopped	
	child_pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
	
	if(child_pid == -1){
		perror("waitpid failed");
		exit(1);
	}

	for(int gate = 0; gate < N; gate++){
		if(children[gate] == child_pid){
		
			//child received SIGSTOP or SIGTSTP
			if(WIFSTOPPED(status)){
				printf("[PARENT/PID=%d] Child %d with PID=%d has received SIGSTOP\n", getpid(), gate, child_pid);

				if(kill(child_pid, SIGCONT) == -1){
					perror("Failed to send signal");
					exit(1);
				}
				printf("[PARENT/PID=%d] Child %d with PID=%d has resumed\n", getpid(), gate, child_pid);	
			}

			//child exited
			else{
				printf("[PARENT/PID=%d] Child %d with PID=%d exited\n", getpid(), gate, child_pid);

				create_child(&gate);

				printf("[PARENT/PID=%d] Created new child for gate %d(PID %d) with initial state '%c'\n", getpid(), gate, children[gate], my_argv[1][gate]);
			}	
		}
	}
	
}


int main(int argc, char **argv){

	// Checks the number of arguments
	if(argc != 2){
    	printf("Usage: ./gates (f+t)(f+t)*\n");
    	exit(1);
	}

	// When called with flag --help
	if(strcmp(argv[1], "--help") == 0){
    	printf("Usage: ./gates (f+t)(f+t)*\n");
    	exit(0);
  	}

  	// Checks if argv[1] is valid
  	for(char *c = argv[1]; *c != '\0'; c++){
  		if(*c != 'f' && *c != 't'){
  			printf("Usage: ./gates (f+t)(f+t)*\n");
    		exit(1);
  		}
  	}


  	my_argv = argv; // global variable points to argv
  	N =  strlen(argv[1]); //Number of children
  	
  	if(N >= 4194304){
  		printf("You will run out of PIDs\n");
  		exit(1);
  	}
  	
  	// Stores the PIDs of child processes
  	children = malloc(sizeof(pid_t) * N); 
  	if(children == NULL){
  		printf("malloc failed");
  		exit(1);
  	}
  	
  	// Creates the initial children
  	for (int gate = 0; gate < N; gate++){
  		
  		create_child(&gate);
  		
  		// Prints pid and initial state of newly created children
  		printf("[PARENT/PID=%d] Created child %d (PID=%d) and initial state '%c'\n", getpid(), gate, children[gate], argv[1][gate]);
  	}	

  	

  	//Signal Handlers
  	struct sigaction act1, act2, act3, act4; 

    act1.sa_handler = handler_SIGUSR1; 
    act2.sa_handler = handler_SIGUSR2; 
    act3.sa_handler = handler_SIGTERM; 
    act4.sa_handler = handler_SIGCHLD;

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
    
    if(sigaction(SIGCHLD, &act4, NULL) == -1){
    	perror("Error at sigaction for SIGCHLD");
    	exit(1);
    }

    

    //Main loop 
    while(1)
        pause();
	

	return 0;
}