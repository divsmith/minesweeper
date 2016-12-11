// Parker Smith
// CS3210
// Term Project
// Minesweeper

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <ncurses.h>
#include <sqlite3.h>
#include <sys/wait.h>
#include <sys/types.h>

void Usage();
void NewGame();
void PrintHud();
void PrintGrid();
void StartTimer();
void PlaceBombs();
void ViewScores();
void PrintBoard();
void InitializeGrid();
void PrintWholeGrid();
void Click(int i, int j);
void InitializeMutexes();
void InitializeScreens();
void SIGTERMHandler(int sig);
void FloodFill(int i, int j);
void CalculateAdjacentBombs();
void *TimerThread (void *args);
void FloodFillRecurse(int i, int j);
static int SQLTest(void *NotUsed, int argc, char **argv, char **azColName);
static int ViewScoresSQL(void *NotUsed, int argc, char **argv, char **azColName);

#define NAME_LENGTH 256

int res;
int score;
pid_t pid;
int boardX;
int boardY;
int seconds;
int screenX;
int screenY;
sqlite3 *db;
int initialX;
int initialY;
bool gameWon;
int pipes[2];
char sql[128];
bool gameLost;
int difficulty;
int gridRows = 10;
int gridCols = 10;
int numberOfBombs;
char readBuffer[6];
int bombsRemaining;
pthread_t a_thread;
WINDOW *hud, *board;
void *thread_result;
char *zErrorMsg = 0;
struct sigaction act;
char name[NAME_LENGTH];
bool sqlResults = false;
bool timerStarted = false;
int bombsCorrectlyFlagged;
pthread_mutex_t screenMutex;
pthread_mutex_t secondsMutex;
pthread_mutex_t wonLostMutex;
char writeBuffer[] = "second";

struct Tile {
	bool isMine;
	bool isFlagged;
	bool is3BVMarked;
	bool isFloodFillMarked;
	int adjacentMines;
};

struct Tile grid[10][10];

