/**
 * G8RTOS_Scheduler.c
 * uP2 - Fall 2022
 */
#include <stdint.h>
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"
#include "driverlib/systick.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "BoardSupport/inc/RGBLedDriver.h"
#include "G8RTOS_Scheduler.h"
#include "G8RTOS_Structures.h"
#include "G8RTOS_CriticalSection.h"
#include <string.h>

#include "driverlib/gpio.h"

/*
 * G8RTOS_Start exists in asm
 */
extern void G8RTOS_Start();

/* System Core Clock From system_msp432p401r.c */
extern uint32_t SystemCoreClock;

/*
 * Pointer to the currently running Thread Control Block
 */
//extern tcb_t * CurrentlyRunningThread;

/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Defines ******************************************************************************/

/* Status Register with the Thumb-bit Set */
#define THUMBBIT 0x01000000

/*********************************************** Defines ******************************************************************************/


/*********************************************** Data Structures Used *****************************************************************/

/* Thread Control Blocks
 *	- An array of thread control blocks to hold pertinent information for each thread
 */
static tcb_t threadControlBlocks[MAX_THREADS];

/* Thread Stacks
 *	- An array of arrays that will act as individual stacks for each thread
 */
static int32_t threadStacks[MAX_THREADS][STACKSIZE];

/* Periodic Event Threads
 * - An array of periodic events to hold pertinent information for each thread
 */
static ptcb_t Pthread[MAXPTHREADS];

/*********************************************** Data Structures Used *****************************************************************/


/*********************************************** Private Variables ********************************************************************/

/*
 * Current Number of Threads currently in the scheduler
 */
static uint32_t NumberOfThreads;
//static uint32_t threadTotal;   //total threads created, dead or alive
static uint32_t NumberOfSleepThreads;

/*
 * Current Number of Periodic Threads currently in the scheduler
 */
static uint32_t NumberOfPthreads;


static tcb_t *head;
static tcb_t *nextptr;
static tcb_t *prevptr;
static tcb_t *ptr;


/*********************************************** Private Variables ********************************************************************/


/*********************************************** Private Functions ********************************************************************/

/*
 * Initializes the Systick and Systick Interrupt
 * The Systick interrupt will be responsible for starting a context switch between threads
 * Param "numCycles": Number of cycles for each systick interrupt
 */
static void InitSysTick(uint32_t numCycles)
{
    // your code
    SysTickEnable();
    SysTickIntEnable();
    SysTickPeriodSet(numCycles);
}

/*
 * Chooses the next thread to run.
 * Lab 2 Scheduling Algorithm:
 * 	- Simple Round Robin: Choose the next running thread by selecting the currently running thread's next pointer
 * 	- Check for sleeping and blocked threads
 */
void G8RTOS_Scheduler()
{
    // your code
    currentMaxPriority = 255;
    if(CurrentlyRunningThread->isAlive == false){
        CurrentlyRunningThread = nextptr;
    }
    else{
        CurrentlyRunningThread = CurrentlyRunningThread->nextTCB;
    }
    ptr = CurrentlyRunningThread;
    int i;
    for(i = 0; i < NumberOfThreads; i++){
        if(ptr->blocked == 0 && ptr->priority < currentMaxPriority && ptr->asleep == 0){
            //update currentmaxpriority to have priority of lowest priority available thread (run through all threads)
            currentMaxPriority = ptr->priority;
        }
        ptr = ptr->nextTCB;
    }
    while(CurrentlyRunningThread->blocked != 0 || CurrentlyRunningThread->priority > currentMaxPriority || CurrentlyRunningThread->asleep == 1){
        CurrentlyRunningThread = CurrentlyRunningThread->nextTCB;
    }
}


/*
 * SysTick Handler
 * The Systick Handler now will increment the system time,
 * set the PendSV flag to start the scheduler,
 * and be responsible for handling sleeping and periodic threads
 */
