minesweeper: minesweeper.c
	gcc -ggdb -Wall -Werror minesweeper.c -o minesweeper -l pthread -ldl

clean:
	-rm minesweeper