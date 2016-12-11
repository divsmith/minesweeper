#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <curses.h>
#include <ncurses.h>
#include <sqlite3.h>

void NewGame();
void PlaceBombs();
void InitializeGrid();
void PrintGrid();
void CalculateAdjacentBombs();
void FloodFill(int i, int j);
void FloodFillRecurse(int i, int j);
void PrintWholeGrid();
void Usage();
void ViewScores();
void Click(int i, int j);
void StartTimer();
void *TimerThread (void *args);
void SIGTERMHandler(int sig);
void InitializeMutexes();
void InitializeScreens();
void PrintHud();
void PrintBoard();
static int SQLTest(void *NotUsed, int argc, char **argv, char **azColName);
static int ViewScoresSQL(void *NotUsed, int argc, char **argv, char **azColName);

#define NAME_LENGTH 256

struct Tile {
	bool isMine;
	bool isFlagged;
	bool is3BVMarked;
	bool isFloodFillMarked;
	int adjacentMines;
};

struct Tile grid[10][10];
int gridRows = 10;
int gridCols = 10;
int numberOfBombs;
int bombsRemaining;
int bombsCorrectlyFlagged;
bool gameLost;
bool gameWon;
pid_t pid;
int seconds;
pthread_t a_thread;
int res;
int pipes[2];
struct sigaction act;
char writeBuffer[] = "second";
char readBuffer[6];
void *thread_result;
bool timerStarted = false;
pthread_mutex_t secondsMutex;
pthread_mutex_t screenMutex;
pthread_mutex_t wonLostMutex;
WINDOW *hud, *board;
int screenX;
int screenY;
int boardX;
int boardY;
int initialX;
int initialY;
int difficulty;
sqlite3 *db;
char sql[128];
char *zErrorMsg = 0;
int score;
char name[NAME_LENGTH];
bool sqlResults = false;

