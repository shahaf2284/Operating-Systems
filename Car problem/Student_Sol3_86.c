///////////////////////////////////////////Includes////////////////////////////////////////////////////
#include <stdio.h>					// Printing the board
#include <stdlib.h>					// Randomizing FIN_PROB
#include <wait.h>					// Giving threads a little rest
#include <unistd.h>					// Giving threads a little rest
#include <pthread.h>					// Managing threads


///////////////////////////////////////////Defines/////////////////////////////////////////////////////
#define N 				5		// Board length/height
#define FIN_PROB 			0.1		// Probability to exit from circle
#define MIN_INTER_ARRIVAL_IN_NS 	8000000		// Lower bound of creation time
#define MAX_INTER_ARRIVAL_IN_NS 	9000000		// Higher bound of creation time
#define INTER_MOVES_IN_NS 		100000		// Higher bound of moving time
#define SIM_TIME 			2		// Simulation time in seconds
#define ARR_SIZE 			(4*(N-1))	// Simulation board actual size
#define CORNER_DIFF 			(N-1)		// The difference between generators/sinks
#define BLANK				    ' '		// The character that represents a blank (in print)
#define CAR_MARK			'*'		// The character that represents a car (in print)
#define CIRCLE_MARK			'@'		// The character that represents the circle (in print)
#define NUM_OF_PRINTS			10		// Number of prints of the board
#define TRUE				1
#define FALSE				0

///////////////////////////////////////////Structs/////////////////////////////////////////////////////
typedef struct Car{
	/* A struct defined for the cars on board. Each car keeps track of it's place
	 * 		and keeps its thread (for later destruction).
	 * */
	pthread_t carThread;				// 'Locking' a thread with its car
	int placeOnRoad;				// The index on the board the car is in
	int justBorn;					// A flag to indicate whether we can exit or not (Upon creation)
	struct Car* nextDelCar;				// Link list of cars for easy delete
} Car;

typedef struct Board{
	/* The board of the simulation. It has all the car generators and keeps track of
	 * 		total car count so we won't create more cars than panels or destroy more
	 * 		cars than we created. If a panel is NULL then it's safe to move to it
	 * */
	Car* panel[ARR_SIZE];				                             // The legal panels for cars to go through
	Car* delStack;					                                         // A stack to hold all cars removed from board
	int carCounter;					                                         // Car counter
	pthread_mutex_t delStackMutex;			            // Deletion stack's lock
	pthread_mutex_t boardMutex[ARR_SIZE];		// Locks array for panel protection
	pthread_mutex_t counterMutex;			                // A lock for the counter (Ensuring we don't 'lose' a car)
	pthread_t printer;			                                        // Printer thread
	pthread_t stationID[4];				                            // Station IDs
} Board;


///////////////////////////////////////////Functions///////////////////////////////////////////////////
void initBoard();
void freeAll(int state);
void* printBoard();
void* driveCar(void* car);
void* generateCar(void* id);
void moveCar(int placeOnBoard);
void trafficUp();
void trafficDown();
Car* buildCar(int placeOnBoard); 


///////////////////////////////////////////Global Variables/////////////////////////////////////////////
Board board;


///////////////////////////////////////////Implementations//////////////////////////////////////////////
int main(void){
	time_t t;
	int i;
	int id[4] = {0,1,2,3}; //Station ids
	srand((unsigned) time(&t));

	initBoard();

	// Create Station threads
	for(i=0;i<=3;i++){
		if(pthread_create(&board.stationID[i], NULL, generateCar, id+i)){
			perror("Error in creating station threads!\n");
			exit(EXIT_FAILURE);
		}

	}

	// Create printer thread
	if(pthread_create(&board.printer, NULL, printBoard, NULL)){
		perror("Error in creating printer thread!\n");
		exit(EXIT_FAILURE);
	}

	sleep(SIM_TIME);
	freeAll(EXIT_SUCCESS);
}

