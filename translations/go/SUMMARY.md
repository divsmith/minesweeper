# Minesweeper - Go Implementation

This directory contains a Go implementation of the Minesweeper game that was originally written in C.

## Files

- `main.go` - The main game implementation
- `game_test.go` - Unit tests for the game logic
- `go.mod` - Go module definition
- `go.sum` - Go module checksums
- `Makefile` - Build and run instructions
- `README.md` - Documentation for the Go version

## Key Changes from C to Go

1. **Concurrency Model**:
   - Replaced pthreads with goroutines
   - Replaced pipes and fork with channels
   - Simplified the timer implementation using Go's time package

2. **UI Library**:
   - Replaced ncurses with termbox-go for terminal UI

3. **Database**:
   - Replaced SQLite3 C library with pure Go SQLite driver

4. **Build System**:
   - Replaced Makefile with Go modules
   - Added proper dependency management

## Features Maintained

- Three difficulty levels (Easy, Normal, Hard)
- High score tracking with persistent SQLite database
- Timer that tracks game duration
- Flagging of suspected mines
- Flood fill algorithm for revealing empty areas
- Terminal-based user interface with the same controls

## Building and Running

```bash
# Build the application
make build

# Run the application
./minesweeper -e  # Easy mode
./minesweeper -n  # Normal mode
./minesweeper -h  # Hard mode
./minesweeper -s  # View high scores

# Run tests
make test
```

## Controls

- Arrow keys: Move cursor
- Enter: Reveal tile
- Ctrl+F: Flag/unflag tile
- Ctrl+R: Restart game
- Ctrl+Q: Quit game