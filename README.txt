Parker Smith
CS3210
Final Project Usage / Technical Description


Once the project is built with 'make', execute it with './minesweeper'.
Running the executable with no options will display the usage statement showing the options.

Options:
	-e (play on easy mode)
	-n (play on normal mode)
	-h (play on hard mode)
	-s (view the high scores)

Run the executable as './minesweeper -e' to start the game on easy mode. The timer at the top
left shows how long the game has been running for, and the bombs remaining counter shows how
many mines are left to find and flag. Use the arrow keys to navigate the gameboard. To uncover
a tile, press 'Enter'. If it is a bomb, the game will end. If it is not a bomb, the tile will
change to an number indicating the number of bombs adjacent to it. If a given tile is suspected
to be a bomb, pressing 'f' while over it will flag it and decrement the bomb remaining counter.
The game is won by correctly flagging all of the bombs on the gameboard.

Once the game is either won or lost, the option is given to either play again by pressing 'r'
or quit by pressing 'q'. Both of these options can also be used at any time during gameplay.

If the game is won and the score is in the top 10 highest scores, the user is asked for their
name. The name and score is then saved in the database.

To view the highest scores, run './minesweeper -s'. If no scores have been saved in the database,
a message indicating so will appear. Otherwise, up to 10 names and scores will appear.




Technical Description / Required Functions.

The timer was the most difficult part of the game. While it would have been easier to just use
a thread to keep time, I decided to fulfil the requirement for IPC by spinning off a process
and using it as the timer. Every second, it sends a message through a pipe back to the main
process. Those messages are caught by a thread of the main process, which then updates the game
timer. When the user quits and the main process is shutting down, it sends a terminate signal
to the timer process. This signal is caught and used to close the write end of the IPC pipe.
The timer thread in the main process then receives the EOF signal and closes.

The UI is handled via ncurses, using noecho() and cbreak() modes for most user input. Two different
windows are used, one for the HUD with the timer, remaining bombs, and difficulty, and the other
for the gameboard below.

The File I/O requirement is fulfilled with the Sqlite database for holding the high scores. Several
different queries and callbacks are used based on whether the high scores are being displayed,
or determining whether a given score is high enough to be saved. The database used is 'scores.db'.
The program creates this database and it's schema automatically. I've included a secondary database,
named 'demo.db' preloaded with the scores shown in my demo video for ease of testing. Just rename
it from 'demo.db' to 'scores.db' and the application will automatically use it.

