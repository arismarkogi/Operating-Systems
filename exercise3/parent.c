#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/select.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>



int N; // Number of children
pid_t *children; // store PIDs of children

/* Converts the content of buff from string to int 
   If the conversion succeeds it returns true else it returns false
   if false is returned (*num)'s behavior is undefined
*/
bool string_to_integer(const char *buff, int *num, const char end_char){
	
	*num = 0; // Must be initialized to zero to produce the right result

	const char *leftmost_digit = buff; 

    /* If first element is '-' it might be a negative integer
    so start iterating from the second element */
    bool neg = (buff[0] == '-'); 
	if(neg) leftmost_digit++;


    /* If the numbers starts with zero(s)
    we do NOT assume that it is represented in base 8*/
    
    // Check for zero(s) at the begining
    bool zeros = (*leftmost_digit == '0');
   	if(zeros) while(*(++leftmost_digit) == '0');


    /*  2^31-1 == INT_MAX and -(2^31) == INT_MIN have 10 digits
        If the number has more than 10 digits return false
        If any charachter is not digit or end_char return false
	    If the first charachter is end_char and no zero(s) precede, there is no numebr so input is invalid 
	    If the number has exactly 10 digits then we check for overflow/underflow:
	   	   if *leftmost_digit >  2 we are sure there is overflow/underflow
	   	   if *leftmost_digit <= 2 when there is overflow/undeflow 
	   	  	    in bit representation of *num, MSB flips --> *num changes sign
    */

    const char *c = leftmost_digit; // Pointer to the number inside buffer the for the loop 
    
    for(int i = 0; i < 11; i++){

    	
    	/*	*num << N == (*num) * (2^N)
    	Add a digit to the result */
    	if(isdigit(*c))
    		*num =  (*num << 3) + (*num << 1) + ((neg) ? '0' - *(c++) : *(c++) - '0');

    	// Check if there is at least one digit and the for overflow/underflow
    	else if(*c == end_char)
    		
    		return !(i == 0 && !zeros || i == 10 && (*leftmost_digit > '2'|| (neg) == (*num > 0))); 

    	// NOT (digit OR end_char)
    	else return false;
    }
    // More than 10 digits
    return false;
}

void exit_all(const int exit_code){

	//Blocks SIGCHLD signal
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1){
		perror("Failed to block SIGCHLD signal");
		exit(1);
	}

	for(int i = 0; i < N; i++){

		printf("[PARENT] Waiting child %d to exit\n", i);
		if(kill(children[i], SIGTERM) == -1){
			perror("Failed to send SIGTERM");
			exit(1);
		}
		// If errno == ECHILD, child has already died
		if(waitpid(children[i], NULL, 0) == -1 || errno == ECHILD){
			perror("waitpid failed");
			exit(1);
		}
	}
	printf("[PARENT] All children exited, terminating as well\n");

	exit(exit_code);
}


void create_child(const int *index, const int **read_fds, const int **write_fds){
	
	if((children[*index] = fork()) < 0){
		perror("fork failed");
		exit(1);
	}

	else if(children[*index] == 0){
		
		// Close read and write and of pipes you won't need
		if(close(read_fds[*index][0]) == -1){
			perror("close failse");
			exit(1);
		}

		if(close(write_fds[*index][1]) == -1){
			perror("close failed");
			exit(1);
		}

		for(int i = *index + 1; i < N; i++){
	

			if(close(read_fds[i][0]) == -1){
				perror("close failed");
				exit(1);
			}
			if(close(read_fds[i][1]) == -1){
				perror("close failed");
				exit(1);
			}
			if(close(write_fds[i][0]) == -1){
				perror("close failed");
				exit(1);
			}
			if(close(write_fds[i][1]) == -1){
				perror("close failed");
				exit(1);
			}

		}

		// Arguments for execv, index and pipe descriptors
		char arg1[4], arg2[11], arg3[11];

		if(sprintf(arg1, "%d", *index) < 0){
  			perror("sprintf failed\n");
  			exit(1);
  		}

  		if(sprintf(arg2, "%d", write_fds[*index][0]) < 0){
  			perror("sprintf failed\n");
  			exit(1);
  		}

  		if(sprintf(arg3, "%d", read_fds[*index][1]) < 0){
  			perror("sprintf failed\n");
  			exit(1);
  		}

  		char *args[] = {"child", arg1, arg2, arg3, NULL};
  		execv(args[0], args);

  		// If execv() returned then it has failed
  		perror("execv failed");
  		exit(1);

	}
}



// Initializes set for select
int select_init(fd_set * const read_fds_set, const int **read_fds, const int * max_fd){
	
    FD_ZERO(read_fds_set); // Initialize empty set
    FD_SET(STDIN_FILENO, read_fds_set); // Add stdin_fd
 
    for(int i = 0; i < N; i++)
    	FD_SET(read_fds[i][0], read_fds_set); // Add the read descriptors
    
    // Checks if there is something to read
		return select(*max_fd, read_fds_set, NULL, NULL, NULL);
}

