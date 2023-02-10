<<<<<<< HEAD
/**
 * thread.h
 * @author:
 * uP2 - Fall 2022
 */

#ifndef THREADS_H_
#define THREADS_H_

#include "FinalProject_Tetris/G8RTOS_Semaphores.h"
#include "FinalProject_Tetris/G8RTOS_Structures.h"

#define SCREEN_WIDTH 10
#define SCREEN_LENGTH 16

/*#define DOWN_FIFO 0
#define SIDE_FIFO 1
#define ROTATE_FIFO 2*/
#define CHANGE_FIFO 0



static bool start_rotate_flag;


bool left;
bool right;
bool drop;
bool first;
int debounceL;
int debounceR;


// define semaphores
semaphore_t *LCDMutex;
semaphore_t LCD_disp;
semaphore_t *matrixMutex;
semaphore_t matrixSP;
semaphore_t *activeMutex;
semaphore_t activeT;

//color enums
enum block_color {
    ORANGEB,
    GREENB,
    PURPLEB,
    DBLUEB,
    LBLUEB,
    REDB,
    YELLOWB,
    BLACKB
};
//pair struct for grid coordinates
typedef struct pair {
    int16_t x;
    int16_t y;
} pair;
typedef struct BLOCK {
    pair past_location[4];
    pair current_location[4];
    pair dest_location[4];
    pair rotate_location[4];
    pair rot3[4];
    pair rot4[4];
    enum block_color color;
    bool active;
} BLOCK;
// Define matrix of blocks
static enum block_color matrix[SCREEN_LENGTH][SCREEN_WIDTH];
//define active block
BLOCK a;

//hold active thread ID's
threadId_t threadIDs[3]; ///[0]: lcdprint, [1]: side move, [2]: rotate move
// define your functions
void LCDtap(void);
void waitStartRestart(void);
void checkThread(void);
void gameOver(void);
void blockThread(void);
void IdleThread(void);
void InitGame(void);
void LCDprint(void);
void downMove(void);
void sideMove(void);
void rotateMove(void);
enum block_color getRandColor(int seed, enum block_color last); //helper function
void printLittleNext(int i, bool clear);
int16_t getColorVal(enum block_color c);

static int score; //global score
static int level;
static uint32_t sleepTime, speedSleep;
static enum block_color nextBlock, nextnextBlock, nextnextnextBlock;
static int gameColor;
uint32_t jsData;
int blockNum;

#endif /* THREADS_H_ */
=======
/**
 * thread.h
 * @author:
 * uP2 - Fall 2022
 */

#ifndef THREADS_H_
#define THREADS_H_

#include "FinalProject_Tetris/G8RTOS_Semaphores.h"
#include "FinalProject_Tetris/G8RTOS_Structures.h"

#define SCREEN_WIDTH 10
#define SCREEN_LENGTH 16

/*#define DOWN_FIFO 0
#define SIDE_FIFO 1
#define ROTATE_FIFO 2*/
#define CHANGE_FIFO 0



static bool start_rotate_flag;


bool left;
bool right;
bool drop;
bool first;
int debounceL;
int debounceR;


// define semaphores
semaphore_t *LCDMutex;
semaphore_t LCD_disp;
semaphore_t *matrixMutex;
semaphore_t matrixSP;
semaphore_t *activeMutex;
semaphore_t activeT;

//color enums
enum block_color {
    ORANGEB,
    GREENB,
    PURPLEB,
    DBLUEB,
    LBLUEB,
    REDB,
    YELLOWB,
    BLACKB
};
//pair struct for grid coordinates
typedef struct pair {
    int16_t x;
    int16_t y;
} pair;
typedef struct BLOCK {
    pair past_location[4];
    pair current_location[4];
    pair dest_location[4];
    pair rotate_location[4];
    pair rot3[4];
    pair rot4[4];
    enum block_color color;
    bool active;
} BLOCK;
// Define matrix of blocks
static enum block_color matrix[SCREEN_LENGTH][SCREEN_WIDTH];
//define active block
BLOCK a;

//hold active thread ID's
threadId_t threadIDs[3]; ///[0]: lcdprint, [1]: side move, [2]: rotate move
// define your functions
void LCDtap(void);
void waitStartRestart(void);
void checkThread(void);
void gameOver(void);
void blockThread(void);
void IdleThread(void);
void InitGame(void);
void LCDprint(void);
void downMove(void);
void sideMove(void);
void rotateMove(void);
enum block_color getRandColor(int seed, enum block_color last); //helper function
void printLittleNext(int i, bool clear);
int16_t getColorVal(enum block_color c);

static int score; //global score
static int level;
static uint32_t sleepTime, speedSleep;
static enum block_color nextBlock, nextnextBlock, nextnextnextBlock;
static int gameColor;
uint32_t jsData;
int blockNum;

#endif /* THREADS_H_ */
>>>>>>> 770dc6ee1cf041427f3c33d16ac37c524874d30e
