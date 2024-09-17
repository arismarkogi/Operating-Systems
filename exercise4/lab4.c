#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <sys/select.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sys/socket.h>
#include <regex.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

// A very naive check for the validity of the type of socket host
bool valid_socket_host(const char *host){

	regex_t regex;
	// regcomp: Compiles the regular expression specified by pattern into an executable string of op-codes.
	// regexec: Compares the null-ended string against the compiled regular expression to find a match between the two 
    return !(regcomp(&regex, "^(([A-Za-z][A-Za-z0-9\\-]*)\\.)*([A-Za-z][A-Za-z0-9\\-]*)$", REG_EXTENDED)
    		|| regexec(&regex, host, 0, NULL, 0));

}


/* Converts the content of buff from string to int 
   If the conversion succeeds it returns true else it returns false
   if false is returned (*num)'s behavior is undefined */
bool string_to_unsigned_short_int(const char *buff, short int *num, const char end_char){
	
	*num = 0; // Must be initialized to zero to produce the right result

	const char *leftmost_digit = buff; 

    /* If the numbers starts with zero(s)
    we do NOT assume that it is represented in base 8*/
    
    // Check for zero(s) at the begining
    bool zeros = (*leftmost_digit == '0');
   	if(zeros) while(*(++leftmost_digit) == '0');


   /*  2^16 - 1 == 65535  is the maximum value for unsigned short int and has 5 digits
       If the number has more than 5 digits return false
       If any charachter is not digit or end_char return false
	    If the first charachter is end_char and no zero(s) precede, there is no numebr so input is invalid 
	    If the number has exactly 5 digits then we check for overflow:
	   	   if *leftmost_digit >  6 we are sure there is overflow
	   	   if *leftmost_digit <= 6 when there is overflow 
	   	  	    in bit representation of *num, MSB flips --> *num changes sign
   */

    const char *c = leftmost_digit; // Pointer to the number inside buffer the for the loop 
    
    for(int i = 0; i < 6; i++){

    	/*	*num << N == (*num) * (2^N)
    	Add a digit to the result */
    	if(isdigit(*c))
    		*num =  (*num << 3) + (*num << 1) + *(c++) - '0';

    	// Check if there is at least one digit and the for overflow/underflow
    	else if(*c == end_char)
    		
    		return !(i == 0 && !zeros || i == 5 && (*leftmost_digit > '6'|| *num < 0)); 

    	// NOT (digit OR end_char)
    	else return false;
    }
    // More than 5 digits
    return false;
}

bool check_input(char **argv, char **host, char **port, bool *debug){

	char **p = &argv[1]; // pointer for iterating through argv
	bool found_host =false, found_port = false, found_debug = false;

	while(*p != NULL){
	
		if(strcmp("--debug", *p) == 0){
			// The user can write --debug at input up to one time
			if(!found_debug){ 
				*debug = true;
				found_debug = true;
				p++;
			}
			else return false;
		}

		else if(strcmp("--host", *p) == 0){
			// The user can write --host at input up to one time
			if(!found_host){
				p++;

				if(*p == NULL || !valid_socket_host(*p))
					return false;
    			
    			// Assign new value to socket host and continue
				*host = *p++;
			}
			else return false;
		}

		else if(strcmp("--port", *p) == 0){
			// The user can write --port at input up to one time
			if(!found_port){
				p++;
				unsigned short int port_int = 0; // To check if port number is valid, port valid numbers are unsigned short integers

				if(*p == NULL || !string_to_unsigned_short_int(*p, &port_int, '\0') || port_int == 0)
					return false;
				*port = *p++;
			}
			else return false;
		}

		// Wrong input
		else return false;
	}
	return true;	
}


void get_handler(const bool *debug, const int *socket_fd){


	if(write(*socket_fd,"get", 4) < 4){
		perror("write failed\n");
		close(*socket_fd);
		exit(1);
	}
	if(*debug) printf("[DEBUG] sent 'get'\n");


	char buff[50] = {'\0'};
	int nbytes = 0;
	// Read from socket
	if((nbytes = read(*socket_fd, buff, 50)) ==  -1){
		perror("read failed");
		close(*socket_fd);
		exit(1);
	}

	buff[nbytes - 1] = '\0'; // replace '\n'

	if(*debug) printf("[DEBUG] read '%s'\n", buff);

	unsigned short int event, light;
	float temperature;
	time_t timestamp;

	if (sscanf(buff, "%hu %hu %f %ld", &event, &light, &temperature, &timestamp) != 4){
		printf("Wrong input from 'get'\n");
		exit(1);
	}

	temperature /= 100;

	// Convert the timestamp to the local time string
    char *local_time = asctime(localtime(&timestamp));

	printf("-----------------------------------------------\nLatest event:\n");

	switch (event) {
		case 0:
			printf("boot (0)\n");
			break;
        case 1:
            printf("setup (1)\n");
            break;
        case 2:
            printf("interval (2)\n");
            break;
        case 3:
            printf("button (3)\n");
            break;
        case 4:
            printf("motion (4)\n");
            break;
    }

	printf("Temperature is: %f\n", temperature);
	printf("Light level is: %hu\n", light);
	printf("Timestamp is: %s\n", local_time);
}