void initBoard(){
	/* Input: None
	 * Output: None, but changes the global variable 'board'
	 * Description: Initializes the simulation board - puts NULL on every panel.
	 * 		Also, initializes all mutexes (panel + counter + delStack)
	 * */

	int i;
	for(i=0;i<ARR_SIZE;i++){
		board.panel[i] = NULL;

		// Initialize panel mutex
		if (pthread_mutex_init(&board.boardMutex[i], NULL)){
			perror("Error in initializing panel mutex!\n");
			exit(EXIT_FAILURE);
		}
	}

	// Initialize counter mutex
	if (pthread_mutex_init(&board.counterMutex, NULL)){
		perror("Error in initializing counter mutex!\n");
		exit(EXIT_FAILURE);
	}
	
	// Initialize Delete-Stack mutex
	if (pthread_mutex_init(&board.delStackMutex, NULL)){
		perror("Error in initializing Delete-Stack mutex!\n");
		exit(EXIT_FAILURE);
	}

	// Initialize empty Delete-Stack
	board.delStack = NULL;
}

void freeAll(int state){
	/* Input: None
	 * Output: None
	 * Description: Frees all allocated memory, locks and threads. As every new car is created
	 * 		on a panel, we don't miss any car. Car threads are linked to cars, so we aren't
	 * 		missing them either. Last, any car that moved out is in the Delete-Stack
	 * */

	int i;
	Car* curr = board.delStack, *next;

	// Kill generators' threads
	for(i=0;i<4;i++){
		pthread_cancel(board.stationID[i]);
	}

	for(i=0;i<ARR_SIZE;i++){
		if (board.panel[i]){
			// Kill car threads
			pthread_cancel(board.panel[i]->carThread);

			// Free car space
			free(board.panel[i]);
		}
		// Destroy panel lock
		pthread_mutex_destroy(&board.boardMutex[i]);
	}

	// Destroy counter lock
	pthread_mutex_destroy(&board.counterMutex);

	// Free all the cars that left the simulation
	while(curr){
		next = curr -> nextDelCar;
		curr -> nextDelCar = NULL;
		free(curr);
		curr = next;
	}

	// Destroy Delete-Stack lock
	pthread_mutex_destroy(&board.delStackMutex);

	// Kill printing thread
	pthread_cancel(board.printer);

	exit(state);
}

void* printBoard(){
	/* Input: None
	 * Output: None
	 * Description: Prints the board on console
	 * 		This process/thread goes on forever until program termination
	 * */

	int i,j;
	while(1){
		// Creates a uniform time gap between all prints
		usleep((SIM_TIME/(double)(1+NUM_OF_PRINTS)) * 1000000); // Convert [sec] to [usec]
		for (i=0;i<N;i++){
			for(j=0;j<N;j++){
			switch (i){
				case 0: 			// First line
					putchar(board.panel[N-1-j]? CAR_MARK : BLANK);
					break;
				case CORNER_DIFF: 		// Last line
					putchar(board.panel[j+2*CORNER_DIFF]? CAR_MARK : BLANK);
					break;
				default:			// Rest of lines
					switch (j){
						case 0: 			// First column
							putchar(board.panel[CORNER_DIFF+i]? CAR_MARK : BLANK);
							break;
						case CORNER_DIFF: 		// Last column
							putchar(board.panel[ARR_SIZE-i]? CAR_MARK : BLANK);
							break;
						default:putchar(CIRCLE_MARK);	// Rest of columns
							break;
					}
				}
			}
			// Go down one line
			putchar('\n');
		}
		// Go down one line for next print
		putchar('\n');
	}
}

void* driveCar(void* car){
	/* Input: None
	 * Output: None
	 * Description: Simulates a car. Just waits INTER_MOVES_IN_NS nano seconds
	 * 		and tries to move. This process/thread goes on forever until program termination
	 * */

	Car* currCar = (Car*) car;

	while(1){
		usleep(INTER_MOVES_IN_NS / (double)1000); // Convert [nsec] to [usec]
		//printf("%d\n", currCar->placeOnRoad);
		moveCar(currCar->placeOnRoad);
	}

}