void SysTick_Handler()
{
    // your code
    SystemTime++;
   // int countVal = SystemTime%lcm;
    //handle periodic threads
    int i;
    for(i = 0; i < NumberOfPthreads; i++){  //look through all periodic threads
        if(Pthread[i].currentTime == SystemTime){ //run pthread again
            Pthread[i].currentTime = Pthread[i].currentTime + Pthread[i].period;
          //  (&(PThread[i]))->handler();
            (*Pthread[i].handler)();
        }
    }
    //handle sleeping threads
    ptr = CurrentlyRunningThread;
    int j;
    for(j = 0; j < NumberOfThreads; j++){
       if(ptr->sleepCount == SystemTime){
           ptr->asleep = 0;
           NumberOfSleepThreads--;
       }
        ptr = ptr->nextTCB;

    }

    //start scheduler
    HWREG(NVIC_INT_CTRL) |= NVIC_INT_CTRL_PEND_SV;

}

/*********************************************** Private Functions ********************************************************************/


/*********************************************** Public Variables *********************************************************************/

/* Holds the current time for the whole System */
uint32_t SystemTime;

/*********************************************** Public Variables *********************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Sets variables to an initial state (system time and number of threads)
 * Enables board for highest speed clock and disables watchdog
 */
void G8RTOS_Init()
{
    // your code
    threadTotal = 0;
    SystemTime = 0;
    NumberOfThreads = 0;
    NumberOfSleepThreads = 0;
    NumberOfPthreads = 0;
    currentMaxPriority = 255;
    //relocate vector table to SRAM
    uint32_t newVTORTable = 0x20000000;
    uint32_t * newTable = (uint32_t*)newVTORTable;
    uint32_t * oldTable = (uint32_t*)0;
    int i;
    for(i = 0; i < 155; i++){
        newTable[i] = oldTable[i];
    }
    //initialize all tcb's to be dead
    int j;
    for(j = 0; j < MAX_THREADS; j++){
        threadControlBlocks[j].isAlive = false;
    }
    CurrentlyRunningThread = &threadControlBlocks[0]; //take out?
    HWREG(NVIC_VTABLE) = newVTORTable;
}

/*
 * Starts G8RTOS Scheduler
 * 	- Initializes the Systick
 * 	- Sets Context to first thread
 * Returns: Error Code for starting scheduler. This will only return if the scheduler fails
 */
int G8RTOS_Launch()
{
    // your code
    InitSysTick(SysCtlClockGet() / 1000); // 1 ms tick (1Hz / 1000)



        // add your code
        CurrentlyRunningThread = &threadControlBlocks[0];
        int i;
        for(i = 1; i < NumberOfThreads; i++){  //run thread with highest priority (lowest priority number)
            if(CurrentlyRunningThread->priority > threadControlBlocks[i].priority){
                CurrentlyRunningThread = &threadControlBlocks[i];
            }
        }
        // Set priorities of PendSV and SysTick interrupts to the lowest option
        IntPrioritySet(FAULT_PENDSV,0xE0);
        IntPrioritySet(FAULT_SYSTICK,0xE0);
        //Start G8RTOS
        G8RTOS_Start(); // call the assembly function
    return 0;
}


/*
 * Adds threads to G8RTOS Scheduler
 * 	- Checks if there are still available threads to insert to scheduler
 * 	- Initializes the thread control block for the provided thread
 * 	- Initializes the stack for the provided thread to hold a "fake context"
 * 	- Sets stack tcb stack pointer to top of thread stack
 * 	- Sets up the next and previous tcb pointers in a round robin fashion
 * Param "threadToAdd": Void-Void Function to add as preemptable main thread
 * Returns: Error code for adding threads
 */