/* Returns true if integer is valid and assigns value to num
and exits if necessary*/
bool read_input(int *num){
	
	//Initialize buffer and read input
	char buff[200] = {'0'};

	if(read(STDIN_FILENO, buff, 200)  == -1){
		perror("read failed");
		exit_all(1);
	}

	// memcmp() compares the first (3rd argument) bytes of the strings 
	if(memcmp(buff, "exit\n", 5) == 0)
		exit_all(0);
		
    return string_to_integer(buff, num, '\n');
}


// Write message to child through pipe
void write_message(const int *num, const bool *round_robin, const int **write_fds){
	
	static int cur = -1;
	
	// Calculate the child that will have to work 
	cur = (*round_robin) ? ++cur % N : rand() % N;

	if(write(write_fds[cur][1], num, sizeof(int)) < 0){
        	perror("write failed");
        	exit_all(1);
    }

    printf("[Parent] assigned %d to child %d\n", *num, cur );
}



// Read message from child through pipe
void read_message(fd_set * const read_fds_set, const int **read_fds){
	
	for(int i = 0; i < N; i++){
        
        
        if(FD_ISSET(read_fds[i][0], read_fds_set)){
        
        	int val; // Stores the message of the child 

        	// Reads the message from child
        	if(read(read_fds[i][0], &val, sizeof(int)) == -1){
				perror("read failed from parent");
				exit_all(1);
			}

			printf("[Parent] Received result from child %d --> %d\n", i, val);
        }
    }

}




int main(int argc, char **argv){

	// Checks input and prints error message if necessary
	if(argc < 2 || argc > 3 || !string_to_integer(argv[1], &N, '\0') || N <= 0
		|| argc == 3 && strcmp(argv[2], "--round-robin") != 0 && strcmp(argv[2], "--random") != 0){

		printf("Usage: %s <nChildren> [--random] [--round-robin]\n", argv[0]);
		exit(1);

	}

	/* 1024 file descriptors per process
	   Each pipe needs 2 file descriptors
	   2 pipes for every child process in the program below
	   Already three descriptors used for
	   stdin: 0, stdout: 1, and stderr: 2 */
	if(N > 255){
		printf("Cannot create more than 255 pipes\n");
		exit(1);
	}

	// Define order to send message to children
	bool round_robin = (argc == 3 && strcmp(argv[2], "--random") == 0) ? false : true;


	// Array to store PIDs of children
	children = malloc(sizeof(pid_t) * N); 
  	if(children == NULL){
  		perror("malloc failed");
  		exit(1);
  	}
  	

  	/* 2D Arrays to store pipe decriptors
  	fd of pipes, pwrite_fd: parent writes, child reads, pread_fd: child writes, parent: reads*/
	const int **write_fds = malloc(N * sizeof(int*)), **read_fds = malloc(N * sizeof(int*));
 	

  	if(write_fds == NULL){
  		perror("malloc failed");
  		exit(1);
  	}

  	if(read_fds == NULL){
  		perror("malloc failed");
  		exit(1);
  	}
  	
  	for(int i = 0; i < N; i++){
  		
  		write_fds[i] = malloc(2 * sizeof(int));
  		if(write_fds[i] == NULL){
  			perror("malloc failed");
  			exit(1);
  		}

  		read_fds[i] = malloc(2 * sizeof(int));
  		if(read_fds[i] == NULL){
  			perror("malloc failed");
  			exit(1);
  		}
  	}
  	

  	int max_fd = STDIN_FILENO; // Will be used later in select()


    // Creation of pipes
    for(int i = 0; i < N; i++){
    	
    	if(pipe((int*)write_fds[i]) == -1){
    		perror("pipe failed");
    		exit(1);
    	}

    	if(pipe((int*)read_fds[i]) == -1){
    		perror("pipe failed");
    		exit(1);
    	}
    	// Find maximud fd for select
    	if(max_fd < read_fds[i][0])
    		max_fd = read_fds[i][0];
    }

    max_fd++; // highest file descriptor plus one for 1st argument in select() 

    // Create children and close the ends of pipes that the parent won't need 
	for(int i = 0; i < N; i++){
		
		create_child(&i, read_fds, write_fds);
		
		if(close(read_fds[i][1]) == -1){
			perror("close failed");
			exit_all(1);
		}

		if(close(write_fds[i][0]) == -1){
			perror("close failed");
			exit_all(1);
		}
	}


	fd_set read_fds_set;  // For select, set to include file descriptors
	int num = 0, ret = 0; // For stdin, select()

	// Main loop
	while(true){
		
		// Initialize and execute select(), ret : return value of select()
		ret = select_init(&read_fds_set, read_fds, &max_fd);

		if (ret == -1){		
            perror("select failed");
            exit_all(1);
        }

        // There is something to be read
        else if(ret > 0){

        	// If there is messasge from stdin
        	if(FD_ISSET(STDIN_FILENO, &read_fds_set)){
                
                /* read_input() returns true if the input is a valid integer
                and false in the input is invalid or "help" 
                INT_MAX is not allowed because INT_MAX + 1 inside child will cause overflow */

                if(!read_input(&num) || num == INT_MAX)
                	printf("Type a number to send job to a child!\n");
                else
                	write_message(&num, &round_robin, write_fds);
              	  
            } 

            // Read from a pipe
            else
           	  	read_message(&read_fds_set, read_fds);
        }
    }
				

	return 0;
}