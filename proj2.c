/*
 * Project: IOS Project 2
 * File: proj2.c
 * Title: The Senate Bus Problem (modified)
 * Description: -
 * Author: Michal Pospíšil (xpospi95@stud.fit.vutbr.cz)
 */


/*******************************************************************************************
* LIBRARIES
*******************************************************************************************/
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
#include <string.h>

/*******************************************************************************************
 * DEFINES
 ******************************************************************************************/
#define SHM_SIZE sizeof(mem_t) // mem_t is a struct
#define SEM1 "/xpospi95_sem1"
#define SEM2 "/xpospi95_sem2"
#define SEM3 "/xpospi95_sem3"
#define SEM4 "/xpospi95_sem4"
#define SEM5 "/xpospi95_sem5"

/*******************************************************************************************
 * TYPEDEFS
 ******************************************************************************************/
//SHARED MEMORY STRUCTURE
typedef struct mem{
  size_t riders_on_bus;
  size_t action_counter;
  size_t riders_at_stop;
  size_t transported_riders;
}mem_t;

/*******************************************************************************************
 * GLOBAL VARIABLES
 ******************************************************************************************/
sem_t* stop_free_s;        // Is the bus stop ready for bus/rider?
sem_t* io_lock_s;          // Mutex for log file locking
sem_t* bus_riders_ready_s; // Signal for bus that all riders boarded
sem_t* rider_leave_s;      // Signal for riders to leave the bus
sem_t* rider_board_s;      // Signal for riders to enter the bus

int shmid;            // Shared memory

/*******************************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************************/
int return_number(char *str_number);
inline void error_msg(char* err_string, int exit_code);
void close_semaphores();
void delete_shm();
void bus(int riders, int capacity, int roundtrip, FILE* log_f);
void rider_generator(int capacity, int riders, int gen_time, FILE* log_f);

/*******************************************************************************************
 * MAIN
 * Program starts here.
 ******************************************************************************************/
int main(int argc, char* argv[]) {
  // Randomize
  srand(time(0));

  // Eliminating delay connected to using buffers
  setbuf(stdout,NULL);
  setbuf(stderr,NULL);

  // BEGIN Argument handling
  int r;   // Number of riders
  int c;   // Capacity of the bus
  int art; // Maximum rider generation time
  int abt; // Maximum time of a round-trip

  if(argc != 5) {
    error_msg("ERROR: Missing/unexpected arguments.",1);
  }

  r = return_number(argv[1]);
  c = return_number(argv[2]);
  art = return_number(argv[3]);
  abt = return_number(argv[4]);


  // Checking invaild values
  if(r <= 0) {
    error_msg("ERROR: Invalid number of riders.",1);
  }
  if(c <= 0) {
    error_msg("ERROR: Invalid capacity of the bus.",1);
  }
  if(art < 0 || art > 1000) {
    error_msg("ERROR: Invalid maximum rider generation time.",1);
  }
  if(abt < 0 || abt > 1000) {
    error_msg("ERROR: Invalid round-trip time.",1);
  }
  // END Argument handling

  // BEGIN File handling
  FILE* log_f;
  if((log_f = fopen("proj2.out", "w")) == NULL) {
    fprintf(stderr,"%s\n", strerror(errno));
    error_msg("ERROR: Output file couldn't be created.",1);
  }
  setbuf(log_f,NULL);
  // END File handling

  // BEGIN Shared memory initialization
  if((shmid = shmget(IPC_PRIVATE, SHM_SIZE, 0644 | IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR)) == -1) {
    error_msg("ERROR: Couldn't create shared memory.",2);
  }

  // Getting the shared memory segment and getting it ready
  mem_t *shmem_p = shmat(shmid, NULL, 0);
  shmem_p->riders_on_bus = 0;
  shmem_p->action_counter = 0;
  shmem_p->riders_at_stop = 0;
  shmem_p->transported_riders = 0;
  // END Shared memory initialization

  // BEGIN Semaphore init
  stop_free_s = sem_open(SEM1, O_CREAT, 0666, 0);
  io_lock_s = sem_open(SEM2, O_CREAT, 0666, 0);
  bus_riders_ready_s = sem_open(SEM3, O_CREAT, 0666, 0);
  rider_leave_s = sem_open(SEM4, O_CREAT, 0666, 1);
  rider_board_s = sem_open(SEM5, O_CREAT, 0666, 1);

  sem_init(stop_free_s,1,1);          // Is the stop ready for a bus/rider?
  sem_init(io_lock_s,1,1);            // Mutex for manipulating with log and shared memory
  sem_init(bus_riders_ready_s,1,0);   // bus is waiting for riders to complete their action
  sem_init(rider_leave_s,1,0);        // 0 - riders are not allowed to leave until bus sends the signal
  sem_init(rider_board_s,1,0);        // 0 - riders would block, but when signalled, they can move one by one
  // END Semaphore init

  //PIDs of helpers
  pid_t bus_pid, rider_gen_pid;

  bus_pid = fork();                // Bus creation

  if(bus_pid == 0) {               // BUS PROCESS
    // BUS CODE
    bus(r,c,abt,log_f);

  }
  else {
    if(bus_pid < 0) {
      // Bus fork failed
      delete_shm();       //Deleting shared memory
      close_semaphores();      //Closing all semaphores
      fclose(log_f);           //Writing changes to disk
      error_msg("ERROR: Failed to create adult helper process.",2);
    }
  }

  rider_gen_pid = fork();
  if(rider_gen_pid == 0) {               // BUS PROCESS
    // Rider creator CODE
    rider_generator(c, r, art, log_f);

  }
  else {
    if(rider_gen_pid < 0) {
      // Rider creator fork failed
      delete_shm();       //Deleting shared memory
      close_semaphores();      //Closing all semaphores
      fclose(log_f);           //Writing changes to disk
      error_msg("ERROR: Failed to create adult helper process.",2);
    }
  }

  // Wait for children processes
  pid_t w_pid;
  int status = 0;
  while ((w_pid = wait(&status)) > 0);

  delete_shm();            //Deleting shared memory
  close_semaphores();      //Closing all semaphores
  fclose(log_f);           //Writing changes to disk
  return 0;
}

