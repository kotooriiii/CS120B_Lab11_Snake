/*	Author: Carlos Miranda cmira039@ucr.edu
 *  Partner(s) Name: n/a
 *	Lab Section: 23
 *	Assignment: Lab #11  Exercise #1
 *	Exercise Description: [optional - include for your own benefit]
 *
 *	I acknowledge all content contained herein, excluding template or example
 *	code, is my own original work.
 *
 *	Demo Link: https://www.youtube.com/watch?v=vg8U1FO0HE0
 */
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef _SIMULATE_
#include "simAVRHeader.h"
#endif


/*
	Bit start
*/ 

// Permission to copy is granted provided that this header remains intact. 
// This software is provided with no warranties.

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//Functionality - Sets bit on a PORTx
//Parameter: Takes in a uChar for a PORTx, the pin number and the binary value 
//Returns: The new value of the PORTx
unsigned char SetBit(unsigned char pin, unsigned char number, unsigned char bin_value) 
{
	return (bin_value ? pin | (0x01 << number) : pin & ~(0x01 << number));
}

////////////////////////////////////////////////////////////////////////////////
//Functionality - Gets bit from a PINx
//Parameter: Takes in a uChar for a PINx and the pin number
//Returns: The value of the PINx
unsigned char GetBit(unsigned char port, unsigned char number) 
{
	return ( port & (0x01 << number) );
}

//Helper methods 
//Returns inverse of PINA
unsigned char IPINA()
{
	return ~PINA;
}

unsigned char isOnlyA0()
{
	return IPINA() == 0x01;
}

unsigned char isOnlyA1()
{
	return IPINA() == 0x02;
}

unsigned char isOnlyA2()
{
	return IPINA() == 0x04;
}

unsigned char isOnlyA3()
{
	return IPINA() == 0x08;
}

/* 
	Bit end 
*/

/*
	Timer start
*/

volatile unsigned char TimerFlag = 0;
unsigned long _avr_timer_M = 1;
unsigned long _avr_timer_cntcurr = 0;

void TimerISR()
{
	TimerFlag = 1;
}

void TimerOff()
{
	TCCR1B = 0x00;
}

void TimerOn()
{
	TCCR1B = 0x0B;

	OCR1A = 125;

	TIMSK1 = 0x02;

	TCNT1 = 0;

	_avr_timer_cntcurr = _avr_timer_M;

	SREG |= 0x80;

}

ISR(TIMER1_COMPA_vect)
{
	_avr_timer_cntcurr--;
	if(_avr_timer_cntcurr == 0)
	{
		TimerISR();
		_avr_timer_cntcurr = _avr_timer_M;
	}
}

void TimerSet(unsigned long M)
{
	_avr_timer_M = M;
	_avr_timer_cntcurr = _avr_timer_M;
}

/*
	Timer end
*/

/* GCD finder
*/

unsigned long int findGCD(unsigned long int a, unsigned long int b)
{
	unsigned long int c;
	while(1){
		c = a % b;
		if( c == 0 ) { return b; }
		a = b;
		b = c;
	}
	return 0;
}

/* end of gcd finder */

/*
	Task begin
*/

typedef struct task
{
	signed char state; //current state
	unsigned long int period; //current task period
	unsigned long int elapsedTime; //elapsed time since last tick
	int (*TickFct)(int); //task tick func
} task;

/*
	Task end
*/

/*
	Grid begin. 
	1) It's a 2D array (1d technically) of the final grid which represents the LED Matrix. 
	2) It's a 2D array showing the current state of the snake
*/

#define GRID_WIDTH 8 // column 
#define GRID_HEIGHT 5 //rows

unsigned char grid[GRID_WIDTH * GRID_HEIGHT];
unsigned char snakeGrid[GRID_WIDTH * GRID_HEIGHT];

void GridSet(int row, int col, unsigned char value)
{
	grid[GRID_WIDTH * row + col] = value;  
}

unsigned char GridGet(int row, int col)
{
	if(row >= GRID_HEIGHT || col >= GRID_WIDTH)
		return 0;
	return grid[GRID_WIDTH * row + col];
}

