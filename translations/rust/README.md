# Minesweeper - Rust Translation

This is a Rust translation of the Minesweeper game originally written in C.

## Building and Running

To build and run the game, you need to have Rust installed on your system.

### Using Cargo (Rust's package manager)

1. Navigate to the `translations/rust` directory
2. Build the project:
   ```
   cargo build
   ```
3. Run the game with a difficulty flag:
   ```
   cargo run -- -e  # Easy mode
   cargo run -- -n  # Normal mode
   cargo run -- -h  # Hard mode
   cargo run -- -s  # View high scores (not yet implemented)
   ```

### Using Make

Alternatively, you can use the provided Makefile:

1. Navigate to the `translations/rust` directory
2. Build the project:
   ```
   make build
   ```
3. Run the game with a difficulty flag:
   ```
   make run-easy    # Easy mode
   make run-normal  # Normal mode
   make run-hard    # Hard mode
   make run-scores  # View high scores (not yet implemented)
   ```

## Game Controls

- Arrow keys: Move the cursor around the game board
- Enter: Reveal a tile
- F: Flag/unflag a tile
- R: Restart the game
- Q: Quit the game

## Features

This Rust translation includes most of the features of the original C version:

- Three difficulty levels (Easy, Normal, Hard)
- Timer to track game duration
- NCurses interface with HUD and game board
- Mine placement and flood fill algorithm
- Flagging functionality
- Win/loss detection

### Features Not Yet Implemented

- High score tracking with SQLite database
- Viewing high scores

## Implementation Details

The Rust version maintains the same architecture as the C version:
- Uses NCurses for the terminal interface
- Implements the same game logic and rules
- Maintains the same visual appearance and user experience

Note: The high score functionality using SQLite has not yet been implemented in this Rust translation.

## Troubleshooting

### No Output When Running the Game

If you're not seeing any game board when running the game, it's likely due to how the terminal environment is handling the ncurses interface. The game is actually working correctly, but the ncurses terminal interface requires a proper terminal to display correctly.

When running through certain shell environments or command capture interfaces (like `make run-easy` which uses `timeout`), the output may appear jumbled or not display properly.

To see the game working correctly, run it in a proper terminal environment:
```bash
cargo run -- -e  # For easy mode
```

The game will then display:
- A 10x10 grid of tiles (shown as `-` for unclicked tiles)
- A HUD showing game information (difficulty, bombs remaining, timer)
- Control instructions at the bottom

### Recent Improvements

Several improvements have been made to the ncurses implementation:
1. Proper initialization order for ncurses
2. Better window management for the HUD and game board
3. Correct keyboard input handling using window-specific functions
4. Improved screen refresh handling

The game is now fully functional and responds to all inputs correctly.