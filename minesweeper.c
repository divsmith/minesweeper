#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

void NewGame();
void PlaceBombs();
void InitializeGrid();
void PrintGrid();
void CalculateAdjacentBombs();
void FloodFill(int i, int j);
void FloodFillRecurse(int i, int j);
void PrintWholeGrid();

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
int numberOfBombs = 10;

int main() {

	NewGame();

	int clickRow = 0;
	int clickCol = 0;

	PrintWholeGrid();

	while (true)
	{
		scanf("%d", &clickRow);
		scanf("%d", &clickCol);

		FloodFill(clickRow, clickCol);

		PrintGrid();
	}
	
	exit(0);
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

void NewGame()
{
	InitializeGrid();
	PlaceBombs();

	CalculateAdjacentBombs();
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
			i++;
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