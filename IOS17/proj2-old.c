#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#include <sys/sem.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/shm.h>


#define ADULT_LEAVE_SEM "/xp95_sem1"
#define CHILD_ENTER_SEM "/xp95_sem2"
#define MUTEX_SEM "/xp95_sem3"
#define ADULT_NEED_LEAVE_SEM "/xp95_sem4"
#define SHM_SIZE sizeof(mem) //sizeof(int)*CHAR_BIT)

sem_t *s_can_adult_leave;
sem_t *s_adult_needs_leave;
sem_t *s_can_child_enter;
sem_t *s_mutex;



//FUNCTION DEFINITIONS
inline void error_msg(char* err_string, int exit_code);
int return_number(char *strNumber);
void generate_adults(int a, int agt, int awt, FILE * file_out, int shmid);
void generate_children(int c, int cgt, int cwt, FILE * file_out, int shmid);
int create_shared_memory();
void delete_shared_memory(int shmid);
void create_semaphores();
void close_semaphores();

//SHARED MEMORY STRUCTURE
typedef struct mem{
  int adult_count;
  int child_count;
  int children_waiting;
  size_t action_count;
}mem;


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

  //All semaphores will be created, errors are handled in this function
  create_semaphores();

  //Creating the shared memory, errors are treated in the function
  int shmid = create_shared_memory();

  //Getting the shared memory segment and getting it ready
  mem *p_shmem = shmat(shmid, NULL, 0);
  p_shmem->action_count = 1;
  p_shmem->adult_count = 0;
  p_shmem->children_waiting = 0;
  p_shmem->child_count = 0;

  //PIDs of helpers
  pid_t helper_adult_pid, helper_child_pid;

  helper_adult_pid = fork();                //Helper process 1 creation - for adults

  if(helper_adult_pid == 0) {               //Helper process 1 continues here
      generate_adults(a,agt,awt,file_out,shmid);


  }
  else {
    if(helper_adult_pid > 0) {              //Main process continues here, this isn'ŧ visible to any other process
      helper_child_pid = fork();              //Helper process 2 creation - generates children

      if(helper_child_pid == 0) {             //Helper process 2 continues here - children generator
        generate_children(c, cgt, cwt, file_out,shmid);
      }
      if(helper_child_pid == -1) {            //FORK FAIL
        error_msg("ERROR: Failed to create child helper process.",2);
      }
      //Main process continues here

      //Waiting for adults
      int status;
      while ((helper_adult_pid = waitpid(-1, &status, 0)) != -1) {
        printf("Adult helper process %d terminated\n",helper_adult_pid);
        //printf("Adult generator ending.\n");
        exit(0);
      }

      //Waiting for children
      while ((helper_child_pid = waitpid(-1, &status, 0)) != -1) {
        printf("Child helper process %d terminated\n",helper_child_pid);
        //printf("Children generator ending.\n");

        exit(0);
      }
    }
    else {
      error_msg("ERROR: Failed to create adult helper process.",2);
    }
  }

  wait(NULL);


  delete_shared_memory(shmid);       //Deleting shared memory
  close_semaphores();                //Closing all semaphores
  fclose(file_out);                  //Writing changes to disk
  return 0;
}

