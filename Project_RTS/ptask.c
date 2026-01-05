//---------------------------------------------------------------------------------
//          PTASK LYBRARY
//---------------------------------------------------------------------------------
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include "ptask.h"

//---------------------------------------------------------------------------------
// GLOBAL CONSTANTS

#define     MAX_TASKS   10
#define     MICRO       0
#define     MILLI       1
#define     ACT         1
#define     INACT       0

//---------------------------------------------------------------------------------
// GLOBAL VARIABLES

int ptask_policy = SCHED_FIFO;         // Scheduling policy
struct timespec ptask_t0;              // System start time

//---------------------------------------------------------------------------------
// TASK PARAMETERS

// struct task_par {
//     int         arg;            // task argument
//     long        wcet;           // task WCET in us
//     int         period;         // task period in ms
//     int         deadline;       // relative deadline in ms
//     int         priority;       // task priority in [0, 99]
//     int         dmiss;          // # of deadline misses
//     struct      timespec at;    // next activation time
//     struct      timespec dl;    // current abs deadline
//     pthread_t   tid;            // thread id
//     sem_t       tsem;           // activation semaphore
// };
// struct task_par tp[MAX_TASKS];

//---------------------------------------------------------------------------------
// FUNCTIONS
//---------------------------------------------------------------------------------

//---------------------------------------------------------------------------------
// TIME_COPY(*td, ts):
// copies time variable ts in a destination variable pointed by td
void time_copy(struct timespec *td, struct timespec ts)
{
td->tv_sec = ts.tv_sec;
td->tv_nsec = ts.tv_nsec;
}

//---------------------------------------------------------------------------------
// TIME_ADD_MS(*t, ms):
// adds a value ms expressed in ms to the variable pointed by t
void time_add_ms(struct timespec *t, int ms)
{
    t->tv_sec += ms/1000;
    t->tv_nsec += (ms%1000)*1000000;
    if (t->tv_nsec > 1000000000) {
        t->tv_nsec -= 1000000000;
        t->tv_sec += 1;
    }
}

//---------------------------------------------------------------------------------
// TIME_CMP(t1, t2):
// compares 2 time variable t1 and t2, returns 0 if equal,
// 1 if t1>t2, -1 if t1<t2
int time_cmp(struct timespec t1, struct timespec t2)
{
    if (t1.tv_sec > t2.tv_sec) return 1;
    if (t1.tv_sec < t2.tv_sec) return -1;
    if (t1.tv_nsec > t2.tv_nsec) return 1;
    if (t1.tv_nsec < t2.tv_nsec) return -1;
    return 0;
}

//---------------------------------------------------------------------------------
// PTASK_INIT(policy):
// sets private semaphores for managing explicit activation
// of aperiodic tasks
void ptask_init(int policy)
{
int i;
    ptask_policy = policy;
    clock_gettime(CLOCK_MONOTONIC, &ptask_t0);

    // initialize activation semaphores
    for (i=0; i<MAX_TASKS; i++)
        sem_init(&tp[i].tsem, 0, 0);
}

//---------------------------------------------------------------------------------
// GET_SYSTIME(unit):
// returns current elapsed time since ptask_t0
long    get_systime(int unit)
{
struct timespec t;
long   tu, mul, div;
        switch (unit) {
            case MICRO:     mul = 1000000; div = 1000; break;
            case MILLI:     mul = 1000; div = 1000000; break;
            default:        mul = 1000; div = 1000000; break;
        }

        clock_gettime(CLOCK_MONOTONIC, &t);
        tu = (t.tv_sec - ptask_t0.tv_sec)*mul;
        tu += (t.tv_nsec - ptask_t0.tv_nsec)/div;

        return tu;
}