// Check if buff has the form: "N name surname reason"
bool check_permission(const char *buff){
	
	int N;
	char name[50], surname[50], reason[50];
	
	// sscanf: on success it returns the number of arguments if the list successfully filled
	return sscanf(buff, "%d %s %s %s", &N, name, surname, reason) == 4;

}

void permission_handler(const bool *debug, const int *socket_fd, const char *buff_input){
	
	// Send message to host
	if(write(*socket_fd, buff_input, strlen(buff_input)) < strlen(buff_input)){
		perror("write failed\n");
		close(*socket_fd);
		exit(1);
	}

	if(*debug) printf("[DEBUG] sent '%s'\n", buff_input);	

	char buff_socket[100] = {'\0'};
	
	int nbytes = 0;

	// Read from socket
	if((nbytes = read(*socket_fd, buff_socket, 100))  == -1){
		perror("read failed");
		close(*socket_fd);
		exit(1);
	}

	buff_socket[nbytes - 1] = '\0'; // replace '\n'

	if(*debug) printf("[DEBUG] read '%s'\n", buff_socket);

	printf("Send verification code: '%s'\n", buff_socket);
	
	char input_buffer[100] = {'\0'};

	if((nbytes = read(STDIN_FILENO, input_buffer, 100)) == -1){
		perror("read failed");
		close(*socket_fd);
		exit(1);
	}

	input_buffer[nbytes - 1] = '\0'; // replace '\n'

	if(write(*socket_fd, input_buffer, nbytes) < nbytes){
		perror("write failed");
		close(*socket_fd);
		exit(1);
	}

	if(*debug) printf("[DEBUG] sent '%s'\n", input_buffer);	
	// Read from socket
	if((nbytes = read(*socket_fd, buff_socket, 100))  == -1){
		perror("read failed");
		close(*socket_fd);
		exit(1);
	}

	buff_socket[nbytes - 1] = '\0'; // replace '\n'

	if(*debug) printf("[DEBUG] read '%s'\n", buff_socket);
	printf("Response: '%s'\n\n", buff_socket);
}



int main(int argc, char **argv){

	// Deafault values
	char *host = "iot.dslab.pub.ds.open-cloud.xyz";
	char *port = "18080";
	bool debug = false;
	
	// Check if input is valid
	if(!check_input(argv, &host, &port, &debug)){
		printf("Usage : %s [--host HOST] [--port PORT] [--debug]\n", argv[0]);
		exit(1);
	}

    struct addrinfo hints, *servinfo = NULL;

    memset(&hints, 0, sizeof(hints));   // Set hints to zero
    hints.ai_family = AF_INET;         //  IPv4 
    hints.ai_socktype = SOCK_STREAM;  //   TCP socket

    // Get address information
    if(getaddrinfo(host, port, &hints, &servinfo) != 0){
   		perror("getaddrinfo failed");
   		exit(1);
   	}

   	// Socket Creation
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(socket_fd == -1){
		perror("socket failed");
		exit(1);
	}

   	// Connect to the server
    if (connect(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        perror("connect failed");
        close(socket_fd);
        freeaddrinfo(servinfo);
        exit(1);
    }


	char buff[200] = {'\0'}; // Initialization of empty buffer for STDIN
	int nbytes = 0;

	while(true){
		if((nbytes = read(STDIN_FILENO, buff, 200))  == -1){
			perror("read failed");
			close(socket_fd);
			exit(1);
		}
		
		// memcmp() compares the first (3rd argument) bytes of the strings 
		if(memcmp(buff, "get\n", 4) == 0)
			get_handler(&debug, &socket_fd);

		else if(memcmp(buff, "exit\n", 5) == 0){
			close(socket_fd);
			exit(0);
		}
		
		// If input has the type: N name surname reason
		else if(check_permission(buff)){
			buff[nbytes - 1] = '\0';
			permission_handler(&debug, &socket_fd, buff);
		}
		
		// Print help message
		else
			printf("You can write: 'get' OR 'exit' OR 'N name surname reason'\n");

	}


	close(socket_fd);
	return 0;

}