sched_ErrCode_t G8RTOS_AddThread(void (*threadToAdd)(void), uint8_t priority, char *name)
{
    // your code
    IBit_State = StartCriticalSection();
    if (NumberOfThreads > MAX_THREADS)
        {
            return THREAD_LIMIT_REACHED;
        }
        else
        {
            uint32_t indexToAddTo = 0;
            if (NumberOfThreads == 0)
            {
                // There is only one thread (in the linked list), so both the next and previous threads are itself
                threadControlBlocks[NumberOfThreads].nextTCB = &threadControlBlocks[NumberOfThreads];
                threadControlBlocks[NumberOfThreads].previousTCB = &threadControlBlocks[NumberOfThreads];
            }
            else
            {
                /*
                Append the new thread to the end of the linked list
                * 1. Number of threads will refer to the newest thread to be added since the current index would be NumberOfThreads-1
                * 2. Set the next thread for the new thread to be the first in the list, so that round-robin will be maintained
                * 3. Set the current thread's nextTCB to be the new thread
                * 4. Set the first thread's previous thread to be the new thread, so that it goes in the right spot in the list
                * 5. Point the previousTCB of the new thread to the current thread so that it moves in the correct order
                */

                int i;
                for(i = 0; i < MAX_THREADS; i++){  //find first dead thread spot
                    if(threadControlBlocks[i].isAlive == false){
                        indexToAddTo = i;
                        break;
                    }
                }
                threadControlBlocks[indexToAddTo].nextTCB = CurrentlyRunningThread;
                CurrentlyRunningThread->previousTCB->nextTCB = &threadControlBlocks[indexToAddTo];
                threadControlBlocks[indexToAddTo].previousTCB = CurrentlyRunningThread->previousTCB;
                CurrentlyRunningThread->previousTCB = &threadControlBlocks[indexToAddTo];

            }

            // Set up the stack pointer
            threadControlBlocks[indexToAddTo].stackPointer = &threadStacks[indexToAddTo][STACKSIZE - 16];
            threadStacks[indexToAddTo][STACKSIZE - 1] = THUMBBIT;
            threadStacks[indexToAddTo][STACKSIZE - 2] = (uint32_t)threadToAdd;

            //initialize thread properties
            threadControlBlocks[indexToAddTo].priority = priority;
            if(strlen(name) < MAX_NAME_LENGTH){
                strcpy(threadControlBlocks[indexToAddTo].Threadname, name);
            }
            threadControlBlocks[indexToAddTo].isAlive = true;
            threadControlBlocks[indexToAddTo].ThreadID = threadTotal;
            threadControlBlocks[indexToAddTo].asleep = 0;
            threadControlBlocks[indexToAddTo].blocked = 0;
            // Increment number of threads present in the scheduler
            NumberOfThreads++;
            threadTotal++;
        }
    EndCriticalSection(IBit_State);
    return NO_ERROR;
}


/*
 * Adds periodic threads to G8RTOS Scheduler
 * Function will initialize a periodic event struct to represent event.
 * The struct will be added to a linked list of periodic events
 * Param Pthread To Add: void-void function for P thread handler
 * Param period: period of P thread to add
 * Returns: Error code for adding threads
 */
int G8RTOS_AddPeriodicEvent(void (*PthreadToAdd)(void), uint32_t period, uint32_t execution)
{
    // your code
    if (NumberOfPthreads > MAXPTHREADS)
    {
        return 0;
    }
    else
    {
        Pthread[NumberOfPthreads].handler = PthreadToAdd;
        Pthread[NumberOfPthreads].period = period;
        Pthread[NumberOfPthreads].currentTime = execution;
        Pthread[NumberOfPthreads].executeTime = execution;
        if (NumberOfPthreads == 0)
        {
                // There is only one thread (in the linked list), so both the next and previous threads are itself
                Pthread[NumberOfPthreads].nextPTCB = &Pthread[NumberOfPthreads];
                Pthread[NumberOfPthreads].previousPTCB = &Pthread[NumberOfPthreads];
        }
        else
        {
            Pthread[NumberOfPthreads].nextPTCB = &Pthread[0];
            Pthread[NumberOfPthreads-1].nextPTCB = &Pthread[NumberOfPthreads];
            Pthread[0].previousPTCB = &Pthread[NumberOfPthreads];
            Pthread[NumberOfPthreads].previousPTCB = &Pthread[NumberOfPthreads-1];
        }
        NumberOfPthreads++;
        return 1;
    }
}