void SnakeGridSet(int row, int col, unsigned char value)
{
	snakeGrid[GRID_WIDTH * row + col] = value;  
}

unsigned char SnakeGridGet(int row, int col)
{
	if(row >= GRID_HEIGHT || col >= GRID_WIDTH)
		return 0;
	return snakeGrid[GRID_WIDTH * row + col];
}

/*
	Grid end
*/

/* Shared variables begin */

/**
MovementButtonListenerSM
The variable keeps track of the current snake direction.
0x00 = Right
0x01 = Up
0x02 = Left 
0x03 = Down
0x04 or any other value = NULL, non-moving state
*/
unsigned char currentDirection = 0x04; 

//Current snake location
unsigned char snakeHeadRow = GRID_HEIGHT/2;
unsigned char snakeHeadCol = GRID_WIDTH/2;

//Snake is dead?
unsigned char isDead = 0x00;
//Snake current size 
unsigned char size = 0x01; 

//Food location and whether it exists
unsigned char foodRow=0x0l, foodCol=0x01, foodExists = 0x00;


/*
	Grid -> LED Matrix
*/
unsigned char GPINC = 0x00, GPIND = 0xFF;
unsigned char currentGridColumn = 0x00;

//Respawn timer
unsigned long int respawnTimer = 0;

/* Shared variables end */

//SM
enum MovementButtonListenerSM {MBL_INIT, MBL_LISTEN};
enum MovementHandlerListenerSM {MHL_INIT, MHL_MOVE}; 
enum FoodGeneratorSM{FG_INIT, FG_CREATE, FG_WAIT};
enum GridUpdateSM {GU_INIT, GU_MERGE, GU_DISPLAY, GU_DEAD};


//Tick Funcs
int movementButtonListenerSMTick(int state);
int movementHandlerListenerSMTick(int state);
int foodGeneratorSMTick(int state);
int gridUpdateSMTick(int state);

int main(void) 
{
	//Input(s)
	DDRA = 0x00; PORTA = 0xFF;

	//Output(s)
	DDRB = 0xFF; PORTB = 0x00;
	DDRC = 0xFF; PORTC = 0x00;
	DDRD = 0xFF; PORTD = 0x00;

	srand(time(NULL));

	//Task initialization
	static task task0, task1, task2, task4;
	task *tasks[] = {&task0, &task1, &task2, &task4};
	const unsigned short numTasks = sizeof(tasks)/sizeof(task*);
	
	//Task state undefined
	const char undefined = -1;
	long int gcd = -1;

	//Task 0 (MovementButtonListenerSM)
	task0.state = undefined;
	task0.period = 50;
	task0.elapsedTime = task0.period;
	task0.TickFct = &movementButtonListenerSMTick;
	
	//Task 1 (MovementHandlerListenerSM)
	task1.state = undefined;
	task1.period = 300;
	task1.elapsedTime = task1.period;
	task1.TickFct =&movementHandlerListenerSMTick;
	
	//Task 2 (FoodGeneratorSM)
	task2.state = undefined;
	task2.period = 600;
	task2.elapsedTime = task2.period;
	task2.TickFct =&foodGeneratorSMTick;
	
	//Task 4 (GridUpdateSM)
	task4.state = undefined;
	task4.period = 1;
	task4.elapsedTime = task4.period;
	task4.TickFct = &gridUpdateSMTick;
	
	//Calculate GCD
	unsigned short i; //scheduler
	for(i = 0; i < numTasks; i++)
	{
		if(i == 0)
		{	
			gcd = tasks[i]->period;
		} 
		else 
		{
			gcd = findGCD(gcd, tasks[i]->period);
		}
	}
	
	//Init values to 0;
	for(i = 0 ; i < GRID_WIDTH*GRID_HEIGHT; i++)
	{
		grid[i] = 0;
		snakeGrid[i] = 0;
	}
	
	TimerSet(gcd);	
	TimerOn();
	
	while(1) 
	{
		for(i = 0; i < numTasks; i++)
		{
			if(tasks[i]->elapsedTime ==  tasks[i]->period)
			{
				tasks[i]->state = tasks[i]->TickFct(tasks[i]->state);
				tasks[i]->elapsedTime = 0;
			}
			tasks[i]->elapsedTime += gcd;
		}
		while(!TimerFlag);
		TimerFlag = 0;
	}

	return 1;
}