int main(int argc, char *argv[]) {

	// Error check the inputs
	if (argc > 2 || argc == 1)
	{
		Usage();
	}

	if (strlen(argv[1]) != 2 || argv[1][0] != '-')
	{
		Usage();
	}

	// Check if database exists.
	if (access("scores.db", F_OK) != -1)
	{
		res = sqlite3_open("scores.db", &db);

		if (res)
		{
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		// If not, initialize it.
		res = sqlite3_open("scores.db", &db);

		if (res)
		{
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(EXIT_FAILURE);
		}
		// Create the scores schema
		strcpy(sql, "create table scores("  \
						  "id integer primary key autoincrement unique,"
                          "name varchar(30)," \
                          "score int);");

        res = sqlite3_exec(db, sql, NULL, 0, &zErrorMsg);

		if (res != SQLITE_OK)
		{
			fprintf(stderr, "SQL error3: %s\n", zErrorMsg);
			sqlite3_free(zErrorMsg);
			exit(EXIT_FAILURE);
		}
	}

	// Set difficulty based on user flag.
	switch(argv[1][1])
	{
		case 'e':
			difficulty = 0;
			break;

		case 'n':
			difficulty = 1;
			break;

		case 'h':
			difficulty = 2;
			break;

		case 's':
			ViewScores();
			exit(0);
			break;

		default:
			Usage();
	}

	InitializeMutexes();

	InitializeScreens();

	StartTimer();

	NewGame();

	// After returning from the recursive NewGame() call,
	// start shutting down the program.

	// Kill the timer process and wait for it to finish.
	kill(pid, SIGTERM);
	waitpid(pid, (int*) 0, 0);

	// Wait for the timer update thread to stop.
	res = pthread_join(a_thread, &thread_result);
	if (res != 0) {
		perror("Thread join failed");
		exit(EXIT_FAILURE);
	}

	// Restore console settings.
	echo();
	nocbreak();
	endwin();

	// Close the database.
	sqlite3_close(db);
	exit(0);
}

void NewGame()
{
	// Set the bomb count based on difficulty.
	switch(difficulty)
	{
		case 0:
			numberOfBombs = 5;
			break;

		case 1:
			numberOfBombs = 15;
			break;

		case 2:
			numberOfBombs = 25;
			break;
	}

	InitializeGrid();
	PlaceBombs();
	CalculateAdjacentBombs();

	// Set the initial bombs remaining number.
	bombsRemaining = numberOfBombs;

	// Set the initial starting point of the board on screen.
	initialY = 1;
    initialX = (COLS / 2) - gridCols;

	// Copies of the starting point for moving around.
    screenY = initialY;
    screenX = initialX;

	// Location on the board that the cursor is hovering over.
    boardY = 0;
    boardX = 0;

    gameLost = false;
    gameWon = false;

	// Zero out the correct flag count.
    bombsCorrectlyFlagged = 0;

	// Mutex and zero out the seconds counter
	pthread_mutex_lock(&secondsMutex);
	seconds = 0;
	pthread_mutex_unlock(&secondsMutex);

	PrintHud();
	PrintBoard();

	int key;

	do
	{
		// This is the main event loop. It continually gets user input while
		// the game is running in a non blocking fashion.
		key = getch();

		// Respond to user arrow and keyboard inputs.
		if (key == KEY_LEFT && boardX > 0)
		{
			boardX--;
			screenX -= 2;
		}

		if (key == KEY_RIGHT && boardX < gridCols - 1)
		{
			boardX++;
			screenX += 2;
		}

		if (key == KEY_UP && boardY > 0)
		{
			boardY--;
			screenY--;
		}

		if (key == KEY_DOWN && boardY < gridRows - 1)
		{
			boardY++;
			screenY++;
		}

		if (key == 10)
		{
			// When the user presses enter over a space on the grid,
			// execute the click function for that space.
			Click(boardY, boardX);
		}

		if (key == 'f')
		{
			// Either flag or unflag the current space.
			if (!grid[boardY][boardX].isFloodFillMarked)
			{
				if (!grid[boardY][boardX].isFlagged)
				{
					grid[boardY][boardX].isFlagged = true;
					bombsRemaining--;

					if (grid[boardY][boardX].isMine)
					{
						bombsCorrectlyFlagged++;
					}
				}
				else
				{
					grid[boardY][boardX].isFlagged = false;

					if (grid[boardY][boardX].isMine)
					{
						bombsCorrectlyFlagged--;
					}
					bombsRemaining++;
				}

				if (bombsCorrectlyFlagged == numberOfBombs)
				{
					pthread_mutex_lock(&wonLostMutex);
					gameWon = true;
					pthread_mutex_unlock(&wonLostMutex);
				}
			}
		}

		// Refresh the board on every user event.
		PrintBoard();

		// And repeat until the user quits, restarts, wins, or loses the game.
	} while (key != 'q' && key != 'r' && !gameLost && !gameWon);

	// Once outside of the event loop, check to see whether the user won or lost.
	pthread_mutex_lock(&wonLostMutex);
	if (gameWon)
	{
		// Grab the seconds mutex and pull it to another variable.
		int gameSeconds;
		pthread_mutex_lock(&secondsMutex);
		gameSeconds = seconds;
		pthread_mutex_unlock(&secondsMutex);

		// Compute the score based on the time and difficulty.
		switch(difficulty)
		{
			case 0:
				score = 250 - gameSeconds;
				break;

			case 1:
				score = 500 - gameSeconds;
				break;

			case 2:
				score = 1000 - gameSeconds;
				break;
		}

		// Tell the user they won and show them their score.
		wclear(hud);
		wclear(board);

		mvwprintw(board, 1, (COLS / 2) - 10, "%s", "You Won!");
		mvwprintw(board, 3, (COLS / 2) - 10, "Your score was %d", score);

		wrefresh(hud);
		wrefresh(board);

		// Check whether the score is high enough to be saved.
		strcpy(sql, "select count(score), id, min(score) from scores");

		res = sqlite3_exec(db, sql, SQLTest, 0, &zErrorMsg);
		sleep(2);

		// Ask the user if they want to play again.
		wclear(board);
		mvwprintw(board, 5, (COLS / 2) - 19, "%s", "Press (r) to play again or (q) to quit");
		wrefresh(board);

		timeout(100000);
		key = getch();
	}

	if (gameLost)
	{
		// Wait for 3/4 second to show the user the mine they hit.
		usleep(750000);


		timeout(100000);
		// Ask the user if they want to play again.
		while (key != 'r' && key != 'q')
		{
			wclear(hud);
			wclear(board);

			mvwprintw(board, 1, (COLS / 2) - 5, "%s", "Game Over");
			mvwprintw(board, 3, (COLS / 2) - 19, "%s", "Press (r) to play again or (q) to quit");

			wrefresh(hud);
			wrefresh(board);


			key = getch();
		}

	}
	pthread_mutex_unlock(&wonLostMutex);

	// Restart the event loop if the user wants to play again.
	if (key == 'r')
	{
		timeout(100);
		NewGame();
	}
}

void StartTimer()
{
	// Only start the timer once for the entire life of the process.
	if (!timerStarted)
	{
		// Initialize pipes for communication.
		if (pipe(pipes) == 0)
		{
			// Fork out a second process to run the timer.
			pid = fork();

			switch(pid)
			{
				case -1:
					perror("fork failed");
					exit(1);
				case 0:
					// Setup the child process to handle the kill signal once
					// the game is over
					act.sa_handler = SIGTERMHandler;
					sigemptyset(&act.sa_mask);
					act.sa_flags = 0;
					sigaction(SIGTERM, &act, 0);

					// Continually signal over the IPC pipe every second.
					while (1)
					{
						write(pipes[1], writeBuffer, sizeof(writeBuffer));
						sleep(1);
					}

					break;

				default:
					// Back in the parent process, immediately close the write side
					// pipe since we won't be using it.
					close(pipes[1]);

					// Spin off a thread to catch the timer events thrown off by
					// the timer process.
					res = pthread_create(&a_thread, NULL, TimerThread, NULL);
					if (res != 0)
					{
						perror("thread creation failed");
						exit(EXIT_FAILURE);
					}
					break;
			}
		}
		else
		{
			exit(EXIT_FAILURE);
		}

		// Mark the timer as started so we don't have duplicate processes and threads.
		timerStarted = true;
	}
}

void *TimerThread(void *arg)
{
	while (read(pipes[0], readBuffer, sizeof(readBuffer)) > 0)
	{
		// Until EOF is received by the timer process,
		// increment seconds every time a message is received,
		//	then write the new time to the screen
		read(pipes[0], readBuffer, sizeof(readBuffer));

		pthread_mutex_lock(&secondsMutex);
		seconds++;
		pthread_mutex_unlock(&secondsMutex);


		pthread_mutex_lock(&wonLostMutex);
		if (!gameWon && !gameLost)
		{
			PrintHud();
		}
		pthread_mutex_unlock(&wonLostMutex);
		memset(readBuffer, '\0', sizeof(readBuffer));
	}

	return 0;
}

void SIGTERMHandler(int sig)
{
	// Close the write pipe when
	// the termination signal is
	// received so that the thread
	// on the other end will get EOF
	// and shut down.
	close(pipes[1]);
	exit(0);
}

void PrintBoard()
{
	// Get a mutex for writing to the screens.
	pthread_mutex_lock(&screenMutex);
	wclear(board);
	int currentY = initialY;
	int currentX = initialX;

	// Write out the board from the starting point.
	for (int i = 0; i < gridRows; i++)
	{
		for (int j = 0; j < gridCols; j++)
		{
			if (grid[i][j].isFloodFillMarked)
			{
				if (grid[i][j].isMine)
				{
					// If the current space is a mine that has been clicked on,
					// get a lock for the lost boolean and mark it true.
					mvwprintw(board, currentY, currentX, "%s", "X");
					pthread_mutex_lock(&wonLostMutex);
					gameLost = true;
					pthread_mutex_unlock(&wonLostMutex);
				}
				else
				{
					// Otherwise, if the space has been clicked on,
					// print out the number of adjacent mines.
					mvwprintw(board, currentY, currentX, "%d", grid[i][j].adjacentMines);
				}
			}
			else if (grid[i][j].isFlagged)
			{
				// If the space has been flagged, mark it accordingly.
				mvwprintw(board, currentY, currentX, "%s", "F");
			}
			else
			{
				// Otherwise, mark the space with a generic starting character.
				mvwprintw(board, currentY, currentX, "%s", "-");
			}

			// Jump 2 spaces to the left, because there is an extra space
			// on the screen between each horizontal tile on the grid.
			currentX += 2;
		}

		// Increment the current row
		currentY++;

		// Start the columns back over again.
		currentX = initialX;
	}

	mvwprintw(board, 13, 7, "%s", "Restart-(r) \tQuit-(q)\tFlag-(f)\tClick-(enter)");

	// Move the cursor back to where the user
	// had it.
	wmove(board, screenY, screenX);

	wrefresh(board);
	pthread_mutex_unlock(&screenMutex);
}

void PrintHud()
{
	// Get a mutex lock for writing to the screen.
	pthread_mutex_lock(&screenMutex);

	// Write out all the static info.
	wclear(hud);
	mvwprintw(hud, 1, (COLS / 2) - 6, "%s", "MINESWEEPER");
	char *diff;

	// Dynamically determine the difficulty.
	switch(difficulty)
	{
		case 0:
			diff = "Easy";
			break;

		case 1:
			diff = "Normal";
			break;

		case 2:
			diff = "Hard";
			break;
	}

	// Get a mutex lock for reading the seconds count.
	pthread_mutex_lock(&secondsMutex);
	if (seconds % 60 < 10)
	{
		// If there are less than 10 seconds remaining, write out an extra zero in the time
		// string so that it looks consistent with 2 digits.
		mvwprintw(hud, 3, (COLS / 2) - 30, "Difficulty: %s\tBombs Remaining: %d\tTime: %d:0%d", diff, bombsRemaining, (seconds / 60), (seconds % 60));
	}
	else
	{
		mvwprintw(hud, 3, (COLS / 2) - 30, "Difficulty: %s\tBombs Remaining: %d\tTime: %d:%d", diff, bombsRemaining, (seconds / 60), (seconds % 60));
	}
	pthread_mutex_unlock(&secondsMutex);

	// Move the cursor back to where it was
	// over the gameboard so the user can see
	// what they're doing.
	wmove(board, screenY, screenX);

	wrefresh(hud);
	wrefresh(board);

	pthread_mutex_unlock(&screenMutex);
}

void Click(int i, int j)
{
	FloodFill(i, j);
}

void FloodFillRecurse(int i, int j)
{
	// Determine whether to recursively call FloodFill
	// on a given tile based on whether or not it is a mine
	// and whether it's already marked as open.
	if (!grid[i][j].isMine && !grid[i][j].isFloodFillMarked)
	{
		grid[i][j].isFloodFillMarked = true;

		if (grid[i][j].adjacentMines == 0)
		{
			FloodFill(i, j);
		}
	}
}

void FloodFill(int i, int j)
{
	// Validate that i and j are within bounds.
	if (i < gridRows && j < gridCols)
	{
		// Mark the tile as clicked if it isn't flagged.
		if (!grid[i][j].isFlagged)
		{
			grid[i][j].isFloodFillMarked = true;
		}

		// If it's an empty non-mine tile, recursively call
		// FloodFill through FloodFill recurse on all adjacent
		// tiles.
		if (grid[i][j].adjacentMines == 0 && !grid[i][j].isMine)
		{
			if (!i == 0)
			{
				FloodFillRecurse(i - 1, j);
			}

			if (!(i == gridRows - 1))
			{
				FloodFillRecurse(i + 1, j);
			}

			if (!j == 0)
			{
				FloodFillRecurse(i, j - 1);
			}

			if (!(j == gridCols - 1))
			{
				FloodFillRecurse(i, j + 1);
			}

			if (i < (gridRows - 1) && j < (gridCols - 1))
			{
				FloodFillRecurse(i + 1, j + 1);
			}

			if (i < (gridRows - 1) && j > 0)
			{
				FloodFillRecurse(i + 1, j - 1);
			}

			if (i > 0 && j < (gridCols - 1))
			{
				FloodFillRecurse(i - 1, j + 1);
			}

			if (i > 0 && j > 0)
			{
				FloodFillRecurse(i - 1, j - 1);
			}
		}
	}
}

static int SQLTest(void *notUsed, int argc, char **argv, char **azColName)
{
	// This function determines whether a given score is high
	// enough to go into the database.
	int count = atoi(argv[0]);

    if (count >= 10)
    {
    	int lowestScore = atoi(argv[2]);

		if (score < lowestScore)
		{
			// If there are more than 10 entries in the database and
			// this score isn't higher than the lowest of them, it's
			// not going into the database.
			return 0;
		}
    }

	// Otherwise, we can ask the user for their name.
	nocbreak();
	echo();

	mvwprintw(board, 5, (COLS / 2) - 10, "%s", "Please enter name: ");

	wrefresh(board);

	wgetstr(board, name);

	cbreak();
	noecho();

	// And run the SQL query to add them to the database.
	if (count >= 10)
	{
		sprintf(sql, "delete from scores where id = %d; \ninsert into data(name, score) values(\"%s\", %d);", atoi(argv[1]), name, score);
	}
    else
    {
    	sprintf(sql, "insert into scores(name, score) values(\"%s\", %d);", name, score);
    }

	res = sqlite3_exec(db, sql, NULL, 0, &zErrorMsg);

	if (res != SQLITE_OK)
	{
		fprintf(stderr, "SQL error1: %s\n", zErrorMsg);
		sqlite3_free(zErrorMsg);
		exit(1);
	}

	// Give the user feedback that their score is safe.
	wclear(board);
	mvwprintw(board, 1, (COLS / 2) - 15, "%s", "Your score has been saved");
	wrefresh(board);

	// And return.
    return SQLITE_OK;
}

void ViewScores()
{
	strcpy(sql, "select name, score from scores order by score desc;");

	sqlResults = false;

  	res = sqlite3_exec(db, sql, ViewScoresSQL, 0, &zErrorMsg);
	if (res != SQLITE_OK)
	{
		fprintf(stderr, "SQL error2: %s\n", zErrorMsg);
		sqlite3_free(zErrorMsg);
		exit(1);
	}

	if (!sqlResults)
	{
		printf("No scores yet. Play a game and add one!\n");
	}
}

static int ViewScoresSQL(void *notUsed, int argc, char **argv, char **azColName)
{
	// Determine whether there are any scores to show.
	if (argv[0] != '\0')
	{
		// If so, let the calling function know
		sqlResults = true;
		for (int i = 0; i < argc; i += 2)
		{
			// Loop through and print out the scores.
			printf("%s", argv[i]);

			for (int j = 0; j < 10 - strlen(argv[i]); j++)
			{
				printf(" ");
			}

			printf("%s\n", argv[i + 1]);
		}
	}

	return 0;
}

void Usage()
{
	printf("Usage: minesweeper\n");
	printf("\t   -e (Easy)\n");
	printf("\t   -n (Normal)\n");
	printf("\t   -h (Hard)\n");
	printf("\t   -s (View High Scores)\n");

	exit(1);
}

void CalculateAdjacentBombs()
{
	// Calculate the mines adjacent to each
	// non mine position in the grid.
	for (int i = 0; i < gridRows; i++)
	{
		for (int j = 0; j < gridCols; j++)
		{
			if (!grid[i][j].isMine)
			{
				int adjacentMines = 0;

				if (!i == 0)
				{
					if (grid[i - 1][j].isMine)
					{
						adjacentMines++;
					}
				}

				if (!(i == gridRows - 1))
				{
					if (grid[i + 1][j].isMine)
					{
						adjacentMines++;
					}
				}

				if (!j == 0)
				{
					if (grid[i][j - 1].isMine)
					{
						adjacentMines++;
					}
				}

				if (!(j == gridCols - 1))
				{
					if (grid[i][j + 1].isMine)
					{
						adjacentMines++;
					}
				}

				if (i < (gridRows - 1) && j < (gridCols - 1))
				{
					if (grid[i + 1][j + 1].isMine)
					{
						adjacentMines++;
					}
				}

				if (i < (gridRows - 1) && j > 0)
				{
					if (grid[i + 1][j - 1].isMine)
					{
						adjacentMines++;
					}
				}

				if (i > 0 && j < (gridCols - 1))
				{
					if (grid[i - 1][j + 1].isMine)
					{
						adjacentMines++;
					}
				}

				if (i > 0 && j > 0)
				{
					if (grid[i - 1][j - 1].isMine)
					{
						adjacentMines++;
					}
				}

				grid[i][j].adjacentMines = adjacentMines;
			}
		}
	}
}

void PlaceBombs()
{
	// Randomly place numberOfBombs in the grid array.
	srand((unsigned)time(NULL));
	int bombRow;
	int bombCol;

	for (int i = 0; i < numberOfBombs; i++)
	{
		bombRow = rand() % gridRows;
		bombCol = rand() % gridCols;

		if (grid[bombRow][bombCol].isMine)
		{
			i--;
		}
		else
		{
			grid[bombRow][bombCol].isMine = true;
		}
	}
}

void InitializeGrid()
{
	// Set the array of Tile structs to their
	// initial starting values.
	for (int i = 0; i < gridRows; i++)
	{
		for (int j = 0; j < gridCols; j++)
		{
			grid[i][j].isMine = false;
			grid[i][j].isFlagged = false;
			grid[i][j].is3BVMarked = false;
			grid[i][j].isFloodFillMarked = false;
			grid[i][j].adjacentMines = 0;
		}
	}
}

void InitializeScreens()
{
	// Setup ncurses and the 2 screens that will
	// be used, 1 for the HUD and one for the gameboard.
	initscr();

    hud = newwin(5, COLS, 0, 0);
    board = newwin(LINES - 5, COLS, 5, 0);

    cbreak();
	noecho();
	timeout(100);
	keypad(stdscr, TRUE);
}

void InitializeMutexes()
{
	// Create and verify the 3 mutexes needed
	// for dealing with the variables that are
	// manipulated by the thread and the main
	// process.

	res = pthread_mutex_init(&secondsMutex, NULL);

	if (res != 0)
	{
		perror("Seconds mutex initialization failed");
		exit(EXIT_FAILURE);
	}

	res = pthread_mutex_init(&screenMutex, NULL);

	if (res != 0)
	{
		perror("Screen mutex initialization failed");
		exit(EXIT_FAILURE);
	}

	res = pthread_mutex_init(&wonLostMutex, NULL);

	if (res != 0)
	{
		perror("Won / Lost Mutex initialization failed");
		exit(EXIT_FAILURE);
	}
}