sched_ErrCode_t G8RTOS_AddAPeriodicEvent(void (*AthreadToAdd)(void), uint8_t priority, int32_t IRQn)
{
    // your code
   if(IRQn < 155 && IRQn > 0){
       if(priority > 6){
           return HWI_PRIORITY_INVALID;
       }
       else{
           IBit_State = StartCriticalSection();
           volatile uint32_t * intptr = (uint32_t*)HWREG(NVIC_VTABLE);  //*
           intptr[IRQn] = (uint32_t)AthreadToAdd;
           IntPrioritySet(IRQn,priority);
           IntEnable(IRQn);
           EndCriticalSection(IBit_State);
       }
   }
   else{
       return IRQn_INVALID;
   }
   return NO_ERROR;
}


/*
 * Puts the current thread into a sleep state.
 *  param durationMS: Duration of sleep time in ms
 */
void sleep(uint32_t durationMS)
{
    // your code
    CurrentlyRunningThread->sleepCount = durationMS + SystemTime;   //Sets sleep count
    CurrentlyRunningThread->asleep = 1;                             //Puts the thread to sleep
    nextptr = CurrentlyRunningThread->nextTCB;
    NumberOfSleepThreads++;
    HWREG(NVIC_INT_CTRL) |= NVIC_INT_CTRL_PEND_SV;                  //Start context switch
}

threadId_t G8RTOS_GetThreadId()
{
    return CurrentlyRunningThread->ThreadID;        //Returns the thread ID
}

sched_ErrCode_t G8RTOS_KillThread(threadId_t threadID)
{
    // your code
    IBit_State = StartCriticalSection();
    if(NumberOfThreads <= 1){
        EndCriticalSection(IBit_State);
        return CANNOT_KILL_LAST_THREAD;
    }
    ptr = CurrentlyRunningThread;
    while(ptr->ThreadID != threadID){
        if(ptr->nextTCB == CurrentlyRunningThread){
            return THREAD_DOES_NOT_EXIST;
        }
        else{
            ptr = ptr->nextTCB;
        }
    }
    ptr->isAlive = false;
    //change pointers
    ptr->previousTCB->nextTCB = ptr->nextTCB;
    ptr->nextTCB->previousTCB = ptr->previousTCB;
    ptr->nextTCB = ptr;  //point to itself once deleted?
    ptr->previousTCB = ptr;
    NumberOfThreads--;
    EndCriticalSection(IBit_State);
    //check if killed thread is currently running
    if(ptr == CurrentlyRunningThread){
        HWREG(NVIC_INT_CTRL) |= NVIC_INT_CTRL_PEND_SV;                  //Start context switch
    }
    return NO_ERROR;
}

//Thread kills itself
sched_ErrCode_t G8RTOS_KillSelf()
{
    // your code
    IBit_State = StartCriticalSection();
    if(NumberOfThreads <= 1){
        EndCriticalSection(IBit_State);
        return CANNOT_KILL_LAST_THREAD;
    }
    CurrentlyRunningThread->isAlive = false;
    //change pointers
    nextptr = CurrentlyRunningThread->nextTCB;
    CurrentlyRunningThread->previousTCB->nextTCB = CurrentlyRunningThread->nextTCB;
    CurrentlyRunningThread->nextTCB->previousTCB = CurrentlyRunningThread->previousTCB;
    CurrentlyRunningThread->nextTCB = CurrentlyRunningThread;  //point to itself once deleted?
    CurrentlyRunningThread->previousTCB = CurrentlyRunningThread;
    NumberOfThreads--;
    EndCriticalSection(IBit_State);
    HWREG(NVIC_INT_CTRL) |= NVIC_INT_CTRL_PEND_SV;                  //Start context switch
    return NO_ERROR;

}

uint32_t GetNumberOfThreads(void)
{
    return NumberOfThreads;         //Returns the number of threads
}

void G8RTOS_KillAllThreads()
{
    // your code
    while(NumberOfThreads > 1){ //delete all threads but one
        sched_ErrCode_t err = G8RTOS_KillSelf();
    }
    //kill last thread
    IBit_State = StartCriticalSection();
    int i;
    for(i = 0; i < MAX_THREADS; i++){
        if(threadControlBlocks[i].isAlive == true){
            threadControlBlocks[i].isAlive = false;
            NumberOfThreads--;
        }
    }
    EndCriticalSection(IBit_State);
    //do nothing forever
    while(1);
}






/*********************************************** Public Functions *********************************************************************/
