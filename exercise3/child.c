#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>



void handler_SIGTERM(int signum){
	exit(0);
}

int main(int argc, char **argv){
	
	// For SIGTERM
	struct sigaction act;
	act.sa_handler = handler_SIGTERM;
			
	if(sigaction(SIGTERM, &act, NULL) == -1){
    	perror("Error at sigaction for SIGTERM");
    	exit(1);
   	}

   	const int index = atoi(argv[1]), read_fd = atoi(argv[2]), write_fd = atoi(argv[3]);
	int val = 0; // for the job

	while(true){

		
		// Read message from parent
		if(read(read_fd, &val, sizeof(int)) < 0){
			perror("read failed");
			exit(1);
		}

		printf("[Child %d] [%d] Child received %d!\n", index, getpid(), val);
		
		// The job
		val++;
		sleep(5);

		// Writes back to parent
		if(write(write_fd, &val, sizeof(int)) < 0){
			perror("write failed");
			exit(1);
		}

		printf("[Child %d] [%d] Child finished hard work, writing back %d\n", index, getpid(), val);
	}

}	