//Reads button for movement
int movementButtonListenerSMTick(int state)
{
	switch(state)
	{
		case MBL_INIT:
		state = MBL_LISTEN;
		break;
		
		case MBL_LISTEN:
		state = MBL_LISTEN;
		break;
		
		default:
		state = MBL_INIT;
		break;
	}
	
	switch(state)
	{
		case MBL_INIT:
		currentDirection = 0x04;
		break;
		
		case MBL_LISTEN:
		if(isOnlyA0()) //going right, press right button
		{
			currentDirection = 0x00;
		}
		else if(isOnlyA1())//going up, press up button
		{
			currentDirection = 0x01;

		}
		else if(isOnlyA2())//going left, press left button
		{
			currentDirection = 0x02;

		}
		else if(isOnlyA3())//going down, press down button
		{
			currentDirection = 0x03;
			
		}
		else //no button press or multiple button press. keep current path.
		{
			//Would like to keep path, so do nothing!
			//currentDirection = 0x00;
		}
		break;
		
		default:
		break;
	}
	
	return state;
}

//Removes tail when moving. Recursively looks for the tail. O(n)
void snakeRecursiveMoveDelete(int row, int col, int score)
{
	//Score == 1 means this is the tail!
	if(score == 1)
	{
		//Remove tail 
		SnakeGridSet(row,col,0x00);
		return;
	}
	
	
	//SEARCH FOR THE TAIL
	
	if(SnakeGridGet(row,col-1) == score-1)
	{
		snakeRecursiveMoveDelete(row, col-1, score-1);
	}
	else if(SnakeGridGet(row,col+1) == score-1)
	{
		snakeRecursiveMoveDelete(row, col+1, score-1);
	}
	else if(SnakeGridGet(row+1,col) == score-1)
	{
		snakeRecursiveMoveDelete(row+1, col, score-1);
	}
	else if(SnakeGridGet(row-1,col) == score-1)
	{
		snakeRecursiveMoveDelete(row-1, col, score-1);
	}
	else {
		
		//ERROR!!!!! this should never occur. this state is if the tail is missing. this could never happen
		return;
	}
	
	//Lower score since we mimicked an "eat" operation.
	SnakeGridSet(row, col, SnakeGridGet(row, col)-1);

}


//Recursive function to find food strategically if random function taking too long. O(n)
void foodAlgoFinder(int row, int col, int *nrow, int *ncol, int score, unsigned char isAllowable)
{
	
	//End, try again later..
	if(score == 1)
	{
		return;
	}
	
	//Place food here!
	if(SnakeGridGet(row-1,col-1) == 0 && isAllowable)
	{
		*ncol = col-1;
		*nrow = row-1;
		return;
	}
	else if(SnakeGridGet(row-1,col+1) == 0 && isAllowable)
	{
		*ncol = col+1;
		*nrow = row-1;
		return;
	}
	else if(SnakeGridGet(row+1,col-1) == 0 && isAllowable)
	{
		*ncol = col-1;
		*nrow = row+1;
		return;
	}
	else if(SnakeGridGet(row+1,col+1) == 0 && isAllowable)
	{
		*ncol = col+1;
		*nrow = row+1;
		return;		
	}
	
	//Only place food if we are at bottom half of snake body
	unsigned char isAllowableMethod = score < size/2 ? 0x01 : 0x00;
	
	
	//Keep recursively calling function until we find bottom half of body
	if(SnakeGridGet(row,col-1) == score-1)
	{
		foodAlgoFinder(row, col-1, nrow, ncol, score-1, isAllowableMethod);
	}
	else if(SnakeGridGet(row,col+1) == score -1 )
	{
		foodAlgoFinder(row, col+1, nrow, ncol, score-1,isAllowableMethod);

	}
	else if(SnakeGridGet(row+1,col) == score-1)
	{
		foodAlgoFinder(row+1, col, nrow, ncol, score-1, isAllowableMethod);

	}
	else if(SnakeGridGet(row-1,col) == score-1)
	{
		foodAlgoFinder(row-1, col, nrow, ncol, score-1, isAllowableMethod);

	}
	else {
		//ERROR!!!!! cant find tail
		return;
	}
}