/**
* This function prints the error string to stderr and exits with
* the specified exit code.
*/
void error_msg(char* err_string, int exit_code) {
  fprintf(stderr, "%s\n",err_string);
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

#if 0 //Faulty: This created only one process
void generate_adults(int a, int agt, int awt, FILE * file_out, int shmid) {
  srand(time(0));

  pid_t adult_pid[a];
  for(int i = 0; i < a; i++) {
    //Generating adults in intervals, disabled for first process
    if(i != 0) {
      (void) sleep(agt);
    }

    //Fork creates processes and stores PIDs in an array
    adult_pid[i] = fork();

    if(adult_pid[i] == 0) {                        //Adult's code
      //Attaching shared memory
      mem *p_shmem = shmat(shmid, (void *)0, 0);
      if (p_shmem == (void*)(-1)) {
        close_semaphores();
        delete_shared_memory(shmid);
        error_msg("ERROR: Attaching shared memory segment failed.",2);
      }

      //Process starting
      fprintf(file_out,"%lu\t: A : %d\t: started\n",p_shmem->action_count++,i+1);

      //Entering the critical section, checking mutex status
      sem_wait(s_mutex);
      //Conditions
      fprintf(file_out,"%lu\t: A : %d\t: enter\n",p_shmem->action_count++,i+1);
      sem_post(s_can_child_enter); //Children can enter, last one "closes the door"
      sem_post(s_mutex);

      usleep((rand() % awt)/1000);                   //Milliseconds to microseconds

      sem_post(s_adult_needs_leave);                //This ensures nothing else is outputted while adult waits
      while(trywait(s_can_adult_leave) != EAGAIN) {
        if(p_shmem->child_count <= p_shmem->adult_count*3) {
          sem_post(s_can_adult_leave);
        }
        fprintf(file_out,"%lu\t: A : %d\t: trying to leave\n",p_shmem->action_count++,i+1);
      }

      sem_wait(s_mutex);                            //These operations are critical
      p_shmem->adult_count--;
      sem_post(s_mutex);

      fprintf(file_out,"%lu\t: A : %d\t: finished\n",p_shmem->action_count++,i+1);

      exit(0);                                       //Jumping to helper process
    }
    else {
      if(adult_pid[i] < 0) {                       //FORK FAIL
        //wait(NULL);
        //exit(0);
        close_semaphores();
        delete_shared_memory(shmid);
        fprintf(stderr,"Generating adult process %d failed.", i+1);
        exit(2);                                    //Jumping to the main process
      }
      //Helper goes to next iteration, child is working, but will never get here
    }
  }
}
#endif

void generate_adults(int a, int agt, int awt, FILE * file_out, int shmid) {
  //Randomization of random numbers
  srand(time(0));

  pid_t adult_pid;

  //Process creation
  for(int i = 0; i < a; i++) {

    if(i != 0) {
      (void) sleep(agt/1000);
    }

    adult_pid = fork();

    switch (adult_pid) {
      case -1:                                                            //Fork failure
        close_semaphores();                                               //Closing semaphores
        delete_shared_memory(shmid);                                      //Deleting shared memory
        error_msg("ERROR: Couldn't create new process. Fork failed.",2);  //Printing error and exiting with code 2
      break;

      case 0:;                                         //New adult executes here
        //Attaching shared memory
        mem *p_shmem = shmat(shmid, (void *)0, 0);
        if (p_shmem == (void*)(-1)) {
          close_semaphores();
          delete_shared_memory(shmid);
          error_msg("ERROR: Attaching shared memory segment failed.",2);
        }

        //Process starting
        sem_wait(s_mutex);       //Working with shared memory, locking other processes
        fprintf(file_out,"%lu\t: A : %d\t: started\n",p_shmem->action_count,i+1);
        printf("%lu\t: A : %d\t: started\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_mutex);

        //Entering the critical section - adult can always enter
        sem_wait(s_mutex);
        fprintf(file_out,"%lu\t: A : %d\t: enter\n",p_shmem->action_count,i+1);
        printf("%lu\t: A : %d\t: enter\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        p_shmem->adult_count++;
        for(int i = 0; i < 3; i++) {
          sem_post(s_can_child_enter); //3 children can enter, last one "closes the door"
        }
        sem_post(s_mutex);

        usleep((rand() % awt)/1000);                   //Miliseconds to microseconds, imitating activity

        //Adult wants to leave
        sem_wait(s_mutex);
        sem_post(s_adult_needs_leave);                //This ensures nothing else is outputted while adult waits
        fprintf(file_out,"%lu\t: A : %d\t: trying to leave\n",p_shmem->action_count,i+1);
        printf("%lu\t: A : %d\t: trying to leave\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_mutex);

        //Adult is waiting
        while(sem_wait(s_can_adult_leave) != 0) {
        sem_wait(s_mutex);
        fprintf(file_out,"%lu\t: A : %d\t: waiting : %d : %d\n",p_shmem->action_count,i+1,p_shmem->adult_count,p_shmem->child_count);
        printf("%lu\t: A : %d\t: waiting : %d : %d\n",p_shmem->action_count,i+1,p_shmem->adult_count,p_shmem->child_count);
        p_shmem->action_count++;
        sem_post(s_mutex);
        }

        sem_wait(s_mutex);
        p_shmem->adult_count--;
        fprintf(file_out,"%lu\t: A : %d\t: leave\n",p_shmem->action_count,i+1);
        printf("%lu\t: A : %d\t: leave\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        for(int i = 0; i < 3; i++) {
          sem_wait(s_can_child_enter); //3 children cannot enter from now on
        }
        sem_post(s_mutex);

        wait(0);
        sem_wait(s_mutex);
        fprintf(file_out,"%lu\t: A : %d\t: finished\n",p_shmem->action_count,i+1);
        printf("%lu\t: A : %d\t: finished\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_mutex);

        //Deadlock prevention - allows children to go to the empty center
        if(i == a - 1) {
          for(int i = 0; i < INT_MAX; i++) {
            sem_post(s_can_child_enter);
          }
        }

        exit(0);
      break;
    }
  }
  int status;
  while ((adult_pid = waitpid(-1, &status, 0)) != -1);

}

void generate_children(int c, int cgt, int cwt, FILE * file_out, int shmid) {
  //Randomization of random numbers
  srand(time(0));

  pid_t child_pid;

  //Process creation
  for(int i = 0; i < c; i++) {

    if(i != 0) {
      (void) sleep(cgt/1000);
    }

    child_pid = fork();

    switch (child_pid) {
      case -1:                                                            //Fork failure
        close_semaphores();                                               //Closing semaphores
        delete_shared_memory(shmid);                                      //Deleting shared memory
        error_msg("ERROR: Couldn't create new process. Fork failed.",2);  //Printing error and exiting with code 2
      break;

      case 0:;                                         //New child executes here
        //Attaching shared memory
        mem *p_shmem = shmat(shmid, (void *)0, 0);
        if (p_shmem == (void*)(-1)) {
          close_semaphores();
          delete_shared_memory(shmid);
          error_msg("ERROR: Attaching shared memory segment failed.",2);
        }

        //Process starting
        sem_wait(s_mutex);       //Working with shared memory, locking other processes
        fprintf(file_out,"%lu\t: C : %d\t: started\n",p_shmem->action_count,i+1);
        printf("%lu\t: C : %d\t: started\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_mutex);


        //Trying to enter
        while(sem_wait(s_can_child_enter) != 0) {
          sem_wait(s_mutex);
          fprintf(file_out,"%lu\t: C : %d\t: waiting : %d : %d\n",p_shmem->action_count,i+1,p_shmem->adult_count,p_shmem->child_count);
          printf("%lu\t: C : %d\t: waiting : %d : %d\n",p_shmem->action_count,i+1,p_shmem->adult_count,p_shmem->child_count);
          p_shmem->action_count++;
          p_shmem->children_waiting++;
          sem_post(s_mutex);
        }

        //Entering the critical section
        sem_wait(s_mutex);
        fprintf(file_out,"%lu\t: C : %d\t: enter\n",p_shmem->action_count,i+1);
        printf("%lu\t: C : %d\t: enter\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        p_shmem->child_count++;
        sem_wait(s_can_child_enter); //Decrementing places available for children
        if(p_shmem->children_waiting > 0) { //If children were waiting, decrementing counter of waiting children
          p_shmem->children_waiting--;
        }
        sem_post(s_mutex);

        usleep((rand() % cwt)/1000);                   //Miliseconds to microseconds

        //Child wants to leave
        sem_wait(s_mutex);
        fprintf(file_out,"%lu\t: C : %d\t: trying to leave\n",p_shmem->action_count,i+1);
        printf("%lu\t: C : %d\t: trying to leave\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_mutex);

        //Child is leaving
        sem_wait(s_mutex);
        p_shmem->child_count--;
        fprintf(file_out,"%lu\t: C : %d\t: leave\n",p_shmem->action_count,i+1);
        printf("%lu\t: C : %d\t: leave\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_can_child_enter); //One more children can enter now
        if(p_shmem->child_count <= p_shmem->adult_count*3 && p_shmem->children_waiting == 0) {  //Letting adult go in
          sem_post(s_can_adult_leave);
        }
        sem_post(s_mutex);

        //wait(0);
        sem_wait(s_mutex);
        fprintf(file_out,"%lu\t: C : %d\t: finished\n",p_shmem->action_count,i+1);
        printf("%lu\t: C : %d\t: finished\n",p_shmem->action_count,i+1);
        p_shmem->action_count++;
        sem_post(s_mutex);

        exit(0);
      break;
    }
  }
  int status;
  while (waitpid(-1, &status, 0) != -1); //Waiting for processes to finish
}


/**
* This function creates shared memory and returns its ID for other functions
*/
int create_shared_memory() {
  int shmid;
  if((shmid = shmget(IPC_PRIVATE, SHM_SIZE, 0644 | IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) == -1) {
    close_semaphores();
    error_msg("ERROR: Couldn't create shared memory.",2);
  }
  return shmid;
}

/**
* This function destroys shared memory
*/
void delete_shared_memory(int shmid) {
  shmctl(shmid, IPC_RMID, NULL);
}

/**
* This function creates global semaphores
*/
void create_semaphores() {
  s_can_adult_leave = sem_open(ADULT_LEAVE_SEM, O_CREAT, 0666, 0);
  s_adult_needs_leave = sem_open(ADULT_NEED_LEAVE_SEM, O_CREAT, 0666, 0);
  s_can_child_enter = sem_open(CHILD_ENTER_SEM, O_CREAT, 0666, 0);
  s_mutex = sem_open(MUTEX_SEM, O_CREAT, 0666, 1);

  if(s_can_adult_leave == SEM_FAILED || s_adult_needs_leave == SEM_FAILED || s_can_child_enter == SEM_FAILED || s_mutex == SEM_FAILED) { \
    sem_unlink(ADULT_LEAVE_SEM);
    sem_unlink(ADULT_NEED_LEAVE_SEM);
    sem_unlink(CHILD_ENTER_SEM);
    sem_unlink(MUTEX_SEM);
    error_msg("ERROR: Couldn't create semaphores.",2);
  }
}

/**
* This function deletes all global semaphores
*/
void close_semaphores() {
  sem_unlink(ADULT_LEAVE_SEM);
  sem_unlink(ADULT_NEED_LEAVE_SEM);
  sem_unlink(CHILD_ENTER_SEM);
  sem_unlink(MUTEX_SEM);
}