void* generateCar(void* id){
	/* Input: None
	 * Output: None
	 * Description: Creates a new car. This thread goes on forever until program termination
	 * */
        int randomTime;
        int stationNumber = *((int*)id);
        int currPanel, lastPanel;

        // Find on which panel the generator creates a car on
        if (stationNumber){
            currPanel = stationNumber*CORNER_DIFF;
            lastPanel = currPanel-1;
        }
        else{
            currPanel = 0;
            lastPanel = ARR_SIZE-1;
    }

	while(1){
		// randomTime ~ U([MIN_INTER_ARRIVAL_IN_NS , MAX_INTER_ARRIVAL_IN_NS])
		randomTime = rand()%(MAX_INTER_ARRIVAL_IN_NS - MIN_INTER_ARRIVAL_IN_NS) + MIN_INTER_ARRIVAL_IN_NS;
		usleep(randomTime / (double)1000); // Convert [nsec] to [usec]

		if (board.carCounter<ARR_SIZE-1)
			if (!(board.panel[currPanel] || board.panel[lastPanel])){
				trafficUp();

				pthread_mutex_lock(&board.boardMutex[lastPanel]);
				pthread_mutex_lock(&board.boardMutex[currPanel]);

				board.panel[currPanel] = buildCar(currPanel);

				pthread_mutex_unlock(&board.boardMutex[currPanel]);
				pthread_mutex_unlock(&board.boardMutex[lastPanel]);
			}
	}
}

void moveCar(int placeOnBoard){
	/* Input: None
	 * Output: None
	 * Description: Move a car on the board. A function that chooses whether a car exits or not and adds it to deletion stack
	 * */

	int nextPanel = (placeOnBoard == (ARR_SIZE-1))? 0 : placeOnBoard + 1; 
	int flag;
	Car* temp;

	// Test if we need to exit or not
	if(!((placeOnBoard % CORNER_DIFF) || board.panel[placeOnBoard]->justBorn)){ // Find if we are in a corner
		flag = rand() % 100 < FIN_PROB*100? TRUE : FALSE;
		if (flag){
			pthread_mutex_lock(&board.boardMutex[placeOnBoard]);
			pthread_mutex_lock(&board.delStackMutex);

			// Push to delete stack
			temp = board.delStack;
			board.delStack = board.panel[placeOnBoard];
			board.panel[placeOnBoard] -> nextDelCar = temp;

			// Evacuate panel
			board.panel[placeOnBoard] = NULL;			

			pthread_mutex_unlock(&board.delStackMutex);
			pthread_mutex_unlock(&board.boardMutex[placeOnBoard]);
		
			trafficDown();
			
			pthread_exit(NULL);
		}
	}

	// Continue driving
	if (!board.panel[nextPanel]){
		pthread_mutex_lock(&board.boardMutex[nextPanel]);
		pthread_mutex_lock(&board.boardMutex[placeOnBoard]);	
		
		board.panel[nextPanel] = board.panel[placeOnBoard];
		board.panel[placeOnBoard] = NULL;
		board.panel[nextPanel]->placeOnRoad = nextPanel;
		board.panel[nextPanel]->justBorn = FALSE;

		pthread_mutex_unlock(&board.boardMutex[placeOnBoard]);
		pthread_mutex_unlock(&board.boardMutex[nextPanel]);
	}
}

void trafficUp(){
	/* Input: None
	 * Output: None
	 * Description: Mutex for carCounter, to protect the 'carCounter++' commanad
	 * */

	pthread_mutex_lock(&board.counterMutex);

	board.carCounter++;

	pthread_mutex_unlock(&board.counterMutex);
}

void trafficDown(){
	/* Input: None
	 * Output: None
	 * Description: Mutex for carCounter, to protect the 'carCounter--' commanad
	 * */

	pthread_mutex_lock(&board.counterMutex);

	board.carCounter--;

	pthread_mutex_unlock(&board.counterMutex);
}

Car* buildCar(int placeOnBoard){
	/* Input: Initial place on board
	 * Output: A pointer to a new car
	 * Description: Place a new car on board. The position is given by caller.
	 * 		Later moves on to drive the car
	 * */

	Car* temp = (Car*)malloc(sizeof(Car));

	temp -> justBorn = TRUE;
	temp -> placeOnRoad = placeOnBoard;
	temp -> nextDelCar = NULL;
	if(pthread_create(& temp -> carThread, NULL, driveCar, temp)){
		perror("Error in creating!\n");
		freeAll(EXIT_FAILURE);
	}

	return temp;
}
