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

void NewGame()
{
	InitializeGrid();
	PlaceBombs();
	CalculateAdjacentBombs();

	pthread_mutex_lock(&secondsMutex);
	seconds = 0;
	pthread_mutex_unlock(&secondsMutex);
}

void *TimerThread(void *arg)
{
	while (read(pipes[0], readBuffer, sizeof(readBuffer)) > 0)
	{
		read(pipes[0], readBuffer, sizeof(readBuffer));

		pthread_mutex_lock(&secondsMutex);
		seconds++;
		pthread_mutex_unlock(&secondsMutex);
		printf("%d\n", seconds);
		memset(readBuffer, '\0', sizeof(readBuffer));
	}

	return 0;
}

void ouch(int sig)
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
					act.sa_handler = ouch;
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

					res = pthread_mutex_init(&secondsMutex, NULL);

					if (res != 0)
					{
						perror("Seconds mutex initialization failed");
						exit(EXIT_FAILURE);
					}

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

int main(int argc, char *argv[]) {

	if (argc > 2 || argc == 1)
	{
		Usage();
	}

	if (strlen(argv[1]) != 2 || argv[1][0] != '-')
	{
		Usage();
	}

	switch(argv[1][1])
	{
		case 'e':
			numberOfBombs = 5;
			break;

		case 'n':
			numberOfBombs = 15;
			break;

		case 'h':
			numberOfBombs = 25;
			break;

		case 's':
			ViewScores();
			exit(0);
			break;

		default:
			Usage();
	}

	StartTimer();

	NewGame();

	// Play game here

	sleep(5);

	NewGame();

	sleep(7);

	kill(pid, SIGTERM);

	waitpid(pid, (int*) 0, 0);

	res = pthread_join(a_thread, &thread_result);
	if (res != 0) {
		perror("Thread join failed");
		exit(EXIT_FAILURE);
	}

	exit(0);
}

void Click(int i, int j)
{
	FloodFill(i, j);
}

void ViewScores()
{
	printf("View high scores here\n");
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
		grid[i][j].isFloodFillMarked = true;

		if (grid[i][j].adjacentMines == 0)
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

void PrintGrid()
{
	for (int i = 0; i < gridRows; i++)
	{
		for (int j = 0; j < gridCols; j++)
		{
			if (grid[i][j].isFloodFillMarked)
			{
				if (grid[i][j].isMine)
				{
					printf("%s", "X");
				}
				else
				{
					printf("%d", grid[i][j].adjacentMines);
				}
			}
			else
			{
				printf("-");
			}

			printf(" ");
		}
		printf("\n");
	}
}

void PrintWholeGrid()
{
	for (int i = 0; i < gridRows; i++)
	{
		for (int j = 0; j < gridCols; j++)
		{
			if (grid[i][j].isMine)
			{
				printf("%s", "X");
			}
			else
			{
				printf("%d", grid[i][j].adjacentMines);
			}

			printf(" ");
		}
		printf("\n");
	}
}

void PlaceBombs()
{
	srand((unsigned)time(NULL));
	int bombRow;
	int bombCol;

	for (int i = 0; i < numberOfBombs; i++)
	{
		bombRow = rand() % 10;
		bombCol = rand() % 10;

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