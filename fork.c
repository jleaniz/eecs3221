#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>

#include "fork.h"

/* This is the handler of the alarm signal. It just updates was_alarm */
void alrm_handler(int i)
{
    was_alarm = i;
}

/* Prints string s using perror and exits. Also checks errno to make */
/* sure that it is not zero (if it is just prints s followed by newline */
/* using fprintf to the standard error */
void f_error(char *s)
{
  pid_t pid;
  pid = getpid();
  if (errno == 0) {
    fprintf(stderr, "%s\n", s);
  } else {
    perror(s);
  }
}


/* Creates a child process using fork and a function from the exec family */
/* The standard input output and error are replaced by the last three */
/* arguments to implement redirection and piping */
pid_t start_child(const char *path, char *const argv[],
		  int fdin, int fdout, int fderr)
{
  pid_t wid, pid;
  int status;
  
  // Create child process
  pid = fork();
  
  if (pid<0) {
    f_error("fork failed");
    exit(1);
  }
  
  if (pid==0) { // child process
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // set up file redirections
    if (dup2(fdin,STDIN_FILENO)<0) {
      f_error("dup2 for stdin failed");
      exit(EXIT_FAILURE);
    }

    if (dup2(fderr,STDERR_FILENO)<0) {
      f_error("dup2 for stderr failed");
      exit(EXIT_FAILURE);
    }

    if (dup2(fdout,STDOUT_FILENO)<0) {
      f_error("dup2 for stderr failed");
      exit(EXIT_FAILURE);
    }

    // execute the command
    if (execvp(path, argv) <0) {
      f_error("execvp() failed");
      exit(EXIT_FAILURE);
    }

    close(fdout);
    close(fderr);
    close(fdin);
  } else {
    // panret process
    close(fdin);
    close(fderr);
    close(fdout);

    do {
      alarm(3); // send SIGALRM in 3 seconds
      wid = waitpid(pid, &status, WUNTRACED | WCONTINUED);

      if (wid==pid)
	if (WIFEXITED(status))
	  fprintf(stdout,"Process %s finished with status %d\n",path,WEXITSTATUS(status));
	else if (WIFSIGNALED(status)) 
	  fprintf(stdout,"Process %s was killed\n",path);
       
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  
  return pid;
}
