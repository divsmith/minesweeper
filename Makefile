minesweeper: minesweeper.c
	gcc -ggdb -Wall -Werror minesweeper.c -o minesweeper -l pthread -ldl -D_REENTRANT -lncurses

clean:
	-rm minesweeper