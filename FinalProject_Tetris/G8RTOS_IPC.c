/**
 * G8RTOS IPC - Inter-Process Communication
 * @author:
 * uP2 - Fall 2022
 */
#include <stdint.h>
#include "G8RTOS_IPC.h"
#include "G8RTOS_Semaphores.h"

#define FIFOSIZE 16
#define MAX_NUMBER_OF_FIFOS 4

/*
 * FIFO struct will hold
 *  - buffer
 *  - head
 *  - tail
 *  - lost data
 *  - current size
 *  - mutex
 */

/* Create FIFO struct here */

typedef struct FIFO_t {
    int32_t buffer[16];
    int32_t *head;
    int32_t *tail;
    uint32_t lostData;
    semaphore_t currentSize;
    semaphore_t mutex;
} FIFO_t;


/* Array of FIFOS */
static FIFO_t FIFOs[4];


/*
 * Initializes FIFO Struct
 */
int G8RTOS_InitFIFO(uint32_t FIFOIndex)
{
    // your code
    if(FIFOIndex > (MAX_NUMBER_OF_FIFOS-1)){
        return 0;
    }
    else{
        FIFOs[FIFOIndex].head = &FIFOs[FIFOIndex].buffer[0];
        FIFOs[FIFOIndex].tail = &FIFOs[FIFOIndex].buffer[0];
        G8RTOS_InitSemaphore(&FIFOs[FIFOIndex].currentSize,0);
        G8RTOS_InitSemaphore(&FIFOs[FIFOIndex].mutex,1);
        FIFOs[FIFOIndex].lostData = 0;
        return 1;
    }
}

/*
 * Reads FIFO
 *  - Waits until CurrentSize semaphore is greater than zero
 *  - Gets data and increments the head ptr (wraps if necessary)
 * Param: "FIFOChoice": chooses which buffer we want to read from
 * Returns: uint32_t Data from FIFO
 */
int32_t readFIFO(uint32_t FIFOChoice)
{
    // your code
    G8RTOS_WaitSemaphore(&FIFOs[FIFOChoice].mutex);
    G8RTOS_WaitSemaphore(&FIFOs[FIFOChoice].currentSize);  //change to if?
    int32_t readData = *(FIFOs[FIFOChoice].head);
    if(FIFOs[FIFOChoice].head == &FIFOs[FIFOChoice].buffer[FIFOSIZE-1]){  //wrap head pointer
        FIFOs[FIFOChoice].head = &FIFOs[FIFOChoice].buffer[0];
    }
    else{
        (FIFOs[FIFOChoice].head)++;
    }
    G8RTOS_SignalSemaphore(&FIFOs[FIFOChoice].mutex);
    return readData;

}

/*
 * Writes to FIFO
 *  Writes data to Tail of the buffer if the buffer is not full
 *  Increments tail (wraps if necessary)
 *  Param "FIFOChoice": chooses which buffer we want to read from
 *        "Data': Data being put into FIFO
 *  Returns: error code for full buffer if unable to write
 */
int writeFIFO(uint32_t FIFOChoice, uint32_t Data)
{
    // your code
    *(FIFOs[FIFOChoice].tail) = Data;
    if(FIFOs[FIFOChoice].tail == &FIFOs[FIFOChoice].buffer[FIFOSIZE-1]){  //wrap tail pointer
       FIFOs[FIFOChoice].tail = &FIFOs[FIFOChoice].buffer[0];
    }
    else{
        (FIFOs[FIFOChoice].tail)++;
    }
    if(FIFOs[FIFOChoice].currentSize == FIFOSIZE){  // nothing has been read but full buffer (head & tail pointers in same spot)
        (FIFOs[FIFOChoice].lostData)++;
        if(FIFOs[FIFOChoice].head == &FIFOs[FIFOChoice].buffer[FIFOSIZE-1]){  //wrap head pointer
            FIFOs[FIFOChoice].head = &FIFOs[FIFOChoice].buffer[0];
        }
        else{
            (FIFOs[FIFOChoice].head)++;
        }
        G8RTOS_SignalSemaphore(&FIFOs[FIFOChoice].currentSize);
        return 0;
    }
    else{
        G8RTOS_SignalSemaphore(&FIFOs[FIFOChoice].currentSize);
        return 1;
    }
}

