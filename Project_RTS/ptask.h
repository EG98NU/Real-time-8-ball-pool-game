//---------------------------------------------------------------------------------
// PTASK LIBRARY HEADER
//---------------------------------------------------------------------------------

// #ifndef PTASK_H
// #define PTASK_H

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

//---------------------------------------------------------------------------------
// GLOBAL CONSTANTS
//---------------------------------------------------------------------------------

#define MAX_TASKS  10               // Maximum number of tasks
#define MICRO      0                // Time unit: microseconds
#define MILLI      1                // Time unit: milliseconds
#define ACT        1                // Task activation flag
#define INACT      0                // Task deactivation flag

//---------------------------------------------------------------------------------
// STRUCTURES
//---------------------------------------------------------------------------------

// Task parameters structure
struct task_par {
    int         arg;            // Task argument (used to identify task index)
    long        wcet;           // Worst-case execution time (WCET) in microseconds
    int         period;         // Task period in milliseconds
    int         deadline;       // Relative deadline in milliseconds
    int         priority;       // Task priority in [0, 99]
    int         dmiss;          // Number of deadline misses
    struct      timespec at;    // Next activation time (absolute)
    struct      timespec dl;    // Current absolute deadline
    pthread_t   tid;            // Thread ID for task
    sem_t       tsem;           // Semaphore for task activation
};
struct task_par tp[MAX_TASKS];

//---------------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//----------------------------------------------------------------------------------

// copies time variable ts in a destination variable pointed by td
void time_copy(struct timespec *td, struct timespec ts);

// adds a value ms expressed in ms to the variable pointed by t
void time_add_ms(struct timespec *t, int ms);

// compares 2 time variable t1 and t2, returns 0 if equal,
// 1 if t1>t2, -1 if t1<t2
int time_cmp(struct timespec t1, struct timespec t2);

// sets private semaphores for managing explicit activation
// of aperiodic tasks
void ptask_init(int policy);

// returns current elapsed time since ptask_t0
long    get_systime(int unit);

// task creation
int task_create(
    void* (*task) (void *), int i,
    int period, int drel, int prio, int aflag);

// retrieves the task index stored in tp->arg
int get_task_index(void* arg);

// waits for activation
void wait_for_activation(int i);

// activates the task
void task_activate(int i);

// if the thread is still in execution when reactivated, it increments
// the value of dmiss and returns 1, otherwise returns 0
int deadline_miss(int i);

// suspends the calling thread until the next activation and,
// when awaken, updates activation time and deadline
void wait_for_period(int i);

// changes period
void task_set_period(int i, int per);

// changes deadline
void task_set_deadline(int i, int dline);

// gets period
int task_period(int i);

// gets relative deadline
int task_deadline(int i);

// gets the # of deadline misses
int task_dmiss(int i);

// copies at from tp to timespec structure
void task_atime(int i, struct timespec *at);

// copies dl from tp to timespec structure
void task_adline(int i, struct timespec *dl);

// terminates the task
void wait_for_task_end(int i);


// #endif // PTASK_H