/*******************************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************************/
// Function from my IZP Project 1 - returns number from a string, returns -1 on error.
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

// Prints the error message to stderr and ends the program specified in exit_code
void error_msg(char* err_string, int exit_code){
  fprintf(stderr, "%s\n",err_string);
  exit(exit_code);
}


// This function deletes all global semaphores
void close_semaphores() {
  sem_unlink(SEM1);
  sem_unlink(SEM2);
  sem_unlink(SEM3);
  sem_unlink(SEM4);
  sem_unlink(SEM5);
}


// This function destroys shared memory
void delete_shm() {
  shmctl(shmid, IPC_RMID, NULL);
}

void bus(int riders, int capacity, int roundtrip, FILE* log_f) {
  //Attaching shared memory
  mem_t *shmem_p = shmat(shmid, (void *)0, 0);
  if(shmem_p == (void*)(-1)) {
    close_semaphores();
    delete_shm();
    error_msg("ERROR: Attaching shared memory segment failed.",2);
  }

  // Bus start
  sem_wait(io_lock_s);
  fprintf(log_f, "%lu\t: BUS\t\t: start\n", ++shmem_p->action_counter);
  // NO POST! Evaluating while
  while(shmem_p->transported_riders != (size_t)riders) {
    sem_post(io_lock_s);

    sem_wait(stop_free_s); // Nobody can enter the bus stop
    fprintf(log_f, "%lu\t: BUS\t\t: arrival\n", ++shmem_p->action_counter);
    sem_wait(io_lock_s); // Manipulating shared memory and writing to log
    size_t want_to_board = shmem_p->riders_at_stop;
    if(want_to_board) { // Start boarding if there are people
      fprintf(log_f, "%lu\t: BUS\t\t: start boarding: %lu\n", ++shmem_p->action_counter, shmem_p->riders_at_stop);
      sem_post(io_lock_s); // Allowing all riders to enter (rider has to be able to access the memory)

      if(want_to_board > (size_t)capacity) {
        want_to_board = capacity;
      }

      for(size_t i = 0; i < want_to_board; i++) {
        sem_post(rider_board_s); // Signal to the riders, rider decrements the semaphore
      }

      sem_wait(bus_riders_ready_s); // Last rider provides this signal (based on shared memory)

      sem_wait(io_lock_s); // Get ready to depart
      fprintf(log_f, "%lu\t: BUS\t\t: end boarding: %lu\n", ++shmem_p->action_counter, shmem_p->riders_at_stop);
    }
    fprintf(log_f, "%lu\t: BUS\t\t: depart\n", ++shmem_p->action_counter);
    sem_post(io_lock_s);
    sem_post(stop_free_s);

    // No driving when roundtrip lasts 0 seconds
    if(roundtrip > 0) {
      usleep((rand() % (roundtrip+1)) / 1000);  //Miliseconds to microseconds, imitating roundtrip
    }

    sem_wait(io_lock_s);
    fprintf(log_f, "%lu\t: BUS\t\t: end\n", ++shmem_p->action_counter);
    sem_post(io_lock_s);

    if(want_to_board !=0) { // This doesn't apply to an empty bus
      for(size_t i = 0; i < want_to_board; i++) {
        sem_post(rider_leave_s); // Signal to the riders to leave, rider decrements the semaphore
      }
      sem_wait(bus_riders_ready_s); // Last rider provides this signal (based on shared memory)
    }

    sem_wait(io_lock_s);
  }
  // IO lock engaged before while evaluated to false
  fprintf(log_f, "%lu\t: BUS\t\t: finish\n", ++shmem_p->action_counter);
  sem_post(io_lock_s); // Bus can end now
  exit(0);
}