//Helper method to find food.
void ranFoodGen()
{
	
	unsigned char found = 0x00;
	
	int counter = 0;
	
	do 
	{
		int ranCol = -1;
		int ranRow = -1; 
		if(counter >= 10) //Use O(n) algorithm 
		{
			
			foodAlgoFinder(snakeHeadRow, snakeHeadCol, &ranRow, &ranCol, size, 0);
			
			//Algorithm didn't work, try again at next tick
			if(ranCol == -1 && ranRow == -1)
				return;
			
		} 
		else //Use random algorithm. This could take a VERY LONG TIME if snake is too large.
		{ 
		
			ranCol = rand() % GRID_WIDTH;
			ranRow = rand() % GRID_HEIGHT; 
		
			counter++;
		}
		
		if(SnakeGridGet(ranRow, ranCol)) //If we are trying to place food on a snake body, skip adding a food pellet 
			continue;
		
		//Food is found, break loop.
		foodCol = ranCol;
		foodRow = ranRow;
		foodExists = 0x01;
		found = 0x01;
		
	} while(!found);
	
}

//Handles movement. Any collision to wall, itself, or food.
int movementHandlerListenerSMTick(int state)
{
	
	unsigned char nextRow, nextCol;
	
	switch (state)
	{
		case MHL_INIT:
		state = MHL_MOVE;
		break;
		
		case MHL_MOVE:
		state = MHL_MOVE;
		break;
		
		default:
		state = MHL_INIT;
		break;
	}
	
	switch (state)
	{
		case MHL_INIT:
		SnakeGridSet(snakeHeadRow, snakeHeadCol, 0x01);
		PORTB = 0x01;
		break;
		
		case MHL_MOVE:
		
		//Check bounds of snake
		switch(currentDirection)
		{
			case 0x00: //right
			if(snakeHeadCol == GRID_WIDTH-1)
			{
				//out of bounds, snake collide to end of wall
				isDead = 0x01;
				return state;
			}
			nextCol = snakeHeadCol+1;
			nextRow = snakeHeadRow;
			break;
			
			case 0x01: //up
			if(snakeHeadRow == GRID_HEIGHT-1)
			{
				//out of bounds, snake collide to end of wall
				isDead = 0x01;
				return state;
			}
			nextCol = snakeHeadCol;
			nextRow = snakeHeadRow+1;
			break;
			
			case 0x02: //left
			if(snakeHeadCol == 0)
			{
				//out of bounds, snake collide to end of wall
				isDead = 0x01;
				return state;
			}
			nextCol = snakeHeadCol-1;
			nextRow = snakeHeadRow;
			break;
			
			case 0x03: //down
			if(snakeHeadRow == 0)
			{
				//out of bounds, snake collide to end of wall
				isDead = 0x01;
				return state;
			}
			nextCol = snakeHeadCol;
			nextRow = snakeHeadRow-1;
			break;

			default:
			nextCol = snakeHeadCol;
			nextRow = snakeHeadRow;
			break;
		}
		
		if(SnakeGridGet(nextRow, nextCol) && currentDirection < 4) // Self collision AND its moving
		{
			// termination by self-collision 
			isDead = 0x01;
			return state;
		}
		
		if(foodExists && nextRow == foodRow && nextCol == foodCol) //food! ate food pellet and grows. update score
		{
			SnakeGridSet(nextRow, nextCol, ++size);
			foodExists = 0x00;
			PORTB = size;
		}
		else 
		{
			
			//If it's moving, then delete the tail and update snake's grid
			if(currentDirection < 4)
			{
				//Fake add, we subtract in move delete
				SnakeGridSet(nextRow, nextCol, size+1);
				snakeRecursiveMoveDelete(nextRow, nextCol, size+1);	
			}
			

		
		}
		
		snakeHeadCol = nextCol;
		snakeHeadRow = nextRow;
		
		break;
		
		default:
		break;
	}
	return state;
}

