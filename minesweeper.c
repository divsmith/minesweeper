#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

struct Tile {
	bool isMine;
	bool isFlagged;
	bool is3BVMarked;
	bool isFloodFillMarked;
	int adjacentMines;
};

int main() {
	struct Tile grid[4][4];

	grid[0][0].adjacentMines = 4;
	grid[1][2].isMine = false;
	grid[1][3].isMine = false;

	printf("%d\n", grid[0][0].adjacentMines);
	printf("%d\n", grid[1][2].isMine);
	printf("%d\n", grid[1][3].isMine);

	if (!grid[1][2].isMine)
	{
		printf("%s", "not a mine");
	}

	if (grid[1][2].isMine) {
		printf("%s", "is a mine");
	}
	exit(0);
}