<<<<<<< HEAD
/**
 * thread.c
 * @author:
 * uP2 - Fall 2022
 */

// your includes and defines
#include <FinalProject_Tetris/G8RTOS_Semaphores.h>
#include <FinalProject_Tetris/G8RTOS_Structures.h>
#include <FinalProject_Tetris/G8RTOS_IPC.h>
#include <FinalProject_Tetris/G8RTOS_Scheduler.h>
#include "BoardSupport/inc/demo_sysctl.h"
#include "BoardSupport/inc/opt3001.h"
#include "BoardSupport/inc/bmi160_support.h"
#include "driverlib/sysctl.h"
#include "threads.h"
#include "BoardSupport/inc/RGBLedDriver.h"
#include <stdint.h>
#include "BoardSupport/inc/Joystick.h"
#include "BoardSupport/inc/ILI9341_Lib.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include <time.h>
#include <stdlib.h>

#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"
#include <stdio.h>
#include <string.h>

#include "logo.h"




// your function implementations
void IdleThread(void){  //lowest priority
    while(1){
        //do nothing
    }
}
void LCDtap(void){  // B0 interrupt
          GPIOIntDisable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
          start_rotate_flag = true;
}
void waitStartRestart(void){
    bool on = true;
    gameColor = 0;
    LCD_Text(20,300,"Press B1 to start the game",0xFFFF);
    int i;  //145 across 100 down
    int j;
    for(i = 0; i < 200; i++){
        for(j = 0; j < 138; j++){
            if(i != 199){  //dont print last col
                LCD_SetPoint(i+20, j+60, LOGOBMPBIG2_IMAGE[i+j*200]);
            }
        }
    }
    sleepTime = 1000;  //500
    speedSleep = 1000;  //500
    level = 1;
    while(1){
        if(start_rotate_flag == true){
            //draw rectangle over start screen text
           // LCD_DrawRectangle(50,20,220,220,0);
            LCD_DrawRectangle(59,19,140,201,0);
            LCD_Text(20,300,"Press B1 to start the game",0);
            //debounce
            sleep(500);  //added
            //reset B1 flag
            start_rotate_flag = false;
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            //spawn check thread
            G8RTOS_AddThread(InitGame,0,"game board init");
            //kill self
            G8RTOS_KillSelf();
        }
        else{
            if(on){
                LCD_Text(20,300,"Press B1 to start the game",0);
                on = false;
            }
            else{
                LCD_Text(20,300,"Press B1 to start the game",0xFFFF);
                on = true;
            }
        }
        sleep(500);
    }
}
void InitGame(void){
    //print game board & starting attributes
   // LCD_DrawRectangle(15,0,2,220,0xF81F);  //vertical line
    LCD_Text(205,0,"NEXT",0xFFFF);
    LCD_Text(201,200,"SCORE",0xFFFF);
    LCD_Text(210,220,"0",0xFFFF);  //205,220
    LCD_Text(201,260,"LEVEL",0xFFFF);
    uint8_t lvl  = (uint8_t)(level + '0');
    PutChar(210,280,lvl,0xFFFF);

    nextBlock = getRandColor(SystemTime,BLACKB);
    nextnextBlock = getRandColor(SystemTime+1,nextBlock);
    nextnextnextBlock = getRandColor(SystemTime+2,nextnextBlock);
    printLittleNext(1,false);
    printLittleNext(2,false);
    printLittleNext(3,false);

    //Initialize globals
    score = 0;
    debounceL = 0;
    debounceR = 0;
    if(start_rotate_flag){
        GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
        //renable interrupt
        GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
        start_rotate_flag = false;
    }
    drop = false;
    left = false;
    right = false;
    drop = false;

    int i;
    int factor = 0;

    for(i = 0; i < 17; i++){
        LCD_DrawRectangle(factor,0,1,200,0x0004);  //horiz line
        factor += 20;
    }
    factor = 0;
    for(i = 0; i < 11; i++){
        LCD_DrawRectangle(0,factor,1000,1,0x0004);  //vertical line
        factor += 20;
    }
    //initialize global matrix to all black (block aren't added to this until dropped)
    int j;
    for(i = 0; i < SCREEN_LENGTH; i++){
        for(j = 0; j < SCREEN_WIDTH; j++){
            matrix[i][j] = BLACKB;
        }
    }

    matrixMutex = &matrixSP;
    G8RTOS_InitSemaphore(matrixMutex,1);
    activeMutex = &activeT;
    G8RTOS_InitSemaphore(activeMutex,1);
    LCDMutex = &LCD_disp;
    G8RTOS_InitSemaphore(LCDMutex,1);
    int changefifo = G8RTOS_InitFIFO(CHANGE_FIFO);
    G8RTOS_AddThread(checkThread,1,"check thread");
    G8RTOS_KillSelf();
}
void checkThread(void){

   // prevmatrix[SCREEN_LENGTH][SCREEN_WIDTH] = matrix;
    //check game point condition (row can be deleted)
    bool deleted[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    bool updated[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    bool allblack[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    bool taken[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};

    int i, j, k;
    int count = 1;
    int clrcount = 0;
    for(i = SCREEN_LENGTH-1; i >= 0; i--){ //starting at bottom left corner
        for(j = 0; j < SCREEN_WIDTH; j++){
            if(matrix[i][j] != BLACKB){ //all filled in spaces
                if(j == SCREEN_WIDTH-1){ //last row in col
                    if(count == SCREEN_WIDTH){ //delete row
                        /*for(k = 0; k < SCREEN_WIDTH; k++){ //clear row in matrix
                            matrix[i][k] = BLACKB;
                        }*/
                        deleted[i] = true;
                        score+=5;
                        if(score < 10){
                            uint8_t scr  = (uint8_t)(score + '0');
                           // uint8_t* scr_ptr = &scr;
                            LCD_DrawRectangle(220,210,20,20,0);
                          //  LCD_Text(210,220,scr_ptr,0xFFFF);
                            PutChar(210,220,scr,0xFFFF);  //205,220
                        }
                        else if(score < 100){
                            uint8_t scrl = (uint8_t)((score % 10) + '0');
                            uint8_t div = score/10;
                            uint8_t scrh = (uint8_t)((div % 10) + '0');
                            LCD_DrawRectangle(220,210,20,20,0);
                            PutChar(210,220,scrh,0xFFFF);
                            PutChar(220,220,scrl,0xFFFF);
                        }
                        else{  //max score is 95
                            score-=5;
                        }
                    }
                    count = 0;
                }
                count++;
            }
            else{
                clrcount++;
                if(j == SCREEN_WIDTH-1){ //last row in col
                    if(clrcount == SCREEN_WIDTH){
                        allblack[i] = true;
                    }
                    clrcount = 0;
                }
            }
        }
        clrcount = 0;
        count = 1;
    }
   // int skip = 0;
    i = SCREEN_LENGTH-1;
    while(!deleted[i]){
        if(i == 0){
            break;  //no rows to delete
        }
        i--;
    }
    for(; i > 0; i--){  //move down rows in matrix
        if(allblack[i]){
            break;
        }
        else{
            int row = i-1;
            while(deleted[row] || taken[row]){
                if(row == 0){  //no rows were found to move down
                    break;
                }
                row--;
            }
            taken[row] = true;
            updated[row] = true;
            //copy contents of desired row and clear row
            for(k = 0; k < SCREEN_WIDTH; k++){
                matrix[i][k] = matrix[row][k];
                matrix[row][k] = BLACKB;
            }
            updated[i] = true;
        }
    }
    for(k = 0; k < SCREEN_WIDTH; k++){
        if(matrix[0][k] != BLACKB){
            //sleep(100);
            //game over
            LCD_Clear(0);
            G8RTOS_AddThread(gameOver,1,"game over");
            G8RTOS_KillSelf(); //?
        }
    }
    //reprint LCD rows from first deleted row until row of all black
    count = 0;
    for(i = SCREEN_LENGTH-1; i >= 0; i--){
        if(updated[i]){  //redraw updated rows
            for(k = 0; k < SCREEN_WIDTH; k++){
                int16_t hexcolor = getColorVal(matrix[i][k]);
                LCD_DrawRectangle(i*20+3,k*20+3,16,16,hexcolor);
            }
        }
    }

    //reset sleep time for new block
    speedSleep = sleepTime;

    //reset all flags
    left = false;
    right = false;
    drop = false;
    if(start_rotate_flag){
        GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
        //renable interrupt
        GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
        start_rotate_flag = false;
    }

    char stringy[20];
    sprintf(stringy, "%d",blockNum);
    G8RTOS_AddThread(blockThread,1,stringy);
    G8RTOS_KillSelf();
}
void gameOver(void){
    int16_t colors[7] = {0xF800, 0xFD60, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0x991D};
    uint8_t scrl = (uint8_t)((score % 10) + '0');
    uint8_t div = score/10;
    uint8_t scrh = (uint8_t)((div % 10) + '0');
    //print score
    LCD_Text(100,220,"SCORE: ",0xFFFF);
    if(score > 9){
        PutChar(150,220,scrh,0xFFFF);
        PutChar(160,220,scrl,0xFFFF);
    }
    else{
        PutChar(150,220,scrl,0xFFFF);
    }
   // uint8_t scr  = (uint8_t)(score + '0');
    //print current level
    LCD_Text(100,240,"LEVEL: ",0xFFFF);
    uint8_t lvl  = (uint8_t)(level + '0');
    PutChar(150,240,lvl,0xFFFF);
    if(level != 9){
        level++;
        lvl  = (uint8_t)(level + '0');
        LCD_Text(4,280,"Press B1 to move on to Level: ",0xFFFF);
        PutChar(120,300,lvl,0xFFFF);
    }
    else{
        level++;
        LCD_Text(8,270,"You've played all 9 levels!",0xFFFF);
        LCD_Text(30,290,"Press B1 to restart",0xFFFF);
    }


    if(sleepTime > 50){  //make blocks move faster with every level
        sleepTime -= 50;  //10
    }
    speedSleep = sleepTime;
    if(gameColor == 6){
        gameColor = 0;
    }
    else{
        gameColor++;
    }

    while(1){
        if(start_rotate_flag && level < 10){  //9 is highest level
            //clear screen
            LCD_Text(100,220,"SCORE: ",0);

            if(score > 9){
                PutChar(150,220,scrh,0);
                PutChar(160,220,scrl,0);
            }
            else{
                PutChar(150,220,scrl,0);
            }
            //print current level
            LCD_Text(100,240,"LEVEL: ",0);
            uint8_t lvl2  = (uint8_t)(level-1 + '0');
            PutChar(150,240,lvl2,0);
            LCD_Text(4,280,"Press B1 to move on to Level: ",0);
            PutChar(120,300,lvl,0);
            LCD_DrawRectangle(60,20,124,202,0);
            //debounce
            sleep(500);
            //clear interrupt flag
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            start_rotate_flag = false;
            //spawn check thread
            G8RTOS_AddThread(InitGame,0,"game board init");
            //kill self
            G8RTOS_KillSelf();
        }
        else if(start_rotate_flag && level == 10){
            LCD_Clear(0);
            G8RTOS_AddThread(waitStartRestart,0,"waitB0");
            //clear interrupt flag
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            start_rotate_flag = false;
            G8RTOS_KillSelf();
        }
        else{
            //srand(SystemTime);
            //int num = (rand() % (6 - 0 + 1)) + 0;
            int i,j;
            for(i = 0; i < 200; i++){
                for(j = 0; j < 122; j++){
                    if(GAMEOVERRESIZE_IMAGE[i+j*200] == 0){
                        LCD_SetPoint(i+20, j+60, colors[gameColor]);
                    }
                }
            }
            if(gameColor == 6){
                gameColor = 0;
            }
            else{
                gameColor++;
            }
        }
        sleep(500);
    }

}
void LCDprint(void){
  //  threadIDs[0] = G8RTOS_GetThreadId();
    while(1){
        //wait for fifo to indicate a change to print (1 means down change, 2 means side to side change, 3 means rotate change)
        int32_t changeVal = readFIFO(CHANGE_FIFO); //will wait here until another thread has updated block position
        G8RTOS_WaitSemaphore(activeMutex);
        G8RTOS_WaitSemaphore(LCDMutex);
        //clear past block spot
        LCD_DrawRectangle(a.past_location[0].y*20+3,a.past_location[0].x*20+3,16,16,0);
        LCD_DrawRectangle(a.past_location[1].y*20+3,a.past_location[1].x*20+3,16,16,0);
        LCD_DrawRectangle(a.past_location[2].y*20+3,a.past_location[2].x*20+3,16,16,0);
        LCD_DrawRectangle(a.past_location[3].y*20+3,a.past_location[3].x*20+3,16,16,0);

        if(changeVal != 1){  //dest doesn't change when moving down
            //draw over past dest location
           LCD_DrawRectangle(a.dest_location[0].y*20+3,a.dest_location[0].x*20+3,16,16,0);
           LCD_DrawRectangle(a.dest_location[1].y*20+3,a.dest_location[1].x*20+3,16,16,0);
           LCD_DrawRectangle(a.dest_location[2].y*20+3,a.dest_location[2].x*20+3,16,16,0);
           LCD_DrawRectangle(a.dest_location[3].y*20+3,a.dest_location[3].x*20+3,16,16,0);

            //update dest location
           int16_t temp0y = a.current_location[0].y;
           int16_t temp1y = a.current_location[1].y;
           int16_t temp2y = a.current_location[2].y;
           int16_t temp3y = a.current_location[3].y;
           int16_t temp0x = a.current_location[0].x;
           int16_t temp1x = a.current_location[1].x;
           int16_t temp2x = a.current_location[2].x;
           int16_t temp3x = a.current_location[3].x;
           G8RTOS_WaitSemaphore(matrixMutex);

           while(1){
               if(matrix[temp0y+1][temp0x] != BLACKB || matrix[temp1y+1][temp1x] != BLACKB || matrix[temp2y+1][temp2x] != BLACKB || matrix[temp3y+1][temp3x] != BLACKB || temp0y == 15 || temp1y == 15 || temp2y == 15 || temp3y == 15){
                   break;
               }
               temp0y++;
               temp1y++;
               temp2y++;
               temp3y++;
           }
           G8RTOS_SignalSemaphore(matrixMutex);
           //update dest location of block
            a.dest_location[0].x = temp0x;
            a.dest_location[0].y = temp0y;
            a.dest_location[1].x = temp1x;
            a.dest_location[1].y = temp1y;
            a.dest_location[2].x = temp2x;
            a.dest_location[2].y = temp2y;
            a.dest_location[3].x = temp3x;
            a.dest_location[3].y = temp3y;
            //draw new dest location
            //print destination location shadow
            LCD_DrawRectangle(a.dest_location[0].y*20+3,a.dest_location[0].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[1].y*20+3,a.dest_location[1].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[2].y*20+3,a.dest_location[2].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[3].y*20+3,a.dest_location[3].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[0].y*20+4,a.dest_location[0].x*20+4,14,14,0);
            LCD_DrawRectangle(a.dest_location[1].y*20+4,a.dest_location[1].x*20+4,14,14,0);
            LCD_DrawRectangle(a.dest_location[2].y*20+4,a.dest_location[2].x*20+4,14,14,0);
            LCD_DrawRectangle(a.dest_location[3].y*20+4,a.dest_location[3].x*20+4,14,14,0);
        }
         //draw new block location
         int16_t c = getColorVal(a.color);
         LCD_DrawRectangle(a.current_location[0].y*20+3,a.current_location[0].x*20+3,16,16,c);
         LCD_DrawRectangle(a.current_location[1].y*20+3,a.current_location[1].x*20+3,16,16,c);
         LCD_DrawRectangle(a.current_location[2].y*20+3,a.current_location[2].x*20+3,16,16,c);
         LCD_DrawRectangle(a.current_location[3].y*20+3,a.current_location[3].x*20+3,16,16,c);
         G8RTOS_SignalSemaphore(LCDMutex);
         //set drawn block location as past location for next run
        a.past_location[0].x = a.current_location[0].x;
        a.past_location[0].y = a.current_location[0].y;
        a.past_location[1].x = a.current_location[1].x;
        a.past_location[1].y = a.current_location[1].y;
        a.past_location[2].x = a.current_location[2].x;
        a.past_location[2].y = a.current_location[2].y;
        a.past_location[3].x = a.current_location[3].x;
        a.past_location[3].y = a.current_location[3].y;
        G8RTOS_SignalSemaphore(activeMutex);
    }
}
void downMove(void){
    bool done = false;
    while(1){
        G8RTOS_WaitSemaphore(activeMutex);
        if(a.current_location[0].y == 15 || a.current_location[1].y == 15 || a.current_location[2].y == 15 || a.current_location[3].y == 15){
            done = true;
        }
        else{
            G8RTOS_WaitSemaphore(matrixMutex);
            int b;
            for(b = 0; b < 4; b++){ //check each block location moved down one
                 if(matrix[a.current_location[b].y+1][a.current_location[b].x] != BLACKB){  //overlaps
                     done = true;
                     break;
                 }
            }
            G8RTOS_SignalSemaphore(matrixMutex);
        }
        if(done){  //if done, add to global matrix, spawn check thread, then kill self
            G8RTOS_WaitSemaphore(LCDMutex);  //make sure lcd finishes drawing before finishing block round
            G8RTOS_WaitSemaphore(matrixMutex);
            matrix[a.current_location[0].y][a.current_location[0].x] = a.color;
            matrix[a.current_location[1].y][a.current_location[1].x] = a.color;
            matrix[a.current_location[2].y][a.current_location[2].x] = a.color;
            matrix[a.current_location[3].y][a.current_location[3].x] = a.color;
            G8RTOS_SignalSemaphore(matrixMutex);  //take out?
            G8RTOS_SignalSemaphore(activeMutex); //take out?
            G8RTOS_KillThread(threadIDs[0]);
            G8RTOS_KillThread(threadIDs[1]);
            G8RTOS_KillThread(threadIDs[2]);
            G8RTOS_AddThread(checkThread,1,"check thread");
            matrixMutex = &matrixSP;
            G8RTOS_InitSemaphore(matrixMutex,1);
            activeMutex = &activeT;
            G8RTOS_InitSemaphore(activeMutex,1);
            LCDMutex = &LCD_disp;
            G8RTOS_InitSemaphore(LCDMutex,1);
            int changefifo = G8RTOS_InitFIFO(CHANGE_FIFO);
            G8RTOS_KillSelf();
        }
        else{ //if not done, update location
            a.current_location[0].y += 1;
            a.current_location[1].y += 1;
            a.current_location[2].y += 1;
            a.current_location[3].y += 1;
            a.rotate_location[0].y += 1;
            a.rotate_location[1].y += 1;
            a.rotate_location[2].y += 1;
            a.rotate_location[3].y += 1;
            a.rot3[0].y += 1;
            a.rot3[1].y += 1;
            a.rot3[2].y += 1;
            a.rot3[3].y += 1;
            a.rot4[0].y += 1;
            a.rot4[1].y += 1;
            a.rot4[2].y += 1;
            a.rot4[3].y += 1;

            G8RTOS_SignalSemaphore(activeMutex);
            writeFIFO(CHANGE_FIFO, 1);
        }
       // G8RTOS_SignalSemaphore(activeMutex);
        sleep(speedSleep);
    }
}
void sideMove(void){
   // threadIDs[1] = G8RTOS_GetThreadId();
    while(1){
        G8RTOS_WaitSemaphore(activeMutex);
        //check if either button has been pressed
        if(left
                && a.current_location[0].x != 0
                && a.current_location[1].x != 0
                && a.current_location[2].x != 0
                && a.current_location[3].x != 0){
            G8RTOS_WaitSemaphore(matrixMutex);
            if(matrix[a.current_location[0].y][a.current_location[0].x-1] == BLACKB
                    && matrix[a.current_location[1].y][a.current_location[1].x-1] == BLACKB
                    && matrix[a.current_location[2].y][a.current_location[2].x-1] == BLACKB
                    && matrix[a.current_location[3].y][a.current_location[3].x-1] == BLACKB){
                G8RTOS_SignalSemaphore(matrixMutex);
                a.current_location[0].x -= 1;
                a.current_location[1].x -= 1;
                a.current_location[2].x -= 1;
                a.current_location[3].x -= 1;
                a.rotate_location[0].x -= 1;
                a.rotate_location[1].x -= 1;
                a.rotate_location[2].x -= 1;
                a.rotate_location[3].x -= 1;
                a.rot3[0].x -= 1;
                a.rot3[1].x -= 1;
                a.rot3[2].x -= 1;
                a.rot3[3].x -= 1;
                a.rot4[0].x -= 1;
                a.rot4[1].x -= 1;
                a.rot4[2].x -= 1;
                a.rot4[3].x -= 1;
                G8RTOS_SignalSemaphore(activeMutex);
                left = false;
                writeFIFO(CHANGE_FIFO, 2);
            }
            else{
                G8RTOS_SignalSemaphore(matrixMutex);
                G8RTOS_SignalSemaphore(activeMutex);
            }
           // G8RTOS_SignalSemaphore(matrixMutex);
        }
        else if(right
                && a.current_location[0].x != 9
                && a.current_location[1].x != 9
                && a.current_location[2].x != 9
                && a.current_location[3].x != 9){
            G8RTOS_WaitSemaphore(matrixMutex);
            if(matrix[a.current_location[0].y][a.current_location[0].x+1] == BLACKB
                    && matrix[a.current_location[1].y][a.current_location[1].x+1] == BLACKB
                    && matrix[a.current_location[2].y][a.current_location[2].x+1] == BLACKB
                    && matrix[a.current_location[3].y][a.current_location[3].x+1] == BLACKB){
                G8RTOS_SignalSemaphore(matrixMutex);
                a.current_location[0].x += 1;
                a.current_location[1].x += 1;
                a.current_location[2].x += 1;
                a.current_location[3].x += 1;
                a.rotate_location[0].x += 1;
                a.rotate_location[1].x += 1;
                a.rotate_location[2].x += 1;
                a.rotate_location[3].x += 1;
                a.rot3[0].x += 1;
                a.rot3[1].x += 1;
                a.rot3[2].x += 1;
                a.rot3[3].x += 1;
                a.rot4[0].x += 1;
                a.rot4[1].x += 1;
                a.rot4[2].x += 1;
                a.rot4[3].x += 1;
                G8RTOS_SignalSemaphore(activeMutex);
                right = false;
                writeFIFO(CHANGE_FIFO, 2);
            }
            else{
                G8RTOS_SignalSemaphore(matrixMutex);
                G8RTOS_SignalSemaphore(activeMutex);
            }
        }
        else{
            G8RTOS_SignalSemaphore(activeMutex);
        }
        sleep(50);
    }
}
void rotateMove(void){
   // threadIDs[2] = G8RTOS_GetThreadId();
    while(1){
        //update rotate location of block
        if(start_rotate_flag){
            G8RTOS_WaitSemaphore(activeMutex);
            if(a.rotate_location[0].x <= 9
                    && a.rotate_location[1].x <= 9
                    && a.rotate_location[2].x <= 9
                    && a.rotate_location[3].x <= 9
                    && a.rotate_location[0].x >= 0
                    && a.rotate_location[1].x >= 0
                    && a.rotate_location[2].x >= 0
                    && a.rotate_location[3].x >= 0){
                G8RTOS_WaitSemaphore(matrixMutex);
                if(matrix[a.rotate_location[0].y][a.rotate_location[0].x] == BLACKB
                    && matrix[a.rotate_location[1].y][a.rotate_location[1].x] == BLACKB
                    && matrix[a.rotate_location[2].y][a.rotate_location[2].x] == BLACKB
                    && matrix[a.rotate_location[3].y][a.rotate_location[3].x] == BLACKB){
                    G8RTOS_SignalSemaphore(matrixMutex);
                    //replace current location and rotate location
                        int16_t temp0x = a.current_location[0].x;
                        int16_t temp0y = a.current_location[0].y;
                        int16_t temp1x = a.current_location[1].x;
                        int16_t temp1y = a.current_location[1].y;
                        int16_t temp2x = a.current_location[2].x;
                        int16_t temp2y = a.current_location[2].y;
                        int16_t temp3x = a.current_location[3].x;
                        int16_t temp3y = a.current_location[3].y;
                        a.current_location[0].x = a.rotate_location[0].x;
                        a.current_location[1].x = a.rotate_location[1].x;
                        a.current_location[2].x = a.rotate_location[2].x;
                        a.current_location[3].x = a.rotate_location[3].x;
                        a.current_location[0].y = a.rotate_location[0].y;
                        a.current_location[1].y = a.rotate_location[1].y;
                        a.current_location[2].y = a.rotate_location[2].y;
                        a.current_location[3].y = a.rotate_location[3].y;
                        a.rotate_location[0].x = a.rot3[0].x;
                        a.rotate_location[1].x = a.rot3[1].x;
                        a.rotate_location[2].x = a.rot3[2].x;
                        a.rotate_location[3].x = a.rot3[3].x;
                        a.rotate_location[0].y = a.rot3[0].y;
                        a.rotate_location[1].y = a.rot3[1].y;
                        a.rotate_location[2].y = a.rot3[2].y;
                        a.rotate_location[3].y = a.rot3[3].y;
                        a.rot3[0].x = a.rot4[0].x;
                        a.rot3[1].x = a.rot4[1].x;
                        a.rot3[2].x = a.rot4[2].x;
                        a.rot3[3].x = a.rot4[3].x;
                        a.rot3[0].y = a.rot4[0].y;
                        a.rot3[1].y = a.rot4[1].y;
                        a.rot3[2].y = a.rot4[2].y;
                        a.rot3[3].y = a.rot4[3].y;
                        a.rot4[0].x = temp0x;
                        a.rot4[1].x = temp1x;
                        a.rot4[2].x = temp2x;
                        a.rot4[3].x = temp3x;
                        a.rot4[0].y = temp0y;
                        a.rot4[1].y = temp1y;
                        a.rot4[2].y = temp2y;
                        a.rot4[3].y = temp3y;
                        G8RTOS_SignalSemaphore(activeMutex);
                        writeFIFO(CHANGE_FIFO, 3);
                }
                else{
                    G8RTOS_SignalSemaphore(matrixMutex);
                    G8RTOS_SignalSemaphore(activeMutex);
                }
            }
            else{
                G8RTOS_SignalSemaphore(activeMutex);
            }
            //debounce
            sleep(500);
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            start_rotate_flag = false;
        }
        else if(drop){
                speedSleep = 10;
                drop = false;
        }

        sleep(50);
    }
}

void blockThread(void){
    BLOCK currentBlock;
    switch(nextBlock) {
       case YELLOWB :
          currentBlock.current_location[0].x = 5;
          currentBlock.current_location[0].y = 0;
          currentBlock.current_location[1].x = 4;
          currentBlock.current_location[1].y = 0;
          currentBlock.current_location[2].x = 5;
          currentBlock.current_location[2].y = 1;
          currentBlock.current_location[3].x = 4;
          currentBlock.current_location[3].y = 1;
          currentBlock.rotate_location[0].x = 5;  //yellow has same rotate position
          currentBlock.rotate_location[0].y = 0;
          currentBlock.rotate_location[1].x = 4;
          currentBlock.rotate_location[1].y = 0;
          currentBlock.rotate_location[2].x = 5;
          currentBlock.rotate_location[2].y = 1;
          currentBlock.rotate_location[3].x = 4;
          currentBlock.rotate_location[3].y = 1;
          currentBlock.rot3[0].x = 5;  //yellow has same rotate position
          currentBlock.rot3[0].y = 0;
          currentBlock.rot3[1].x = 4;
          currentBlock.rot3[1].y = 0;
          currentBlock.rot3[2].x = 5;
          currentBlock.rot3[2].y = 1;
          currentBlock.rot3[3].x = 4;
          currentBlock.rot3[3].y = 1;
          currentBlock.rot4[0].x = 5;  //yellow has same rotate position
          currentBlock.rot4[0].y = 0;
          currentBlock.rot4[1].x = 4;
          currentBlock.rot4[1].y = 0;
          currentBlock.rot4[2].x = 5;
          currentBlock.rot4[2].y = 1;
          currentBlock.rot4[3].x = 4;
          currentBlock.rot4[3].y = 1;
          currentBlock.past_location[0].x = 5;
          currentBlock.past_location[0].y = 0;
          currentBlock.past_location[1].x = 4;
          currentBlock.past_location[1].y = 0;
          currentBlock.past_location[2].x = 5;
          currentBlock.past_location[2].y = 1;
          currentBlock.past_location[3].x = 4;
          currentBlock.past_location[3].y = 1;
          currentBlock.color = YELLOWB;
          break;
       case GREENB :
           currentBlock.current_location[0].x = 4;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 4;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 1;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 2;
           currentBlock.rotate_location[0].x = 5;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 1;
           currentBlock.rotate_location[3].x = 3;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 1;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 1;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 2;
           currentBlock.rot4[0].x = 5;  //rotate position  4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 4;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 3;
           currentBlock.rot4[3].y = 1;
           currentBlock.past_location[0].x = 4;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 4;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 1;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 2;
           currentBlock.color = GREENB;
          break;
       case PURPLEB :
           currentBlock.current_location[0].x = 4;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 4;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 4;
           currentBlock.current_location[2].y = 2;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 1;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 5;
           currentBlock.rotate_location[2].y = 0;
           currentBlock.rotate_location[3].x = 4;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 1;
           currentBlock.rot3[1].x = 5;
           currentBlock.rot3[1].y = 0;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 1;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 2;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 1;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 1;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 4;
           currentBlock.rot4[3].y = 0;
           currentBlock.past_location[0].x = 4;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 4;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 4;
           currentBlock.past_location[2].y = 2;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 1;
           currentBlock.color = PURPLEB;  //find actual
          break;
       case DBLUEB :
           currentBlock.current_location[0].x = 5;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 5;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 2;
           currentBlock.current_location[3].x = 4;
           currentBlock.current_location[3].y = 2;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 3;
           currentBlock.rotate_location[1].y = 1;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 1;
           currentBlock.rotate_location[3].x = 5;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 1;
           currentBlock.rot3[2].x = 4;
           currentBlock.rot3[2].y = 2;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 0;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 0;
           currentBlock.rot4[3].x = 5;
           currentBlock.rot4[3].y = 1;
           currentBlock.past_location[0].x = 5;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 5;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 2;
           currentBlock.past_location[3].x = 4;
           currentBlock.past_location[3].y = 2;
           currentBlock.color = DBLUEB;
          break;
       case LBLUEB :
           currentBlock.current_location[0].x = 5;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 5;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 2;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 3;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 5;
           currentBlock.rotate_location[2].y = 0;
           currentBlock.rotate_location[3].x = 6;
           currentBlock.rotate_location[3].y = 0;
           currentBlock.rot3[0].x = 5;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 5;
           currentBlock.rot3[1].y = 1;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 2;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 3;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 0;
           currentBlock.rot4[3].x = 6;
           currentBlock.rot4[3].y = 0;
           currentBlock.past_location[0].x = 5;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 5;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 2;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 3;
           currentBlock.color = LBLUEB;
          break;
       case REDB :
           currentBlock.current_location[0].x = 5;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 5;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 4;
           currentBlock.current_location[2].y = 1;
           currentBlock.current_location[3].x = 4;
           currentBlock.current_location[3].y = 2;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 1;
           currentBlock.rotate_location[3].x = 5;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 1;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 2;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 0;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 1;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 4;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 5;
           currentBlock.rot4[3].y = 1;
           currentBlock.past_location[0].x = 5;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 5;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 4;
           currentBlock.past_location[2].y = 1;
           currentBlock.past_location[3].x = 4;
           currentBlock.past_location[3].y = 2;
           currentBlock.color = REDB;
           break;
       case ORANGEB :
           currentBlock.current_location[0].x = 3;
           currentBlock.current_location[0].y = 1;
           currentBlock.current_location[1].x = 4;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 1;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 0;
           currentBlock.rotate_location[0].x = 4;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 1;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 2;
           currentBlock.rotate_location[3].x = 5;
           currentBlock.rotate_location[3].y = 2;
           currentBlock.rot3[0].x = 3;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 0;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 0;
           currentBlock.rot3[3].x = 3;
           currentBlock.rot3[3].y = 1;
           currentBlock.rot4[0].x = 4;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 5;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 5;
           currentBlock.rot4[3].y = 2;
           currentBlock.past_location[0].x = 3;
           currentBlock.past_location[0].y = 1;
           currentBlock.past_location[1].x = 4;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 1;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 0;
           currentBlock.color = ORANGEB;
           break;
       default :
           break;
    }
    //select random block color to be next
    nextBlock = nextnextBlock;
    nextnextBlock = nextnextnextBlock;
    nextnextnextBlock = getRandColor(SystemTime,nextnextBlock);
    //nextnextnextBlock = YELLOWB;
    printLittleNext(1,true);
    printLittleNext(1,false);
    printLittleNext(2,true);
    printLittleNext(2,false);
    printLittleNext(3,true);
    printLittleNext(3,false);


    a = currentBlock;

    //draw new block location
    int16_t c = getColorVal(a.color);
    LCD_DrawRectangle(a.current_location[0].y*20+3,a.current_location[0].x*20+3,16,16,c);
    LCD_DrawRectangle(a.current_location[1].y*20+3,a.current_location[1].x*20+3,16,16,c);
    LCD_DrawRectangle(a.current_location[2].y*20+3,a.current_location[2].x*20+3,16,16,c);
    LCD_DrawRectangle(a.current_location[3].y*20+3,a.current_location[3].x*20+3,16,16,c);

    //update dest location
    int16_t temp0y = a.current_location[0].y;
    int16_t temp1y = a.current_location[1].y;
    int16_t temp2y = a.current_location[2].y;
    int16_t temp3y = a.current_location[3].y;
    int16_t temp0x = a.current_location[0].x;
    int16_t temp1x = a.current_location[1].x;
    int16_t temp2x = a.current_location[2].x;
    int16_t temp3x = a.current_location[3].x;
    while(1){
        if(matrix[temp0y+1][temp0x] != BLACKB || matrix[temp1y+1][temp1x] != BLACKB || matrix[temp2y+1][temp2x] != BLACKB || matrix[temp3y+1][temp3x] != BLACKB || temp0y == 15 || temp1y == 15 || temp2y == 15 || temp3y == 15){
            break;
        }
        temp0y++;
        temp1y++;
        temp2y++;
        temp3y++;
    }
    //update dest location of block
     a.dest_location[0].x = temp0x;
     a.dest_location[0].y = temp0y;
     a.dest_location[1].x = temp1x;
     a.dest_location[1].y = temp1y;
     a.dest_location[2].x = temp2x;
     a.dest_location[2].y = temp2y;
     a.dest_location[3].x = temp3x;
     a.dest_location[3].y = temp3y;
     //draw new dest location
     //print destination location shadow
     LCD_DrawRectangle(a.dest_location[0].y*20+3,a.dest_location[0].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[1].y*20+3,a.dest_location[1].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[2].y*20+3,a.dest_location[2].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[3].y*20+3,a.dest_location[3].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[0].y*20+4,a.dest_location[0].x*20+4,14,14,0);
     LCD_DrawRectangle(a.dest_location[1].y*20+4,a.dest_location[1].x*20+4,14,14,0);
     LCD_DrawRectangle(a.dest_location[2].y*20+4,a.dest_location[2].x*20+4,14,14,0);
     LCD_DrawRectangle(a.dest_location[3].y*20+4,a.dest_location[3].x*20+4,14,14,0);

    // sleep(sleepTime);

    //spawn game threads
    threadIDs[0] = threadTotal;
    G8RTOS_AddThread(LCDprint,0,"lcd print");
    G8RTOS_AddThread(downMove,1,"down move");
    threadIDs[1] = threadTotal;
    G8RTOS_AddThread(sideMove,1,"side move");
    threadIDs[2] = threadTotal;
    G8RTOS_AddThread(rotateMove,1,"rotate move");
    first = true;



    G8RTOS_KillSelf();
}

enum block_color getRandColor(int seed, enum block_color last){
   // return YELLOWB; //
    srand(seed);
    int num = (rand() % (6 - 0 + 1)) + 0;
    switch(num) {
       case 0 :
          return ORANGEB;
       case 1 :
           return GREENB;
       case 2 :
           return PURPLEB;
       case 3 :
           return DBLUEB;
       case 4 :
           return LBLUEB;
       case 5 :
           return REDB;
       case 6 :
           return YELLOWB;
       default :
           return YELLOWB;
    }
}
int16_t getColorVal(enum block_color c){
    switch(c) {
            case ORANGEB :
                return 0xFD60;
            case GREENB :
                return 0x07E0;
            case PURPLEB :
                return 0x991D;
            case DBLUEB :
                return 0x001F;
            case LBLUEB :
                return 0x07FF;
            case REDB :
                return 0xF800;
            case YELLOWB :
                return 0xFFE0;
            case BLACKB :
                return 0;
            default :
                return 0;
    }
}

void printLittleNext(int i, bool clear){
    int x1,y1,x2,y2,x3,y3,x4,y4;
    int16_t color = 0;
    enum block_color blockInQuestion = BLACKB;
    if(i == 1){
        blockInQuestion = nextBlock;
    }
    else if(i == 2){
        blockInQuestion = nextnextBlock;
    }
    else if(i == 3){
        blockInQuestion = nextnextnextBlock;
    }
    switch(blockInQuestion) {
       case YELLOWB :
           x1 = 8;
           y1 = 8;
           x2 = 16;
           y2 = 8;
           x3 = 8;
           y3 = 16;
           x4 = 16;
           y4 = 16;
           color = 0xFFE0;
           break;
       case GREENB :
           x1 = 0;
           y1 = 8;
           x2 = 0;
           y2 = 16;
           x3 = 8;
           y3 = 16;
           x4 = 8;
           y4 = 24;
           color = 0x07E0;
           break;
       case PURPLEB :
           x1 = 8;
           y1 = 0;
           x2 = 8;
           y2 = 8;
           x3 = 8;
           y3 = 16;
           x4 = 16;
           y4 = 8;
           color = 0x991D;
           break;
       case DBLUEB :
           x1 = 16;
           y1 = 0;
           x2 = 16;
           y2 = 8;
           x3 = 16;
           y3 = 16;
           x4 = 8;
           y4 = 16;
           color = 0x001F;
           break;
       case LBLUEB :
           x1 = 8;
           y1 = 0;
           x2 = 8;
           y2 = 8;
           x3 = 8;
           y3 = 16;
           x4 = 8;
           y4 = 24;
           color = 0x07FF;
           break;
       case REDB :
           x1 = 16;
           y1 = 0;
           x2 = 16;
           y2 = 8;
           x3 = 8;
           y3 = 8;
           x4 = 8;
           y4 = 16;
           color = 0xF800;
           break;
       case ORANGEB :
           x1 = 16;
           y1 = 8;
           x2 = 16;
           y2 = 16;
           x3 = 8;
           y3 = 16;
           x4 = 0;
           y4 = 16;
           color = 0xFD60;
           break;
       default :
           x1 = 0;
           x2 = 0;
           x3 = 0;
           x4 = 0;
           y1 = 0;
           y2 = 0;
           y3 = 0;
           y4 = 0;
           color = 0;
    }
    if(clear){
        if(i == 1){
           LCD_DrawRectangle(30,211,32,22,0);
        }
        else if(i == 2){
            LCD_DrawRectangle(80,211,32,22,0);
        }
        else if(i == 3){
            LCD_DrawRectangle(130,211,32,22,0);
        }
    }
    else{
        if(i == 1){
            LCD_DrawRectangle(30+y1,211+x1,6,6,color);
            LCD_DrawRectangle(30+y2,211+x2,6,6,color);
            LCD_DrawRectangle(30+y3,211+x3,6,6,color);
            LCD_DrawRectangle(30+y4,211+x4,6,6,color);
        }
        else if(i == 2){
            LCD_DrawRectangle(80+y1,211+x1,6,6,color);
            LCD_DrawRectangle(80+y2,211+x2,6,6,color);
            LCD_DrawRectangle(80+y3,211+x3,6,6,color);
            LCD_DrawRectangle(80+y4,211+x4,6,6,color);
        }
        else if(i == 3){
            LCD_DrawRectangle(130+y1,211+x1,6,6,color);
            LCD_DrawRectangle(130+y2,211+x2,6,6,color);
            LCD_DrawRectangle(130+y3,211+x3,6,6,color);
            LCD_DrawRectangle(130+y4,211+x4,6,6,color);
        }
    }


}
=======
/**
 * thread.c
 * @author:
 * uP2 - Fall 2022
 */

// your includes and defines
#include <FinalProject_Tetris/G8RTOS_Semaphores.h>
#include <FinalProject_Tetris/G8RTOS_Structures.h>
#include <FinalProject_Tetris/G8RTOS_IPC.h>
#include <FinalProject_Tetris/G8RTOS_Scheduler.h>
#include "BoardSupport/inc/demo_sysctl.h"
#include "BoardSupport/inc/opt3001.h"
#include "BoardSupport/inc/bmi160_support.h"
#include "driverlib/sysctl.h"
#include "threads.h"
#include "BoardSupport/inc/RGBLedDriver.h"
#include <stdint.h>
#include "BoardSupport/inc/Joystick.h"
#include "BoardSupport/inc/ILI9341_Lib.h"
#include "driverlib/uart.h"
#include "utils/uartstdio.h"
#include <time.h>
#include <stdlib.h>

#include "driverlib/interrupt.h"
#include "inc/hw_ints.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "inc/hw_memmap.h"
#include <stdio.h>
#include <string.h>

#include "logo.h"




// your function implementations
void IdleThread(void){  //lowest priority
    while(1){
        //do nothing
    }
}
void LCDtap(void){  // B0 interrupt
          GPIOIntDisable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
          start_rotate_flag = true;
}
void waitStartRestart(void){
    bool on = true;
    gameColor = 0;
    LCD_Text(20,300,"Press B1 to start the game",0xFFFF);
    int i;  //145 across 100 down
    int j;
    for(i = 0; i < 200; i++){
        for(j = 0; j < 138; j++){
            if(i != 199){  //dont print last col
                LCD_SetPoint(i+20, j+60, LOGOBMPBIG2_IMAGE[i+j*200]);
            }
        }
    }
    sleepTime = 1000;  //500
    speedSleep = 1000;  //500
    level = 1;
    while(1){
        if(start_rotate_flag == true){
            //draw rectangle over start screen text
           // LCD_DrawRectangle(50,20,220,220,0);
            LCD_DrawRectangle(59,19,140,201,0);
            LCD_Text(20,300,"Press B1 to start the game",0);
            //debounce
            sleep(500);  //added
            //reset B1 flag
            start_rotate_flag = false;
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            //spawn check thread
            G8RTOS_AddThread(InitGame,0,"game board init");
            //kill self
            G8RTOS_KillSelf();
        }
        else{
            if(on){
                LCD_Text(20,300,"Press B1 to start the game",0);
                on = false;
            }
            else{
                LCD_Text(20,300,"Press B1 to start the game",0xFFFF);
                on = true;
            }
        }
        sleep(500);
    }
}
void InitGame(void){
    //print game board & starting attributes
   // LCD_DrawRectangle(15,0,2,220,0xF81F);  //vertical line
    LCD_Text(205,0,"NEXT",0xFFFF);
    LCD_Text(201,200,"SCORE",0xFFFF);
    LCD_Text(210,220,"0",0xFFFF);  //205,220
    LCD_Text(201,260,"LEVEL",0xFFFF);
    uint8_t lvl  = (uint8_t)(level + '0');
    PutChar(210,280,lvl,0xFFFF);

    nextBlock = getRandColor(SystemTime,BLACKB);
    nextnextBlock = getRandColor(SystemTime+1,nextBlock);
    nextnextnextBlock = getRandColor(SystemTime+2,nextnextBlock);
    printLittleNext(1,false);
    printLittleNext(2,false);
    printLittleNext(3,false);

    //Initialize globals
    score = 0;
    debounceL = 0;
    debounceR = 0;
    if(start_rotate_flag){
        GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
        //renable interrupt
        GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
        start_rotate_flag = false;
    }
    drop = false;
    left = false;
    right = false;
    drop = false;

    int i;
    int factor = 0;

    for(i = 0; i < 17; i++){
        LCD_DrawRectangle(factor,0,1,200,0x0004);  //horiz line
        factor += 20;
    }
    factor = 0;
    for(i = 0; i < 11; i++){
        LCD_DrawRectangle(0,factor,1000,1,0x0004);  //vertical line
        factor += 20;
    }
    //initialize global matrix to all black (block aren't added to this until dropped)
    int j;
    for(i = 0; i < SCREEN_LENGTH; i++){
        for(j = 0; j < SCREEN_WIDTH; j++){
            matrix[i][j] = BLACKB;
        }
    }

    matrixMutex = &matrixSP;
    G8RTOS_InitSemaphore(matrixMutex,1);
    activeMutex = &activeT;
    G8RTOS_InitSemaphore(activeMutex,1);
    LCDMutex = &LCD_disp;
    G8RTOS_InitSemaphore(LCDMutex,1);
    int changefifo = G8RTOS_InitFIFO(CHANGE_FIFO);
    G8RTOS_AddThread(checkThread,1,"check thread");
    G8RTOS_KillSelf();
}
void checkThread(void){

   // prevmatrix[SCREEN_LENGTH][SCREEN_WIDTH] = matrix;
    //check game point condition (row can be deleted)
    bool deleted[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    bool updated[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    bool allblack[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};
    bool taken[SCREEN_LENGTH] = {false,false,false,false,false,false,false,false,false,false,false,false,false,false,false,false};

    int i, j, k;
    int count = 1;
    int clrcount = 0;
    for(i = SCREEN_LENGTH-1; i >= 0; i--){ //starting at bottom left corner
        for(j = 0; j < SCREEN_WIDTH; j++){
            if(matrix[i][j] != BLACKB){ //all filled in spaces
                if(j == SCREEN_WIDTH-1){ //last row in col
                    if(count == SCREEN_WIDTH){ //delete row
                        /*for(k = 0; k < SCREEN_WIDTH; k++){ //clear row in matrix
                            matrix[i][k] = BLACKB;
                        }*/
                        deleted[i] = true;
                        score+=5;
                        if(score < 10){
                            uint8_t scr  = (uint8_t)(score + '0');
                           // uint8_t* scr_ptr = &scr;
                            LCD_DrawRectangle(220,210,20,20,0);
                          //  LCD_Text(210,220,scr_ptr,0xFFFF);
                            PutChar(210,220,scr,0xFFFF);  //205,220
                        }
                        else if(score < 100){
                            uint8_t scrl = (uint8_t)((score % 10) + '0');
                            uint8_t div = score/10;
                            uint8_t scrh = (uint8_t)((div % 10) + '0');
                            LCD_DrawRectangle(220,210,20,20,0);
                            PutChar(210,220,scrh,0xFFFF);
                            PutChar(220,220,scrl,0xFFFF);
                        }
                        else{  //max score is 95
                            score-=5;
                        }
                    }
                    count = 0;
                }
                count++;
            }
            else{
                clrcount++;
                if(j == SCREEN_WIDTH-1){ //last row in col
                    if(clrcount == SCREEN_WIDTH){
                        allblack[i] = true;
                    }
                    clrcount = 0;
                }
            }
        }
        clrcount = 0;
        count = 1;
    }
   // int skip = 0;
    i = SCREEN_LENGTH-1;
    while(!deleted[i]){
        if(i == 0){
            break;  //no rows to delete
        }
        i--;
    }
    for(; i > 0; i--){  //move down rows in matrix
        if(allblack[i]){
            break;
        }
        else{
            int row = i-1;
            while(deleted[row] || taken[row]){
                if(row == 0){  //no rows were found to move down
                    break;
                }
                row--;
            }
            taken[row] = true;
            updated[row] = true;
            //copy contents of desired row and clear row
            for(k = 0; k < SCREEN_WIDTH; k++){
                matrix[i][k] = matrix[row][k];
                matrix[row][k] = BLACKB;
            }
            updated[i] = true;
        }
    }
    for(k = 0; k < SCREEN_WIDTH; k++){
        if(matrix[0][k] != BLACKB){
            //sleep(100);
            //game over
            LCD_Clear(0);
            G8RTOS_AddThread(gameOver,1,"game over");
            G8RTOS_KillSelf(); //?
        }
    }
    //reprint LCD rows from first deleted row until row of all black
    count = 0;
    for(i = SCREEN_LENGTH-1; i >= 0; i--){
        if(updated[i]){  //redraw updated rows
            for(k = 0; k < SCREEN_WIDTH; k++){
                int16_t hexcolor = getColorVal(matrix[i][k]);
                LCD_DrawRectangle(i*20+3,k*20+3,16,16,hexcolor);
            }
        }
    }

    //reset sleep time for new block
    speedSleep = sleepTime;

    //reset all flags
    left = false;
    right = false;
    drop = false;
    if(start_rotate_flag){
        GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
        //renable interrupt
        GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
        start_rotate_flag = false;
    }

    char stringy[20];
    sprintf(stringy, "%d",blockNum);
    G8RTOS_AddThread(blockThread,1,stringy);
    G8RTOS_KillSelf();
}
void gameOver(void){
    int16_t colors[7] = {0xF800, 0xFD60, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0x991D};
    uint8_t scrl = (uint8_t)((score % 10) + '0');
    uint8_t div = score/10;
    uint8_t scrh = (uint8_t)((div % 10) + '0');
    //print score
    LCD_Text(100,220,"SCORE: ",0xFFFF);
    if(score > 9){
        PutChar(150,220,scrh,0xFFFF);
        PutChar(160,220,scrl,0xFFFF);
    }
    else{
        PutChar(150,220,scrl,0xFFFF);
    }
   // uint8_t scr  = (uint8_t)(score + '0');
    //print current level
    LCD_Text(100,240,"LEVEL: ",0xFFFF);
    uint8_t lvl  = (uint8_t)(level + '0');
    PutChar(150,240,lvl,0xFFFF);
    if(level != 9){
        level++;
        lvl  = (uint8_t)(level + '0');
        LCD_Text(4,280,"Press B1 to move on to Level: ",0xFFFF);
        PutChar(120,300,lvl,0xFFFF);
    }
    else{
        level++;
        LCD_Text(8,270,"You've played all 9 levels!",0xFFFF);
        LCD_Text(30,290,"Press B1 to restart",0xFFFF);
    }


    if(sleepTime > 50){  //make blocks move faster with every level
        sleepTime -= 50;  //10
    }
    speedSleep = sleepTime;
    if(gameColor == 6){
        gameColor = 0;
    }
    else{
        gameColor++;
    }

    while(1){
        if(start_rotate_flag && level < 10){  //9 is highest level
            //clear screen
            LCD_Text(100,220,"SCORE: ",0);

            if(score > 9){
                PutChar(150,220,scrh,0);
                PutChar(160,220,scrl,0);
            }
            else{
                PutChar(150,220,scrl,0);
            }
            //print current level
            LCD_Text(100,240,"LEVEL: ",0);
            uint8_t lvl2  = (uint8_t)(level-1 + '0');
            PutChar(150,240,lvl2,0);
            LCD_Text(4,280,"Press B1 to move on to Level: ",0);
            PutChar(120,300,lvl,0);
            LCD_DrawRectangle(60,20,124,202,0);
            //debounce
            sleep(500);
            //clear interrupt flag
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            start_rotate_flag = false;
            //spawn check thread
            G8RTOS_AddThread(InitGame,0,"game board init");
            //kill self
            G8RTOS_KillSelf();
        }
        else if(start_rotate_flag && level == 10){
            LCD_Clear(0);
            G8RTOS_AddThread(waitStartRestart,0,"waitB0");
            //clear interrupt flag
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            start_rotate_flag = false;
            G8RTOS_KillSelf();
        }
        else{
            //srand(SystemTime);
            //int num = (rand() % (6 - 0 + 1)) + 0;
            int i,j;
            for(i = 0; i < 200; i++){
                for(j = 0; j < 122; j++){
                    if(GAMEOVERRESIZE_IMAGE[i+j*200] == 0){
                        LCD_SetPoint(i+20, j+60, colors[gameColor]);
                    }
                }
            }
            if(gameColor == 6){
                gameColor = 0;
            }
            else{
                gameColor++;
            }
        }
        sleep(500);
    }

}
void LCDprint(void){
  //  threadIDs[0] = G8RTOS_GetThreadId();
    while(1){
        //wait for fifo to indicate a change to print (1 means down change, 2 means side to side change, 3 means rotate change)
        int32_t changeVal = readFIFO(CHANGE_FIFO); //will wait here until another thread has updated block position
        G8RTOS_WaitSemaphore(activeMutex);
        G8RTOS_WaitSemaphore(LCDMutex);
        //clear past block spot
        LCD_DrawRectangle(a.past_location[0].y*20+3,a.past_location[0].x*20+3,16,16,0);
        LCD_DrawRectangle(a.past_location[1].y*20+3,a.past_location[1].x*20+3,16,16,0);
        LCD_DrawRectangle(a.past_location[2].y*20+3,a.past_location[2].x*20+3,16,16,0);
        LCD_DrawRectangle(a.past_location[3].y*20+3,a.past_location[3].x*20+3,16,16,0);

        if(changeVal != 1){  //dest doesn't change when moving down
            //draw over past dest location
           LCD_DrawRectangle(a.dest_location[0].y*20+3,a.dest_location[0].x*20+3,16,16,0);
           LCD_DrawRectangle(a.dest_location[1].y*20+3,a.dest_location[1].x*20+3,16,16,0);
           LCD_DrawRectangle(a.dest_location[2].y*20+3,a.dest_location[2].x*20+3,16,16,0);
           LCD_DrawRectangle(a.dest_location[3].y*20+3,a.dest_location[3].x*20+3,16,16,0);

            //update dest location
           int16_t temp0y = a.current_location[0].y;
           int16_t temp1y = a.current_location[1].y;
           int16_t temp2y = a.current_location[2].y;
           int16_t temp3y = a.current_location[3].y;
           int16_t temp0x = a.current_location[0].x;
           int16_t temp1x = a.current_location[1].x;
           int16_t temp2x = a.current_location[2].x;
           int16_t temp3x = a.current_location[3].x;
           G8RTOS_WaitSemaphore(matrixMutex);

           while(1){
               if(matrix[temp0y+1][temp0x] != BLACKB || matrix[temp1y+1][temp1x] != BLACKB || matrix[temp2y+1][temp2x] != BLACKB || matrix[temp3y+1][temp3x] != BLACKB || temp0y == 15 || temp1y == 15 || temp2y == 15 || temp3y == 15){
                   break;
               }
               temp0y++;
               temp1y++;
               temp2y++;
               temp3y++;
           }
           G8RTOS_SignalSemaphore(matrixMutex);
           //update dest location of block
            a.dest_location[0].x = temp0x;
            a.dest_location[0].y = temp0y;
            a.dest_location[1].x = temp1x;
            a.dest_location[1].y = temp1y;
            a.dest_location[2].x = temp2x;
            a.dest_location[2].y = temp2y;
            a.dest_location[3].x = temp3x;
            a.dest_location[3].y = temp3y;
            //draw new dest location
            //print destination location shadow
            LCD_DrawRectangle(a.dest_location[0].y*20+3,a.dest_location[0].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[1].y*20+3,a.dest_location[1].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[2].y*20+3,a.dest_location[2].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[3].y*20+3,a.dest_location[3].x*20+3,16,16,0xFFFF);
            LCD_DrawRectangle(a.dest_location[0].y*20+4,a.dest_location[0].x*20+4,14,14,0);
            LCD_DrawRectangle(a.dest_location[1].y*20+4,a.dest_location[1].x*20+4,14,14,0);
            LCD_DrawRectangle(a.dest_location[2].y*20+4,a.dest_location[2].x*20+4,14,14,0);
            LCD_DrawRectangle(a.dest_location[3].y*20+4,a.dest_location[3].x*20+4,14,14,0);
        }
         //draw new block location
         int16_t c = getColorVal(a.color);
         LCD_DrawRectangle(a.current_location[0].y*20+3,a.current_location[0].x*20+3,16,16,c);
         LCD_DrawRectangle(a.current_location[1].y*20+3,a.current_location[1].x*20+3,16,16,c);
         LCD_DrawRectangle(a.current_location[2].y*20+3,a.current_location[2].x*20+3,16,16,c);
         LCD_DrawRectangle(a.current_location[3].y*20+3,a.current_location[3].x*20+3,16,16,c);
         G8RTOS_SignalSemaphore(LCDMutex);
         //set drawn block location as past location for next run
        a.past_location[0].x = a.current_location[0].x;
        a.past_location[0].y = a.current_location[0].y;
        a.past_location[1].x = a.current_location[1].x;
        a.past_location[1].y = a.current_location[1].y;
        a.past_location[2].x = a.current_location[2].x;
        a.past_location[2].y = a.current_location[2].y;
        a.past_location[3].x = a.current_location[3].x;
        a.past_location[3].y = a.current_location[3].y;
        G8RTOS_SignalSemaphore(activeMutex);
    }
}
void downMove(void){
    bool done = false;
    while(1){
        G8RTOS_WaitSemaphore(activeMutex);
        if(a.current_location[0].y == 15 || a.current_location[1].y == 15 || a.current_location[2].y == 15 || a.current_location[3].y == 15){
            done = true;
        }
        else{
            G8RTOS_WaitSemaphore(matrixMutex);
            int b;
            for(b = 0; b < 4; b++){ //check each block location moved down one
                 if(matrix[a.current_location[b].y+1][a.current_location[b].x] != BLACKB){  //overlaps
                     done = true;
                     break;
                 }
            }
            G8RTOS_SignalSemaphore(matrixMutex);
        }
        if(done){  //if done, add to global matrix, spawn check thread, then kill self
            G8RTOS_WaitSemaphore(LCDMutex);  //make sure lcd finishes drawing before finishing block round
            G8RTOS_WaitSemaphore(matrixMutex);
            matrix[a.current_location[0].y][a.current_location[0].x] = a.color;
            matrix[a.current_location[1].y][a.current_location[1].x] = a.color;
            matrix[a.current_location[2].y][a.current_location[2].x] = a.color;
            matrix[a.current_location[3].y][a.current_location[3].x] = a.color;
            G8RTOS_SignalSemaphore(matrixMutex);  //take out?
            G8RTOS_SignalSemaphore(activeMutex); //take out?
            G8RTOS_KillThread(threadIDs[0]);
            G8RTOS_KillThread(threadIDs[1]);
            G8RTOS_KillThread(threadIDs[2]);
            G8RTOS_AddThread(checkThread,1,"check thread");
            matrixMutex = &matrixSP;
            G8RTOS_InitSemaphore(matrixMutex,1);
            activeMutex = &activeT;
            G8RTOS_InitSemaphore(activeMutex,1);
            LCDMutex = &LCD_disp;
            G8RTOS_InitSemaphore(LCDMutex,1);
            int changefifo = G8RTOS_InitFIFO(CHANGE_FIFO);
            G8RTOS_KillSelf();
        }
        else{ //if not done, update location
            a.current_location[0].y += 1;
            a.current_location[1].y += 1;
            a.current_location[2].y += 1;
            a.current_location[3].y += 1;
            a.rotate_location[0].y += 1;
            a.rotate_location[1].y += 1;
            a.rotate_location[2].y += 1;
            a.rotate_location[3].y += 1;
            a.rot3[0].y += 1;
            a.rot3[1].y += 1;
            a.rot3[2].y += 1;
            a.rot3[3].y += 1;
            a.rot4[0].y += 1;
            a.rot4[1].y += 1;
            a.rot4[2].y += 1;
            a.rot4[3].y += 1;

            G8RTOS_SignalSemaphore(activeMutex);
            writeFIFO(CHANGE_FIFO, 1);
        }
       // G8RTOS_SignalSemaphore(activeMutex);
        sleep(speedSleep);
    }
}
void sideMove(void){
   // threadIDs[1] = G8RTOS_GetThreadId();
    while(1){
        G8RTOS_WaitSemaphore(activeMutex);
        //check if either button has been pressed
        if(left
                && a.current_location[0].x != 0
                && a.current_location[1].x != 0
                && a.current_location[2].x != 0
                && a.current_location[3].x != 0){
            G8RTOS_WaitSemaphore(matrixMutex);
            if(matrix[a.current_location[0].y][a.current_location[0].x-1] == BLACKB
                    && matrix[a.current_location[1].y][a.current_location[1].x-1] == BLACKB
                    && matrix[a.current_location[2].y][a.current_location[2].x-1] == BLACKB
                    && matrix[a.current_location[3].y][a.current_location[3].x-1] == BLACKB){
                G8RTOS_SignalSemaphore(matrixMutex);
                a.current_location[0].x -= 1;
                a.current_location[1].x -= 1;
                a.current_location[2].x -= 1;
                a.current_location[3].x -= 1;
                a.rotate_location[0].x -= 1;
                a.rotate_location[1].x -= 1;
                a.rotate_location[2].x -= 1;
                a.rotate_location[3].x -= 1;
                a.rot3[0].x -= 1;
                a.rot3[1].x -= 1;
                a.rot3[2].x -= 1;
                a.rot3[3].x -= 1;
                a.rot4[0].x -= 1;
                a.rot4[1].x -= 1;
                a.rot4[2].x -= 1;
                a.rot4[3].x -= 1;
                G8RTOS_SignalSemaphore(activeMutex);
                left = false;
                writeFIFO(CHANGE_FIFO, 2);
            }
            else{
                G8RTOS_SignalSemaphore(matrixMutex);
                G8RTOS_SignalSemaphore(activeMutex);
            }
           // G8RTOS_SignalSemaphore(matrixMutex);
        }
        else if(right
                && a.current_location[0].x != 9
                && a.current_location[1].x != 9
                && a.current_location[2].x != 9
                && a.current_location[3].x != 9){
            G8RTOS_WaitSemaphore(matrixMutex);
            if(matrix[a.current_location[0].y][a.current_location[0].x+1] == BLACKB
                    && matrix[a.current_location[1].y][a.current_location[1].x+1] == BLACKB
                    && matrix[a.current_location[2].y][a.current_location[2].x+1] == BLACKB
                    && matrix[a.current_location[3].y][a.current_location[3].x+1] == BLACKB){
                G8RTOS_SignalSemaphore(matrixMutex);
                a.current_location[0].x += 1;
                a.current_location[1].x += 1;
                a.current_location[2].x += 1;
                a.current_location[3].x += 1;
                a.rotate_location[0].x += 1;
                a.rotate_location[1].x += 1;
                a.rotate_location[2].x += 1;
                a.rotate_location[3].x += 1;
                a.rot3[0].x += 1;
                a.rot3[1].x += 1;
                a.rot3[2].x += 1;
                a.rot3[3].x += 1;
                a.rot4[0].x += 1;
                a.rot4[1].x += 1;
                a.rot4[2].x += 1;
                a.rot4[3].x += 1;
                G8RTOS_SignalSemaphore(activeMutex);
                right = false;
                writeFIFO(CHANGE_FIFO, 2);
            }
            else{
                G8RTOS_SignalSemaphore(matrixMutex);
                G8RTOS_SignalSemaphore(activeMutex);
            }
        }
        else{
            G8RTOS_SignalSemaphore(activeMutex);
        }
        sleep(50);
    }
}
void rotateMove(void){
   // threadIDs[2] = G8RTOS_GetThreadId();
    while(1){
        //update rotate location of block
        if(start_rotate_flag){
            G8RTOS_WaitSemaphore(activeMutex);
            if(a.rotate_location[0].x <= 9
                    && a.rotate_location[1].x <= 9
                    && a.rotate_location[2].x <= 9
                    && a.rotate_location[3].x <= 9
                    && a.rotate_location[0].x >= 0
                    && a.rotate_location[1].x >= 0
                    && a.rotate_location[2].x >= 0
                    && a.rotate_location[3].x >= 0){
                G8RTOS_WaitSemaphore(matrixMutex);
                if(matrix[a.rotate_location[0].y][a.rotate_location[0].x] == BLACKB
                    && matrix[a.rotate_location[1].y][a.rotate_location[1].x] == BLACKB
                    && matrix[a.rotate_location[2].y][a.rotate_location[2].x] == BLACKB
                    && matrix[a.rotate_location[3].y][a.rotate_location[3].x] == BLACKB){
                    G8RTOS_SignalSemaphore(matrixMutex);
                    //replace current location and rotate location
                        int16_t temp0x = a.current_location[0].x;
                        int16_t temp0y = a.current_location[0].y;
                        int16_t temp1x = a.current_location[1].x;
                        int16_t temp1y = a.current_location[1].y;
                        int16_t temp2x = a.current_location[2].x;
                        int16_t temp2y = a.current_location[2].y;
                        int16_t temp3x = a.current_location[3].x;
                        int16_t temp3y = a.current_location[3].y;
                        a.current_location[0].x = a.rotate_location[0].x;
                        a.current_location[1].x = a.rotate_location[1].x;
                        a.current_location[2].x = a.rotate_location[2].x;
                        a.current_location[3].x = a.rotate_location[3].x;
                        a.current_location[0].y = a.rotate_location[0].y;
                        a.current_location[1].y = a.rotate_location[1].y;
                        a.current_location[2].y = a.rotate_location[2].y;
                        a.current_location[3].y = a.rotate_location[3].y;
                        a.rotate_location[0].x = a.rot3[0].x;
                        a.rotate_location[1].x = a.rot3[1].x;
                        a.rotate_location[2].x = a.rot3[2].x;
                        a.rotate_location[3].x = a.rot3[3].x;
                        a.rotate_location[0].y = a.rot3[0].y;
                        a.rotate_location[1].y = a.rot3[1].y;
                        a.rotate_location[2].y = a.rot3[2].y;
                        a.rotate_location[3].y = a.rot3[3].y;
                        a.rot3[0].x = a.rot4[0].x;
                        a.rot3[1].x = a.rot4[1].x;
                        a.rot3[2].x = a.rot4[2].x;
                        a.rot3[3].x = a.rot4[3].x;
                        a.rot3[0].y = a.rot4[0].y;
                        a.rot3[1].y = a.rot4[1].y;
                        a.rot3[2].y = a.rot4[2].y;
                        a.rot3[3].y = a.rot4[3].y;
                        a.rot4[0].x = temp0x;
                        a.rot4[1].x = temp1x;
                        a.rot4[2].x = temp2x;
                        a.rot4[3].x = temp3x;
                        a.rot4[0].y = temp0y;
                        a.rot4[1].y = temp1y;
                        a.rot4[2].y = temp2y;
                        a.rot4[3].y = temp3y;
                        G8RTOS_SignalSemaphore(activeMutex);
                        writeFIFO(CHANGE_FIFO, 3);
                }
                else{
                    G8RTOS_SignalSemaphore(matrixMutex);
                    G8RTOS_SignalSemaphore(activeMutex);
                }
            }
            else{
                G8RTOS_SignalSemaphore(activeMutex);
            }
            //debounce
            sleep(500);
            GPIOIntClear(GPIO_PORTC_BASE,GPIO_INT_PIN_5);
            //renable interrupt
            GPIOIntEnable(GPIO_PORTC_BASE, GPIO_INT_PIN_5);
            start_rotate_flag = false;
        }
        else if(drop){
                speedSleep = 10;
                drop = false;
        }

        sleep(50);
    }
}

void blockThread(void){
    BLOCK currentBlock;
    switch(nextBlock) {
       case YELLOWB :
          currentBlock.current_location[0].x = 5;
          currentBlock.current_location[0].y = 0;
          currentBlock.current_location[1].x = 4;
          currentBlock.current_location[1].y = 0;
          currentBlock.current_location[2].x = 5;
          currentBlock.current_location[2].y = 1;
          currentBlock.current_location[3].x = 4;
          currentBlock.current_location[3].y = 1;
          currentBlock.rotate_location[0].x = 5;  //yellow has same rotate position
          currentBlock.rotate_location[0].y = 0;
          currentBlock.rotate_location[1].x = 4;
          currentBlock.rotate_location[1].y = 0;
          currentBlock.rotate_location[2].x = 5;
          currentBlock.rotate_location[2].y = 1;
          currentBlock.rotate_location[3].x = 4;
          currentBlock.rotate_location[3].y = 1;
          currentBlock.rot3[0].x = 5;  //yellow has same rotate position
          currentBlock.rot3[0].y = 0;
          currentBlock.rot3[1].x = 4;
          currentBlock.rot3[1].y = 0;
          currentBlock.rot3[2].x = 5;
          currentBlock.rot3[2].y = 1;
          currentBlock.rot3[3].x = 4;
          currentBlock.rot3[3].y = 1;
          currentBlock.rot4[0].x = 5;  //yellow has same rotate position
          currentBlock.rot4[0].y = 0;
          currentBlock.rot4[1].x = 4;
          currentBlock.rot4[1].y = 0;
          currentBlock.rot4[2].x = 5;
          currentBlock.rot4[2].y = 1;
          currentBlock.rot4[3].x = 4;
          currentBlock.rot4[3].y = 1;
          currentBlock.past_location[0].x = 5;
          currentBlock.past_location[0].y = 0;
          currentBlock.past_location[1].x = 4;
          currentBlock.past_location[1].y = 0;
          currentBlock.past_location[2].x = 5;
          currentBlock.past_location[2].y = 1;
          currentBlock.past_location[3].x = 4;
          currentBlock.past_location[3].y = 1;
          currentBlock.color = YELLOWB;
          break;
       case GREENB :
           currentBlock.current_location[0].x = 4;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 4;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 1;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 2;
           currentBlock.rotate_location[0].x = 5;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 1;
           currentBlock.rotate_location[3].x = 3;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 1;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 1;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 2;
           currentBlock.rot4[0].x = 5;  //rotate position  4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 4;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 3;
           currentBlock.rot4[3].y = 1;
           currentBlock.past_location[0].x = 4;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 4;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 1;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 2;
           currentBlock.color = GREENB;
          break;
       case PURPLEB :
           currentBlock.current_location[0].x = 4;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 4;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 4;
           currentBlock.current_location[2].y = 2;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 1;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 5;
           currentBlock.rotate_location[2].y = 0;
           currentBlock.rotate_location[3].x = 4;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 1;
           currentBlock.rot3[1].x = 5;
           currentBlock.rot3[1].y = 0;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 1;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 2;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 1;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 1;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 4;
           currentBlock.rot4[3].y = 0;
           currentBlock.past_location[0].x = 4;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 4;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 4;
           currentBlock.past_location[2].y = 2;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 1;
           currentBlock.color = PURPLEB;  //find actual
          break;
       case DBLUEB :
           currentBlock.current_location[0].x = 5;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 5;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 2;
           currentBlock.current_location[3].x = 4;
           currentBlock.current_location[3].y = 2;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 3;
           currentBlock.rotate_location[1].y = 1;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 1;
           currentBlock.rotate_location[3].x = 5;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 1;
           currentBlock.rot3[2].x = 4;
           currentBlock.rot3[2].y = 2;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 0;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 0;
           currentBlock.rot4[3].x = 5;
           currentBlock.rot4[3].y = 1;
           currentBlock.past_location[0].x = 5;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 5;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 2;
           currentBlock.past_location[3].x = 4;
           currentBlock.past_location[3].y = 2;
           currentBlock.color = DBLUEB;
          break;
       case LBLUEB :
           currentBlock.current_location[0].x = 5;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 5;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 2;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 3;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 5;
           currentBlock.rotate_location[2].y = 0;
           currentBlock.rotate_location[3].x = 6;
           currentBlock.rotate_location[3].y = 0;
           currentBlock.rot3[0].x = 5;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 5;
           currentBlock.rot3[1].y = 1;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 2;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 3;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 0;
           currentBlock.rot4[3].x = 6;
           currentBlock.rot4[3].y = 0;
           currentBlock.past_location[0].x = 5;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 5;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 2;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 3;
           currentBlock.color = LBLUEB;
          break;
       case REDB :
           currentBlock.current_location[0].x = 5;
           currentBlock.current_location[0].y = 0;
           currentBlock.current_location[1].x = 5;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 4;
           currentBlock.current_location[2].y = 1;
           currentBlock.current_location[3].x = 4;
           currentBlock.current_location[3].y = 2;
           currentBlock.rotate_location[0].x = 3;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 0;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 1;
           currentBlock.rotate_location[3].x = 5;
           currentBlock.rotate_location[3].y = 1;
           currentBlock.rot3[0].x = 4;  //rotate position 3
           currentBlock.rot3[0].y = 1;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 2;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 0;
           currentBlock.rot3[3].x = 5;
           currentBlock.rot3[3].y = 1;
           currentBlock.rot4[0].x = 3;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 4;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 4;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 5;
           currentBlock.rot4[3].y = 1;
           currentBlock.past_location[0].x = 5;
           currentBlock.past_location[0].y = 0;
           currentBlock.past_location[1].x = 5;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 4;
           currentBlock.past_location[2].y = 1;
           currentBlock.past_location[3].x = 4;
           currentBlock.past_location[3].y = 2;
           currentBlock.color = REDB;
           break;
       case ORANGEB :
           currentBlock.current_location[0].x = 3;
           currentBlock.current_location[0].y = 1;
           currentBlock.current_location[1].x = 4;
           currentBlock.current_location[1].y = 1;
           currentBlock.current_location[2].x = 5;
           currentBlock.current_location[2].y = 1;
           currentBlock.current_location[3].x = 5;
           currentBlock.current_location[3].y = 0;
           currentBlock.rotate_location[0].x = 4;  //rotate position
           currentBlock.rotate_location[0].y = 0;
           currentBlock.rotate_location[1].x = 4;
           currentBlock.rotate_location[1].y = 1;
           currentBlock.rotate_location[2].x = 4;
           currentBlock.rotate_location[2].y = 2;
           currentBlock.rotate_location[3].x = 5;
           currentBlock.rotate_location[3].y = 2;
           currentBlock.rot3[0].x = 3;  //rotate position 3
           currentBlock.rot3[0].y = 0;
           currentBlock.rot3[1].x = 4;
           currentBlock.rot3[1].y = 0;
           currentBlock.rot3[2].x = 5;
           currentBlock.rot3[2].y = 0;
           currentBlock.rot3[3].x = 3;
           currentBlock.rot3[3].y = 1;
           currentBlock.rot4[0].x = 4;  //rotate position 4
           currentBlock.rot4[0].y = 0;
           currentBlock.rot4[1].x = 5;
           currentBlock.rot4[1].y = 0;
           currentBlock.rot4[2].x = 5;
           currentBlock.rot4[2].y = 1;
           currentBlock.rot4[3].x = 5;
           currentBlock.rot4[3].y = 2;
           currentBlock.past_location[0].x = 3;
           currentBlock.past_location[0].y = 1;
           currentBlock.past_location[1].x = 4;
           currentBlock.past_location[1].y = 1;
           currentBlock.past_location[2].x = 5;
           currentBlock.past_location[2].y = 1;
           currentBlock.past_location[3].x = 5;
           currentBlock.past_location[3].y = 0;
           currentBlock.color = ORANGEB;
           break;
       default :
           break;
    }
    //select random block color to be next
    nextBlock = nextnextBlock;
    nextnextBlock = nextnextnextBlock;
    nextnextnextBlock = getRandColor(SystemTime,nextnextBlock);
    //nextnextnextBlock = YELLOWB;
    printLittleNext(1,true);
    printLittleNext(1,false);
    printLittleNext(2,true);
    printLittleNext(2,false);
    printLittleNext(3,true);
    printLittleNext(3,false);


    a = currentBlock;

    //draw new block location
    int16_t c = getColorVal(a.color);
    LCD_DrawRectangle(a.current_location[0].y*20+3,a.current_location[0].x*20+3,16,16,c);
    LCD_DrawRectangle(a.current_location[1].y*20+3,a.current_location[1].x*20+3,16,16,c);
    LCD_DrawRectangle(a.current_location[2].y*20+3,a.current_location[2].x*20+3,16,16,c);
    LCD_DrawRectangle(a.current_location[3].y*20+3,a.current_location[3].x*20+3,16,16,c);

    //update dest location
    int16_t temp0y = a.current_location[0].y;
    int16_t temp1y = a.current_location[1].y;
    int16_t temp2y = a.current_location[2].y;
    int16_t temp3y = a.current_location[3].y;
    int16_t temp0x = a.current_location[0].x;
    int16_t temp1x = a.current_location[1].x;
    int16_t temp2x = a.current_location[2].x;
    int16_t temp3x = a.current_location[3].x;
    while(1){
        if(matrix[temp0y+1][temp0x] != BLACKB || matrix[temp1y+1][temp1x] != BLACKB || matrix[temp2y+1][temp2x] != BLACKB || matrix[temp3y+1][temp3x] != BLACKB || temp0y == 15 || temp1y == 15 || temp2y == 15 || temp3y == 15){
            break;
        }
        temp0y++;
        temp1y++;
        temp2y++;
        temp3y++;
    }
    //update dest location of block
     a.dest_location[0].x = temp0x;
     a.dest_location[0].y = temp0y;
     a.dest_location[1].x = temp1x;
     a.dest_location[1].y = temp1y;
     a.dest_location[2].x = temp2x;
     a.dest_location[2].y = temp2y;
     a.dest_location[3].x = temp3x;
     a.dest_location[3].y = temp3y;
     //draw new dest location
     //print destination location shadow
     LCD_DrawRectangle(a.dest_location[0].y*20+3,a.dest_location[0].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[1].y*20+3,a.dest_location[1].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[2].y*20+3,a.dest_location[2].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[3].y*20+3,a.dest_location[3].x*20+3,16,16,0xFFFF);
     LCD_DrawRectangle(a.dest_location[0].y*20+4,a.dest_location[0].x*20+4,14,14,0);
     LCD_DrawRectangle(a.dest_location[1].y*20+4,a.dest_location[1].x*20+4,14,14,0);
     LCD_DrawRectangle(a.dest_location[2].y*20+4,a.dest_location[2].x*20+4,14,14,0);
     LCD_DrawRectangle(a.dest_location[3].y*20+4,a.dest_location[3].x*20+4,14,14,0);

    // sleep(sleepTime);

    //spawn game threads
    threadIDs[0] = threadTotal;
    G8RTOS_AddThread(LCDprint,0,"lcd print");
    G8RTOS_AddThread(downMove,1,"down move");
    threadIDs[1] = threadTotal;
    G8RTOS_AddThread(sideMove,1,"side move");
    threadIDs[2] = threadTotal;
    G8RTOS_AddThread(rotateMove,1,"rotate move");
    first = true;



    G8RTOS_KillSelf();
}

enum block_color getRandColor(int seed, enum block_color last){
   // return YELLOWB; //
    srand(seed);
    int num = (rand() % (6 - 0 + 1)) + 0;
    switch(num) {
       case 0 :
          return ORANGEB;
       case 1 :
           return GREENB;
       case 2 :
           return PURPLEB;
       case 3 :
           return DBLUEB;
       case 4 :
           return LBLUEB;
       case 5 :
           return REDB;
       case 6 :
           return YELLOWB;
       default :
           return YELLOWB;
    }
}
int16_t getColorVal(enum block_color c){
    switch(c) {
            case ORANGEB :
                return 0xFD60;
            case GREENB :
                return 0x07E0;
            case PURPLEB :
                return 0x991D;
            case DBLUEB :
                return 0x001F;
            case LBLUEB :
                return 0x07FF;
            case REDB :
                return 0xF800;
            case YELLOWB :
                return 0xFFE0;
            case BLACKB :
                return 0;
            default :
                return 0;
    }
}

void printLittleNext(int i, bool clear){
    int x1,y1,x2,y2,x3,y3,x4,y4;
    int16_t color = 0;
    enum block_color blockInQuestion = BLACKB;
    if(i == 1){
        blockInQuestion = nextBlock;
    }
    else if(i == 2){
        blockInQuestion = nextnextBlock;
    }
    else if(i == 3){
        blockInQuestion = nextnextnextBlock;
    }
    switch(blockInQuestion) {
       case YELLOWB :
           x1 = 8;
           y1 = 8;
           x2 = 16;
           y2 = 8;
           x3 = 8;
           y3 = 16;
           x4 = 16;
           y4 = 16;
           color = 0xFFE0;
           break;
       case GREENB :
           x1 = 0;
           y1 = 8;
           x2 = 0;
           y2 = 16;
           x3 = 8;
           y3 = 16;
           x4 = 8;
           y4 = 24;
           color = 0x07E0;
           break;
       case PURPLEB :
           x1 = 8;
           y1 = 0;
           x2 = 8;
           y2 = 8;
           x3 = 8;
           y3 = 16;
           x4 = 16;
           y4 = 8;
           color = 0x991D;
           break;
       case DBLUEB :
           x1 = 16;
           y1 = 0;
           x2 = 16;
           y2 = 8;
           x3 = 16;
           y3 = 16;
           x4 = 8;
           y4 = 16;
           color = 0x001F;
           break;
       case LBLUEB :
           x1 = 8;
           y1 = 0;
           x2 = 8;
           y2 = 8;
           x3 = 8;
           y3 = 16;
           x4 = 8;
           y4 = 24;
           color = 0x07FF;
           break;
       case REDB :
           x1 = 16;
           y1 = 0;
           x2 = 16;
           y2 = 8;
           x3 = 8;
           y3 = 8;
           x4 = 8;
           y4 = 16;
           color = 0xF800;
           break;
       case ORANGEB :
           x1 = 16;
           y1 = 8;
           x2 = 16;
           y2 = 16;
           x3 = 8;
           y3 = 16;
           x4 = 0;
           y4 = 16;
           color = 0xFD60;
           break;
       default :
           x1 = 0;
           x2 = 0;
           x3 = 0;
           x4 = 0;
           y1 = 0;
           y2 = 0;
           y3 = 0;
           y4 = 0;
           color = 0;
    }
    if(clear){
        if(i == 1){
           LCD_DrawRectangle(30,211,32,22,0);
        }
        else if(i == 2){
            LCD_DrawRectangle(80,211,32,22,0);
        }
        else if(i == 3){
            LCD_DrawRectangle(130,211,32,22,0);
        }
    }
    else{
        if(i == 1){
            LCD_DrawRectangle(30+y1,211+x1,6,6,color);
            LCD_DrawRectangle(30+y2,211+x2,6,6,color);
            LCD_DrawRectangle(30+y3,211+x3,6,6,color);
            LCD_DrawRectangle(30+y4,211+x4,6,6,color);
        }
        else if(i == 2){
            LCD_DrawRectangle(80+y1,211+x1,6,6,color);
            LCD_DrawRectangle(80+y2,211+x2,6,6,color);
            LCD_DrawRectangle(80+y3,211+x3,6,6,color);
            LCD_DrawRectangle(80+y4,211+x4,6,6,color);
        }
        else if(i == 3){
            LCD_DrawRectangle(130+y1,211+x1,6,6,color);
            LCD_DrawRectangle(130+y2,211+x2,6,6,color);
            LCD_DrawRectangle(130+y3,211+x3,6,6,color);
            LCD_DrawRectangle(130+y4,211+x4,6,6,color);
        }
    }


}
>>>>>>> 770dc6ee1cf041427f3c33d16ac37c524874d30e