void rider_generator(int capacity, int riders, int gen_time, FILE* log_f) {
  int status = 0;
  pid_t rider_pid, w_pid;
  for(int i = 0; i < riders; i++) {
    rider_pid = fork();

    switch (rider_pid) {
      case -1:                                                            // Fork failure
        close_semaphores();                                               // Closing semaphores
        delete_shm();                                                     // Deleting shared memory
        error_msg("ERROR: Couldn't create new process. Fork failed.",2);  // Printing error and exiting with code 2
        break;

      case 0:;                                         // New rider executes here
        // Attaching shared memory
        mem_t *shmem_p = shmat(shmid, (void *)0, 0);
        if(shmem_p == (void*)(-1)) {
          close_semaphores();
          delete_shm();
          error_msg("ERROR: Attaching shared memory segment failed.",2);
        }

        // Rider starting
        sem_wait(io_lock_s);       //Working with shared memory, locking other processes
        fprintf(log_f, "%lu\t: RID %d\t\t: start\n", ++shmem_p->action_counter,i+1);
        sem_post(io_lock_s);

        sem_wait(stop_free_s);    // Wait for bus to leave / other rider to enter
        sem_wait(io_lock_s);
        fprintf(log_f, "%lu\t: RID %d\t\t: enter: %lu\n", ++shmem_p->action_counter,i+1,++shmem_p->riders_at_stop);
        sem_post(stop_free_s);
        sem_post(io_lock_s);


        sem_wait(rider_board_s); // This was incremented by bus to the number of waiting people, blocking until bus starts boarding
        sem_wait(io_lock_s);
        shmem_p->riders_at_stop--;
        shmem_p->riders_on_bus++;
        fprintf(log_f, "%lu\t: RID %d\t\t: boarding\n", ++shmem_p->action_counter,i+1);
        if(shmem_p->riders_on_bus == (size_t)capacity || shmem_p->riders_at_stop == 0) {
          sem_post(bus_riders_ready_s); // Will allow bus to continue
        }
        sem_post(io_lock_s);


        sem_wait(rider_leave_s);  // Signal from bus to leave
        sem_wait(io_lock_s);
        shmem_p->riders_on_bus--;
        shmem_p->transported_riders++;
        fprintf(log_f, "%lu\t: RID %d\t\t: finish\n", ++shmem_p->action_counter,i+1);
        if(shmem_p->riders_on_bus == 0) {
          sem_post(bus_riders_ready_s); // Will allow bus to continue
        }
        sem_post(io_lock_s);

        exit(0);
    }
    // Wait before creating other rider... Or don't, you do you!
    if(gen_time > 0) {
      usleep((rand() % (gen_time+1)) / 1000);  //Miliseconds to microseconds, imitating roundtrip
    }
  }
  while ((w_pid = wait(&status)) > 0);
}