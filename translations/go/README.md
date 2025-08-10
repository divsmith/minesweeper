# Minesweeper in Go

This is a Go implementation of the classic Minesweeper game, based on the original C version.

## Features

- Three difficulty levels (Easy, Normal, Hard)
- High score tracking with SQLite database
- Timer that tracks game duration
- Flagging of suspected mines
- Flood fill algorithm for revealing empty areas
- Terminal-based user interface

## Dependencies

- Go 1.16 or higher
- termbox-go for terminal UI
- go-sqlite for database operations

## Building

```bash
make build
```

or

```bash
go build -o minesweeper
```

## Running

```bash
./minesweeper -e  # Easy mode
./minesweeper -n  # Normal mode
./minesweeper -h  # Hard mode
./minesweeper -s  # View high scores
```

## Controls

- Arrow keys: Move cursor
- Enter: Reveal tile
- Ctrl+F: Flag/unflag tile
- Ctrl+R: Restart game
- Ctrl+Q: Quit game

## Implementation Details

This Go version maintains the same functionality as the original C version but uses Go's concurrency features:

- Goroutines instead of pthreads for the timer
- Channels for communication instead of pipes
- termbox-go instead of ncurses for the terminal UI
- Pure Go SQLite driver instead of C SQLite library