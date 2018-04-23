#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define ADULT_LEAVE_SEM "tmp/xp95_sem1"
#define CHILD_ENTER_SEM "tmp/xp95_sem2"
#define MUTEX_SEM "tmp/xp95_sem3"


inline void error_msg(char* err_string, int exit_code);
int return_number(char *strNumber);
void generate_adults(int a, int agt, int awt, FILE * file_out);
void generate_children(int c, int cgt, int cwt, FILE * file_out);
void create_semaphores();

int main(int argc, char* argv[])
{
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  int a;   //Number of children
  int c;   //Number of children
  int agt; //Maximum adult generation time
  int cgt; //Maximum child generation time
  int awt; //Maximum adult wait time
  int cwt; //Maximum child wait time

  if(argc != 7) {
    error_msg("ERROR: Unexpected arguments.",1);
  }
  else {
    a = return_number(argv[1]);
    c = return_number(argv[2]);
    agt = return_number(argv[3]);
    cgt = return_number(argv[4]);
    awt = return_number(argv[5]);
    cwt = return_number(argv[6]);
  }


  if(a <= 0 || \
     c <= 0 || \
     agt < 0 || agt >= 5001 || \
     cgt < 0 || cgt >= 5001 || \
     awt < 0 || awt >= 5001 || \
     cwt < 0 || cwt >= 5001)                 //Checking for unsupported values
  {
    error_msg("ERROR: Unsupported values.",1);
  }

  FILE* file_out;
  file_out = fopen("proj2.out","w");
  if(file_out == NULL) {
    error_msg("ERROR: Output file couldn't be created.",1);
  }

  setbuf(file_out,NULL);                    //Disabling buffer for the output

  pid_t helper_adult_pid, helper_child_pid;
  helper_adult_pid = fork();                //Helper process 1 creation - generates adults

  if(helper_adult_pid == 0) {               //Helper process 1 continues here
      generate_adults(a,agt,awt,file_out);
  }
  else {
    if(helper_adult_pid > 0) {              //Main process continues here, this isn'ŧ visible to any other process
      helper_child_pid = fork();              //Helper process 2 creation - generates children

      if(helper_child_pid == 0) {             //Helper process 2 continues here - children generator
        //generate_children(c, cgt, cwt, file_out);
      }
      if(helper_child_pid == -1) {            //FORK FAIL
        error_msg("ERROR: Failed to create child helper process.",2);
      }

      wait(NULL); //Waiting for all processes with same group id
      printf("ENDING.");
    }
    else {
      error_msg("ERROR: Failed to create adult helper process.",2);
    }
  }


  fclose(file_out);
  return 0;
}

/**
* This function prints the error string to stderr and exits with
* the specified exit code.
*/
void error_msg(char* err_string, int exit_code) {
  fprintf(stderr, err_string,"\n");
  exit(exit_code);
}

/**
* Function from my IZP Project 1 - returns number from a string,
* returns -1 on error.
*/
int return_number(char *str_number)
{
  int number;
  char *p_nan;

  number = strtol(str_number,&p_nan,10);

  if (p_nan[0] == '\0') {
    return number;
  }
  else {
    return -1;
  }
}

void generate_adults(int a, int agt, int awt, FILE * file_out) {
  srand(time(0));

  pid_t adult_pid[a];
  for(int i = 0; i < a; i++) {
    if(i != 0) {
      (void) sleep(agt);
    }

    adult_pid[i] = fork();

    if(adult_pid[i] == 0) {                        //Adult's code
      fprintf(file_out,"ACTION_COUNT\t: A : %d\t: started\n",i+1);
      fprintf(file_out,"ACTION_COUNT\t: A : %d\t: enter\n",i+1);

      usleep((rand() % awt)/1000);                   //Milliseconds to microseconds

      fprintf(file_out,"ACTION_COUNT\t: A : %d\t: trying to leave\n",i+1);

      fprintf(file_out,"ACTION_COUNT\t: A : %d\t: finished\n",i+1);
      exit(0);                                       //Jumping to helper process
    }
    else {                                         //Helper resumes here
      if(adult_pid[i] > 0) {
        wait(NULL);
        exit(0);                                     //Jumping to the main process
      }
      else {                                       //FORK FAIL
        //FREE THE RESOURCES HERE!!!
        fprintf(stderr,"Generating adult process %d failed.", i+1);
        exit(2);
      }
    }
  }
}
/*
void generate_children(int c, int cgt, int cwt, FILE * file_out) {

}

void create_shared_memory() {
}
*/


void create_semaphores() {
  void* adult_leave_sem = sem_open(ADULT_LEAVE_SEM, O_RDWR);
  void* child_enter_sem = sem_open(CHILD_ENTER_SEM, O_RDWR);
  void* mutex_sem = sem_open(MUTEX_SEM, O_RDWR);

  if(adult_leave_sem == SEM_FAILED || child_enter_sem == SEM_FAILED || mutex_sem == SEM_FAILED) {
    sem_close(adult_leave_sem);
    sem_close(child_enter_sem);
    error_msg("ERROR: Couldn't create semaphores.",2);
  }
}

/*
This project is not finished. This is work in progress.
*/