int main(int argc, char *argv[]) {

	if (argc > 2 || argc == 1)
	{
		Usage();
	}

	if (strlen(argv[1]) != 2 || argv[1][0] != '-')
	{
		Usage();
	}


	if (access("scores", F_OK) != -1)
	{
		res = sqlite3_open("scores", &db);

		if (res)
		{
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		res = sqlite3_open("scores", &db);

		if (res)
		{
			fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
			sqlite3_close(db);
			exit(EXIT_FAILURE);
		}

		strcpy(sql, "create table data("  \
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

	kill(pid, SIGTERM);

	waitpid(pid, (int*) 0, 0);

	res = pthread_join(a_thread, &thread_result);
	if (res != 0) {
		perror("Thread join failed");
		exit(EXIT_FAILURE);
	}

	echo();
	nocbreak();
	endwin();
	sqlite3_close(db);
	exit(0);
}

void PrintBoard()
{
	pthread_mutex_lock(&screenMutex);
	wclear(board);
	int currentY = initialY;
	int currentX = initialX;

	for (int i = 0; i < gridRows; i++)
	{
		for (int j = 0; j < gridCols; j++)
		{
			if (grid[i][j].isFloodFillMarked)
			{
				if (grid[i][j].isMine)
				{
					mvwprintw(board, currentY, currentX, "%s", "X");
					pthread_mutex_lock(&wonLostMutex);
					gameLost = true;
					pthread_mutex_unlock(&wonLostMutex);
				}
				else
				{
					mvwprintw(board, currentY, currentX, "%d", grid[i][j].adjacentMines);
				}
			}
			else if (grid[i][j].isFlagged)
			{
				mvwprintw(board, currentY, currentX, "%s", "F");
			}
			else
			{
				mvwprintw(board, currentY, currentX, "%s", "-");
			}

			currentX += 2;
		}

		currentY++;
		currentX = initialX;
	}

	wmove(board, screenY, screenX);

	wrefresh(board);
	pthread_mutex_unlock(&screenMutex);
}

void PrintHud()
{
	pthread_mutex_lock(&screenMutex);
	wclear(hud);
	mvwprintw(hud, 1, (COLS / 2) - 6, "%s", "MINESWEEPER");
	char *diff;

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

	pthread_mutex_lock(&secondsMutex);
	if (seconds % 60 < 10)
	{
		mvwprintw(hud, 3, (COLS / 2) - 30, "Difficulty: %s\tBombs Remaining: %d\tTime: %d:0%d", diff, bombsRemaining, (seconds / 60), (seconds % 60));
	}
	else
	{
		mvwprintw(hud, 3, (COLS / 2) - 30, "Difficulty: %s\tBombs Remaining: %d\tTime: %d:%d", diff, bombsRemaining, (seconds / 60), (seconds % 60));
	}
	pthread_mutex_unlock(&secondsMutex);

	wmove(board, screenY, screenX);

	wrefresh(hud);
	wrefresh(board);

	pthread_mutex_unlock(&screenMutex);
}

void InitializeScreens()
{
	initscr();

    //	if (!has_colors())
    //	{
    //		endwin();
    //		fprintf(stderr, "Error - no color support on this terminal\n");
    //		exit(1);
    //	}
    //
    //	if (start_color() != OK)
    //	{
    //		endwin();
    //		fprintf(stderr, "Error - could not initialize colors\n");
    //		exit(2);
    //	}

    hud = newwin(5, COLS, 0, 0);
    board = newwin(LINES - 5, COLS, 5, 0);

    cbreak();
	noecho();
	timeout(100);
	keypad(stdscr, TRUE);

    //	for (int i = 0; i < 5; i++)
    //	{
    //		for (int j = 0; j < COLS; j++)
    //		{
    //			mvwaddch(hud, i, j, 'H');
    //			wrefresh(hud);
    //			usleep(1000);
    //		}
    //	}
    //
    //	for (int i = 0; i < LINES - 5; i++)
    //	{
    //		for (int j = 0; j < COLS; j++)
    //		{
    //			mvwaddch(board, i, j, 'B');
    //			wrefresh(board);
    //			usleep(1000);
    //		}
    //	}
    //
    //	PrintHud();
    //
    //	sleep(2);

    //	wmove(board, 10, 10);
    //	wrefresh(board);
    //
    //	sleep(2);

    //	mvwprintw(hud, 1, 5, "%s", "Hello World!");
    //	wrefresh(hud);
    //
    //	sleep(3);
    //
    //	mvwprintw(hud, 1, 20, "%s", "test");
    //	wrefresh(hud);
}

void InitializeMutexes()
{
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

void Click(int i, int j)
{
	FloodFill(i, j);
}

void NewGame()
{
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
	bombsRemaining = numberOfBombs;

	initialY = 1;
    initialX = (COLS / 2) - gridCols;

    screenY = initialY;
    screenX = initialX;

    boardY = 0;
    boardX = 0;

    gameLost = false;
    gameWon = false;

    bombsCorrectlyFlagged = 0;

	pthread_mutex_lock(&secondsMutex);
	seconds = 0;
	pthread_mutex_unlock(&secondsMutex);

	PrintHud();
	PrintBoard();

	int key;

	do
	{
		key = getch();

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
			Click(boardY, boardX);
		}

		if (key == 'f')
		{
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

		PrintBoard();
	} while (key != 'q' && key != 'r' && !gameLost && !gameWon);

	pthread_mutex_lock(&wonLostMutex);
	if (gameWon)
	{
		int gameSeconds;

		pthread_mutex_lock(&secondsMutex);
		gameSeconds = seconds;
		pthread_mutex_unlock(&secondsMutex);

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

		wclear(hud);
		wclear(board);

		mvwprintw(board, 1, (COLS / 2) - 10, "%s", "You Won!");
		mvwprintw(board, 3, (COLS / 2) - 10, "Your score was %d", score);

		wrefresh(hud);
		wrefresh(board);

		strcpy(sql, "select count(score), id, min(score) from data");

		res = sqlite3_exec(db, sql, SQLTest, 0, &zErrorMsg);
		sleep(2);

		wclear(board);
		mvwprintw(board, 5, (COLS / 2) - 19, "%s", "Press (r) to play again or (q) to quit");
		wrefresh(board);

		timeout(100000);
		key = getch();
	}

	if (gameLost)
	{
		usleep(750000);
		timeout(100000);

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

	if (key == 'r')
	{
		timeout(100);
		NewGame();
	}
}

static int ViewScoresSQL(void *notUsed, int argc, char **argv, char **azColName)
{
	if (argv[0] != '\0')
	{
		sqlResults = true;
		for (int i = 0; i < argc; i += 2)
		{
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

static int SQLTest(void *notUsed, int argc, char **argv, char **azColName)
{
	int count = atoi(argv[0]);

    if (count >= 10)
    {
    	int lowestScore = atoi(argv[2]);

		if (score < lowestScore)
		{
			return 0;
		}
    }

	nocbreak();
	echo();

	mvwprintw(board, 5, (COLS / 2) - 10, "%s", "Please enter name: ");

	wrefresh(board);

	wgetstr(board, name);

	cbreak();
	noecho();

	if (count >= 10)
	{
		sprintf(sql, "delete from data where id = %d; \ninsert into data(name, score) values(\"%s\", %d);", atoi(argv[1]), name, score);
	}
    else
    {
    	sprintf(sql, "insert into data(name, score) values(\"%s\", %d);", name, score);
    }

	res = sqlite3_exec(db, sql, NULL, 0, &zErrorMsg);

	if (res != SQLITE_OK)
	{
		fprintf(stderr, "SQL error1: %s\n", zErrorMsg);
		sqlite3_free(zErrorMsg);
		exit(1);
	}

	wclear(board);
	mvwprintw(board, 1, (COLS / 2) - 15, "%s", "Your score has been saved");
	wrefresh(board);

    return SQLITE_OK;
}

void *TimerThread(void *arg)
{
	while (read(pipes[0], readBuffer, sizeof(readBuffer)) > 0)
	{
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
	close(pipes[1]);
	exit(0);
}

void StartTimer()
{
	if (!timerStarted)
	{
		if (pipe(pipes) == 0)
		{
			pid = fork();

			switch(pid)
			{
				case -1:
					perror("fork failed");
					exit(1);
				case 0:
					// Child process
					act.sa_handler = SIGTERMHandler;
					sigemptyset(&act.sa_mask);
					act.sa_flags = 0;

					sigaction(SIGTERM, &act, 0);

					while (1)
					{
						write(pipes[1], writeBuffer, sizeof(writeBuffer));
						sleep(1);
					}

					break;

				default:
					close(pipes[1]);

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

		timerStarted = true;
	}
}

void ViewScores()
{
	strcpy(sql, "select name, score from data order by score desc;");

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

void Usage()
{
	printf("Usage: minesweeper\n");
	printf("\t   -e (Easy)\n");
	printf("\t   -n (Normal)\n");
	printf("\t   -h (Hard)\n");
	printf("\t   -s (View High Scores)\n");

	exit(1);
}

void FloodFillRecurse(int i, int j)
{
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
	if (i < gridRows && j < gridCols)
	{
		if (!grid[i][j].isFlagged)
		{
			grid[i][j].isFloodFillMarked = true;
		}

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

void CalculateAdjacentBombs()
{
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