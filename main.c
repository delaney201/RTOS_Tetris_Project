<<<<<<< HEAD
/**
 * main.c
 * @author:
 * uP2 - Fall 2022
 */


// your includes and defines
#include <FinalProject_Tetris/G8RTOS_Semaphores.h>
#include <FinalProject_Tetris/G8RTOS_Structures.h>
#include <FinalProject_Tetris/G8RTOS_Scheduler.h>

#include <FinalProject_Tetris/G8RTOS_IPC.h>
#include "threads.h"
#include "driverlib/watchdog.h"
#include "inc/hw_memmap.h"
#include "inc/tm4c123gh6pm.h"
#include "BoardSupport/inc/BoardInitialization.h"
#include <stdbool.h>
#include "BoardSupport/inc/I2CDriver.h"
#include "BoardSupport/inc/RGBLedDriver.h"
#include "driverlib/sysctl.h"
#include "BoardSupport/inc/ILI9341_Lib.h"
#include <time.h>
#include <stdlib.h>


#include "driverlib/gpio.h"

extern bool left;
extern bool right;
extern bool drop;
extern int debounceL;
extern int debounceR;



void Pthread0(void)
{
    if((GPIO_PORTC_DATA_R & 0b00010000) == 0){  //if B0 is pressed (left) PC4
        if(debounceL == 0){  //?
            left = true;
        }
        else if(debounceL == 2){
            debounceL = -1;
        }
        debounceL++;
    }
    else if((GPIO_PORTC_DATA_R & 0b01000000) == 0){  //if B2 is pressed (right) PC6
        if(debounceR == 0){  //?
            right = true;
        }
        else if(debounceR == 2){
            debounceR = -1;
        }
        debounceR++;
    }
    else if((GPIO_PORTC_DATA_R & 0b10000000) == 0){  //pc7, no need to debounce, usually held down
        drop = true;
    }
}

void main(void)
{
    // Initialize the G8RTOS
    G8RTOS_Init();
    // Initialize the BSP
    bool isBoardSetup = InitializeBoard();
    //Initialize LCD
    LCD_Init(true);
    LCD_Clear(0);
    nextBlock = YELLOWB;
    BLOCK tempblock;
    tempblock.color = BLACKB;
    tempblock.active = false;
    a = tempblock;
    blockNum = 0;
    //enable B0 AND B2
       SYSCTL_RCGCGPIO_R |= SYSCTL_RCGC2_GPIOC;
       GPIO_PORTC_DEN_R |= 0xD0;  //0X50
       GPIO_PORTC_PUR_R |= 0xD0;
    // Add background thread 0 to 4
    G8RTOS_AddThread(IdleThread,255,"idle");
   // G8RTOS_AddThread(AccelerometerRead,0,"accelerometer");
    G8RTOS_AddThread(waitStartRestart,0,"waitB0");
    G8RTOS_AddAPeriodicEvent(LCDtap,1,INT_GPIOC);
    G8RTOS_AddPeriodicEvent(Pthread0,100,4); //100

    // Launch the G8RTOS
    G8RTOS_Launch();
}
=======
/**
 * main.c
 * @author:
 * uP2 - Fall 2022
 */


// your includes and defines
#include <FinalProject_Tetris/G8RTOS_Semaphores.h>
#include <FinalProject_Tetris/G8RTOS_Structures.h>
#include <FinalProject_Tetris/G8RTOS_Scheduler.h>

#include <FinalProject_Tetris/G8RTOS_IPC.h>
#include "threads.h"
#include "driverlib/watchdog.h"
#include "inc/hw_memmap.h"
#include "inc/tm4c123gh6pm.h"
#include "BoardSupport/inc/BoardInitialization.h"
#include <stdbool.h>
#include "BoardSupport/inc/I2CDriver.h"
#include "BoardSupport/inc/RGBLedDriver.h"
#include "driverlib/sysctl.h"
#include "BoardSupport/inc/ILI9341_Lib.h"
#include <time.h>
#include <stdlib.h>


#include "driverlib/gpio.h"

extern bool left;
extern bool right;
extern bool drop;
extern int debounceL;
extern int debounceR;



void Pthread0(void)
{
    if((GPIO_PORTC_DATA_R & 0b00010000) == 0){  //if B0 is pressed (left) PC4
        if(debounceL == 0){  //?
            left = true;
        }
        else if(debounceL == 2){
            debounceL = -1;
        }
        debounceL++;
    }
    else if((GPIO_PORTC_DATA_R & 0b01000000) == 0){  //if B2 is pressed (right) PC6
        if(debounceR == 0){  //?
            right = true;
        }
        else if(debounceR == 2){
            debounceR = -1;
        }
        debounceR++;
    }
    else if((GPIO_PORTC_DATA_R & 0b10000000) == 0){  //pc7, no need to debounce, usually held down
        drop = true;
    }
}

void main(void)
{
    // Initialize the G8RTOS
    G8RTOS_Init();
    // Initialize the BSP
    bool isBoardSetup = InitializeBoard();
    //Initialize LCD
    LCD_Init(true);
    LCD_Clear(0);
    nextBlock = YELLOWB;
    BLOCK tempblock;
    tempblock.color = BLACKB;
    tempblock.active = false;
    a = tempblock;
    blockNum = 0;
    //enable B0 AND B2
       SYSCTL_RCGCGPIO_R |= SYSCTL_RCGC2_GPIOC;
       GPIO_PORTC_DEN_R |= 0xD0;  //0X50
       GPIO_PORTC_PUR_R |= 0xD0;
    // Add background thread 0 to 4
    G8RTOS_AddThread(IdleThread,255,"idle");
   // G8RTOS_AddThread(AccelerometerRead,0,"accelerometer");
    G8RTOS_AddThread(waitStartRestart,0,"waitB0");
    G8RTOS_AddAPeriodicEvent(LCDtap,1,INT_GPIOC);
    G8RTOS_AddPeriodicEvent(Pthread0,100,4); //100

    // Launch the G8RTOS
    G8RTOS_Launch();
}
>>>>>>> 770dc6ee1cf041427f3c33d16ac37c524874d30e