//---------------------------------------------------------------------------------
// TASK_CREATE(*task, i, period, drel, prio, aflag):
// task creation
int task_create(
    void* (*task) (void *), int i,
    int period, int drel, int prio, int aflag)
{
pthread_attr_t myatt;
struct sched_param mypar;
int tret;

    if (i >= MAX_TASKS) return -1;

    tp[i].arg = i;
    tp[i].period = period;
    tp[i].deadline = drel;
    tp[i].priority = prio;
    tp[i].dmiss = 0;

    pthread_attr_init(&myatt);

    pthread_attr_setinheritsched(&myatt, PTHREAD_EXPLICIT_SCHED);

    pthread_attr_setschedpolicy(&myatt, ptask_policy);

    mypar.sched_priority = tp[i].priority;

    pthread_attr_setschedparam(&myatt, &mypar);

    tret = pthread_create(&tp[i].tid, &myatt, task, (void*)(&tp[i]));
    
    if (aflag == ACT) task_activate(i);

    return tret;
}

//---------------------------------------------------------------------------------
// GET_TASK_INDEX(arg):
// retrieves the task index stored in tp->arg
int get_task_index(void* arg)
{
struct task_par *tpar;
    tpar = (struct task_par *)arg;
    return tpar->arg;
}

//---------------------------------------------------------------------------------
// WAIT_FOR_ACTIVATION(i):
// waits for activation
void wait_for_activation(int i)
{
struct timespec t;
    sem_wait(&tp[i].tsem);
    clock_gettime(CLOCK_MONOTONIC, &t);
    time_copy(&(tp[i].at), t);
    time_copy(&(tp[i].dl), t);
    time_add_ms(&(tp[i].at), tp[i].period);
    time_add_ms(&(tp[i].dl), tp[i].deadline);
}

//---------------------------------------------------------------------------------
// TASK_ACTIVATE(i):
// activates the task
void task_activate(int i)
{
    sem_post(&tp[i].tsem);
}

//---------------------------------------------------------------------------------
// DEADLINE_MISS(i):
// if the thread is still in execution when reactivated, it increments
// the value of dmiss and returns 1, otherwise returns 0
int deadline_miss(int i)
{
struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    if (time_cmp(now, tp[i].dl) > 0) {
        tp[i].dmiss++;
        return 1;
    }
    return 0;
}

//---------------------------------------------------------------------------------
// WAIT_FOR_PERIOD(i):
// suspends the calling thread until the next activation and,
// when awaken, updates activation time and deadline
void wait_for_period(int i)
{
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME,
                    &(tp[i].at), NULL);

    time_add_ms(&(tp[i].at), tp[i].period);
    time_add_ms(&(tp[i].dl), tp[i].period);
}

//---------------------------------------------------------------------------------
// TASK_SET_PERIOD(i, per):
// changes period
void task_set_period(int i, int per)
{
    tp[i].period = per;
}

//---------------------------------------------------------------------------------
// TASK_SET_DEADLINE(i, dline):
// changes deadline
void task_set_deadline(int i, int dline)
{
    tp[i].deadline = dline;
}

//---------------------------------------------------------------------------------
// TASK_PERIOD(i):
// gets period
int task_period(int i)
{
    return tp[i].period;
}

//---------------------------------------------------------------------------------
// TASK_DEADLINE(i):
// gets relative deadline
int task_deadline(int i)
{
    return tp[i].deadline;
}

//---------------------------------------------------------------------------------
// TASK_DMISS(i):
// gets the # of deadline misses
int task_dmiss(int i)
{
    return tp[i].dmiss;
}

//---------------------------------------------------------------------------------
// TASK_ATIME(i, *at):
// copies at from tp to timespec structure
void task_atime(int i, struct timespec *at)
{
    at->tv_sec  = tp[i].at.tv_sec;
    at->tv_nsec = tp[i].at.tv_nsec;
}

//---------------------------------------------------------------------------------
// TASK_ADLINE(i, *dl):
// copies dl from tp to timespec structure
void task_adline(int i, struct timespec *dl)
{
    dl->tv_sec  = tp[i].dl.tv_sec;
    dl->tv_nsec = tp[i].dl.tv_nsec;
}

//---------------------------------------------------------------------------------
// WAIT_FOR_TASK_END(i):
// terminates the task
void wait_for_task_end(int i)
{
    pthread_join(tp[i].tid, NULL);
}

//---------------------------------------------------------------------------------