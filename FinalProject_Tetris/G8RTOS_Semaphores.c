/**
 * G8RTOS_Semaphores.c
 * uP2 - Fall 2022
 */

/*********************************************** Dependencies and Externs *************************************************************/

#include <stdint.h>
#include "inc/hw_types.h"
#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"
#include "G8RTOS_Semaphores.h"
#include "G8RTOS_CriticalSection.h"
#include "G8RTOS_Scheduler.h"


/*********************************************** Dependencies and Externs *************************************************************/


/*********************************************** Public Functions *********************************************************************/

/*
 * Initializes a semaphore to a given value
 * Param "s": Pointer to semaphore
 * Param "value": Value to initialize semaphore to
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_InitSemaphore(semaphore_t *s, int32_t value)
{
    // your code
    IBit_State = StartCriticalSection();
    *s = value;
    EndCriticalSection(IBit_State);
}

/*
 * No longer waits for semaphore
 *  - Decrements semaphore
 *  - Blocks thread if sempahore is unavailable
 * Param "s": Pointer to semaphore to wait on
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_WaitSemaphore(semaphore_t *s)
{
    // your code
    IBit_State = StartCriticalSection();
    (*s)--;
    EndCriticalSection(IBit_State);
    if(*s < 0){  //semaphore not available
        CurrentlyRunningThread->blocked = s;
       // EndCriticalSection(IBit_State);
        HWREG(NVIC_INT_CTRL) |= NVIC_INT_CTRL_PEND_SV;
    }
}

/*
 * Signals the completion of the usage of a semaphore
 *  - Increments the semaphore value by 1
 *  - Unblocks any threads waiting on that semaphore
 * Param "s": Pointer to semaphore to be signaled
 * THIS IS A CRITICAL SECTION
 */
void G8RTOS_SignalSemaphore(semaphore_t *s)
{
    // your code
    IBit_State = StartCriticalSection();
    (*s)++;
    EndCriticalSection(IBit_State);
    if(*s <= 0){
        tcb_t *ptr = CurrentlyRunningThread->nextTCB;
        while(ptr->blocked != s){  //search for thread blocked on s
            ptr = ptr->nextTCB;
        }
        ptr->blocked = 0;  //wakeup blocked thread once found
    }
}

/*********************************************** Public Functions *********************************************************************/


