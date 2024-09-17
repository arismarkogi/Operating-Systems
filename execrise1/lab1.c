#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>


void write_message(char* caller, int* fd){

  //Calculates the lenght of the message in bytes (1 char = 1 byte, +1 is for NULL terminator)
  int size = 1 + snprintf(NULL, 0, "[%s] getpid()= %d, getppid()= %d\n", caller, getpid(), getppid());

  char* buf = malloc(size);

  //Creates the message and saves it at msg
  snprintf(buf, size, "[%s] getpid()= %d, getppid()= %d\n", caller, getpid(), getppid());

  if (write(*fd, buf, size) < size){
    perror("Failed to write");
    exit(1);
  }

  free(buf);
}


int main(int argc, char **argv){

  // Checks the number of arguments
  if(argc != 2){
    printf("Usage: ./a.out filename\n");
    exit(1);
  }

  // When called with flag --help
  if(strcmp(argv[1], "--help") == 0){
    printf("Usage: ./a.out filename\n");
    exit(0);
  }

  // Checks if the file already exists
  struct stat buffer;
  if(stat(argv[1], &buffer) == 0){
    printf("Error: %s already exists\n", argv[1]);
    exit(1);
  }

  // Creates and opens file
  // O_APPEND: All write actions happen at the end of the file
  // The file's owner can read and write(6)
  // Users in the same group as the file's owner can read(4)
  // All users can read(4)
  int fd = open(argv[1], O_CREAT | O_APPEND | O_WRONLY, 0644);
  if(fd == -1){
    perror("Failed to open file");
    exit(1);
  }

  int status; // for wait system call
  pid_t child = fork();

  // Error
  if(child < 0){
    perror("Failed to create child");
    exit(1);
  }

  // Child's code
  if(child == 0){
    printf("Im the child\n");
    write_message("CHILD", &fd);
    exit(0);
  }

  // Parent’s code
  else{
    write_message("PARENT", &fd);
    wait(&status);
    close(fd);
    exit(0);
  }

  return 0;
}
