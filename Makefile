minesweeper: minesweeper.c
	gcc -ggdb -Wall -Werror minesweeper.c sqlite3.c -o minesweeper -l pthread -ldl -D_REENTRANT -lncurses

clean:
	-rm minesweeper