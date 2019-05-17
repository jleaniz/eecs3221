#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "fork.h"

//#define DEBUG

int was_alarm=0;

/* The main program does much of the work. parses the command line arguments */
/* sets up the alarm and the alarm signal handler opens the files and pipes */
/* for redirection etc., invoke start_child, closes files that should be  */
/* closed, waits for the children to finish and reports their status */
/* or exits printing a message and kill its children if they do not die */
/* in time (just the parent exiting may not be enough to kill the children) */
int main(int argc, char *argv[]) {
  int i, j;
  int fdin, fdout, fderr, fderr2;
  int count1 = 0, count2 = 0;
  int pipefd[2], wid, wid2;
  pid_t pid, pid2;

  char *buffer = calloc(BUFSIZ, sizeof(char));
  char *token, *subtoken, *saveptr1, *saveptr2;
  char *argv1[10];
  char *argv2[10];
  struct sigaction saparent;

  // check we have the correct number of arguments
  if (argc < 4) {
    fprintf(stderr, "Error: At least 3 arguments are required.\n");
    exit(1);
  }

  // parse the command line arguments and
  // store the result in buffer
  // e.g.
  // input => ./marker ls /tmp -p- wc -c
  // output => ls:/tmp:=:wc:-c
  for (i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-p-")) == 0)
      strncat(buffer, "=", BUFSIZ - strlen(buffer)-1);
     else
      strncat(buffer, argv[i], BUFSIZ - strlen(buffer)-1);
      if (i != argc-1)
	strncat(buffer, ":", BUFSIZ - strlen(buffer)-1);
  }

  /* The following code parses the buffer and divides the arguments */
  /* into two variables, one for each command with its arguments */
#ifdef DEBUG
  printf("buffer: %s\n",buffer);
#endif
  
  for(j=1, token=strtok_r(buffer,"=",&saveptr1);
      token!=NULL;
      j++, token=strtok_r(NULL,"=",&saveptr1)) {

#ifdef DEBUG
    printf("j=%d %s\n", j, token);
#endif    

    for(i=0, subtoken=strtok_r(token,":",&saveptr2);
	subtoken!=NULL;
	i++, subtoken=strtok_r(NULL,":",&saveptr2)) {

#ifdef DEBUG
      printf("i=%d j=%d %s\n", i, j, subtoken);
#endif
      if (j == 1) {
	argv1[i] = malloc(sizeof(char)*strlen(subtoken));
	argv1[i] = subtoken;
	count1++;
      }	else {
	argv2[i] = malloc(sizeof(char)*strlen(subtoken));
	argv2[i] = subtoken;
	count2++;
      }
    }

#ifdef DEBUG
    printf("count1=%d\n", count1);
    printf("count2=%d\n", count2);
#endif
    
    // add a null entry, needed for execvp()
    argv1[count1] = (char*)NULL;
    argv2[count2] = (char*)NULL;
  }

#ifdef DEBUG
  printf("argv1 contents: %s %s %s %s\n", argv1[0], argv1[1], argv1[2], argv1[3]); 
  printf("argv2 contents: %s %s %s\n", argv2[0], argv2[1], argv2[2]);
#endif

  // open files for stdin/out/err redirection
  if ((fdout = open("test.out", O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
    f_error("Error opening output file. Exiting...");
    exit(EXIT_FAILURE);
  }

  if ((fdin = open("test.in", O_RDONLY|O_CREAT, 0666)) < 1) {
    f_error("Error opening input file. Exiting...");
    exit(EXIT_FAILURE);
  }

  if ((fderr = open("test.err1", O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
    f_error("Error opening output file. Exiting...");
    exit(EXIT_FAILURE);
  }
  
  if ((fderr2 = open("test.err2", O_WRONLY|O_CREAT|O_TRUNC, 0666)) < 0) {
    f_error("Error opening output file. Exiting...");
    exit(EXIT_FAILURE);
  }

  // create a pipe for communication between processes
  if (pipe(pipefd) == -1){
    f_error("Pipe() failed. Exiting...");
    exit(EXIT_FAILURE);
  }

  // set up signal handler
  saparent.sa_handler = alrm_handler;
  sigaction(SIGALRM, &saparent, NULL);
  
  // spawn 2 child processes using the arguments from argv
  // if the process does not exit in 3 seconds, an alarm is sent
  pid = start_child(argv1[0], argv1, fdin, pipefd[1], fderr);
  pid2 = start_child(argv2[0], argv2, pipefd[0], fdout, fderr2);


  // if we received SIGALRM then
  // kill our children like a crazy person
  if (was_alarm != 0) {
    fprintf(stderr, "marker: At least one process did not finish\n");
    if(kill(pid, 0) == 0) {
      if(kill(pid, SIGKILL) < 0)
	f_error("kill() failed");
    }
    if(kill(pid2, 0) == 0) {
      if(kill(pid2, SIGKILL) < 0) 
	f_error("kill() failed");
    }
  }

  return 0;
}

