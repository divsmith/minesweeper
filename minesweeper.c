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
int numberOfBombs = 5;

int main() {

	NewGame();
//	grid[0][0].adjacentMines = 4;
//	grid[1][2].isMine = false;
//	grid[1][3].isMine = false;
//
//	printf("%d\n", grid[0][0].adjacentMines);
//	printf("%d\n", grid[1][2].isMine);
//	printf("%d\n", grid[1][3].isMine);
//
//	if (!grid[1][2].isMine)
//	{
//		printf("%s", "not a mine");
//	}
//
//	if (grid[1][2].isMine) {
//		printf("%s", "is a mine");
//	}
	exit(0);
}

void NewGame()
{
	InitializeGrid();
	PlaceBombs();

	PrintGrid();

	CalculateAdjacentBombs();
}

//void CalculateAdjacentBombs()
//{
//	for (int i = 0; i < gridRows; i++)
//	{
//		for (int j = 0; j < gridCols; j++)
//		{
//			int adjacentMines = 0;
//
//			if ()
//		}
//	}
//}

void PrintGrid()
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