//Generates food SM
int foodGeneratorSMTick(int state)
{
	switch(state)
	{
		case FG_INIT:
		state= FG_CREATE;
		break;
		
		case FG_CREATE:
		if(foodExists)
		{
			state = FG_WAIT;
		}
		break;
		
		case FG_WAIT:
		if(!foodExists)
		{
			state = FG_CREATE;
		}
		break;
		
		default:
		state = FG_INIT;
		break;
	}
	
	switch(state)
	{
		case FG_INIT:
		break;
		
		case FG_CREATE:
		ranFoodGen(); //function defined above
		break;
		
		case FG_WAIT:
		break;
		
		default:
		break;
	}
	return state;
}


//Tick function update the LED Matrix GRID.
int gridUpdateSMTick(int state)
{
	
	
	switch(state)
	{
		case GU_INIT:
		state = GU_MERGE;
		break;
		
		case GU_MERGE:
		if(isDead)
			state = GU_DEAD;
		else 
			state = GU_DISPLAY;
		break; 
		case GU_DISPLAY:
		if(isDead)
			state = GU_DEAD;
		else 
			state = GU_MERGE;
		break;
		
		case GU_DEAD:
		if(!isDead)
			state = GU_MERGE;
		break;
		default:
		state = GU_INIT;
		break;
	}
	
		
	switch(state)
	{
		case GU_INIT:
		break;
		
		case GU_MERGE:

		//Loop all the cells, update grid to match snake's state
		for(int i_row = 0; i_row < GRID_HEIGHT; i_row++)
		{
			for(int i_col = 0; i_col < GRID_WIDTH; i_col++)
			{
					GridSet(i_row, i_col, SnakeGridGet(i_row, i_col) >= 0x01 ? 0x01 : 0x00); 
			}
		}
		
		//If food exists, show it on the grid!
		if(foodExists)
		{
			GridSet(foodRow, foodCol, 0x01); 
		}
		break;
		
		case GU_DISPLAY:
		
		//If column is over the width, reset to 0
		if(currentGridColumn >= GRID_WIDTH)
		{
			currentGridColumn = 0;
		}
				
		unsigned char sum = 0;
		
		//A little math to calculate power of what row we are in. Since we are using LED Matrix patterns, then we need to add the bit's value.
		for(int i_row = 0; i_row < GRID_HEIGHT; i_row++)
		{
			unsigned char pow = 1;
			
			for(int i_pow = 0; i_pow < i_row; i_pow++)
			{
				pow *= 2;
			}
			
			if(GridGet(i_row, currentGridColumn))
				sum += pow;
		}
		
		GPIND = sum; //Update it to temp variable
	
		unsigned char powCol = 1;
			
		//Do the same with columns! 
		for(int i_pow = 0; i_pow < currentGridColumn; i_pow++)
		{
				powCol *= 2;
		}	
		
		GPINC = powCol;//Update to temp variable
		
		//Update ports
		PORTC = GPINC;
		PORTD = ~GPIND;
		
		//Next column!
		currentGridColumn++;
		break;
		
		case GU_DEAD:
		GPINC = 0x00; //death screen
		GPIND = 0xFF;
		PORTC = GPINC;
		PORTD = GPIND;
		
		//Wait 5 seconds before respawning 
		if(respawnTimer >= 5000)
		{
			//Reinitialize all values
			
			respawnTimer = 0;
			isDead = 0x00;
			size = 0x01;
			foodExists = 0x00;
			snakeHeadRow = GRID_HEIGHT/2;
			snakeHeadCol = GRID_WIDTH/2;
			currentDirection = 0x04;

			unsigned long int i; 
			//Init values to 0;
			for(i = 0 ; i < GRID_WIDTH*GRID_HEIGHT; i++)
			{
				grid[i] = 0;
				snakeGrid[i] = 0;
			}
			SnakeGridSet(snakeHeadRow, snakeHeadCol, 0x01);

		}
		else
		{ 
			respawnTimer++;
		}
		break;
		
		default:
		break;
	}
	
	return state;	
}

