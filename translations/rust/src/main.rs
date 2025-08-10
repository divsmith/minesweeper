// Parker Smith
// CS3210
// Term Project
// Minesweeper - Rust Translation

use ncurses::*;
use std::time::{Duration, Instant};
use std::thread;
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};

// Constants
const GRID_ROWS: usize = 10;
const GRID_COLS: usize = 10;

// Tile structure
#[derive(Clone, Copy)]
struct Tile {
    is_mine: bool,
    is_flagged: bool,
    is_3bv_marked: bool,
    is_flood_fill_marked: bool,
    adjacent_mines: i32,
}

impl Tile {
    fn new() -> Tile {
        Tile {
            is_mine: false,
            is_flagged: false,
            is_3bv_marked: false,
            is_flood_fill_marked: false,
            adjacent_mines: 0,
        }
    }
}

// Game state structure
struct GameState {
    grid: [[Tile; GRID_COLS]; GRID_ROWS],
    score: i32,
    board_x: i32,
    board_y: i32,
    seconds: Arc<Mutex<i32>>,
    screen_x: i32,
    screen_y: i32,
    initial_x: i32,
    initial_y: i32,
    game_won: Arc<AtomicBool>,
    game_lost: Arc<AtomicBool>,
    difficulty: i32,
    number_of_bombs: i32,
    bombs_remaining: i32,
    bombs_correctly_flagged: i32,
    timer_started: bool,
    hud: WINDOW,
    board: WINDOW,
}

impl GameState {
    fn new() -> GameState {
        GameState {
            grid: [[Tile::new(); GRID_COLS]; GRID_ROWS],
            score: 0,
            board_x: 0,
            board_y: 0,
            seconds: Arc::new(Mutex::new(0)),
            screen_x: 0,
            screen_y: 0,
            initial_x: 0,
            initial_y: 0,
            game_won: Arc::new(AtomicBool::new(false)),
            game_lost: Arc::new(AtomicBool::new(false)),
            difficulty: 0,
            number_of_bombs: 0,
            bombs_remaining: 0,
            bombs_correctly_flagged: 0,
            timer_started: false,
            hud: std::ptr::null_mut(),
            board: std::ptr::null_mut(),
        }
    }

    fn initialize_grid(&mut self) {
        for i in 0..GRID_ROWS {
            for j in 0..GRID_COLS {
                self.grid[i][j] = Tile::new();
            }
        }
    }

    fn place_bombs(&mut self) {
        use std::time::{SystemTime, UNIX_EPOCH};
        use oorandom::Rand32;
        
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("Time went backwards")
            .as_secs() as u64;
        
        let mut rng = Rand32::new(now);
        
        let mut bombs_placed = 0;
        while bombs_placed < self.number_of_bombs {
            let bomb_row = (rng.rand_u32() % GRID_ROWS as u32) as usize;
            let bomb_col = (rng.rand_u32() % GRID_COLS as u32) as usize;
            
            if !self.grid[bomb_row][bomb_col].is_mine {
                self.grid[bomb_row][bomb_col].is_mine = true;
                bombs_placed += 1;
            }
        }
    }

    fn calculate_adjacent_bombs(&mut self) {
        for i in 0..GRID_ROWS {
            for j in 0..GRID_COLS {
                if !self.grid[i][j].is_mine {
                    let mut adjacent_mines = 0;
                    
                    // Check all 8 surrounding cells
                    for di in -1..=1 {
                        for dj in -1..=1 {
                            if di == 0 && dj == 0 {
                                continue;
                            }
                            
                            let ni = i as i32 + di;
                            let nj = j as i32 + dj;
                            
                            if ni >= 0 && ni < GRID_ROWS as i32 && 
                               nj >= 0 && nj < GRID_COLS as i32 {
                                if self.grid[ni as usize][nj as usize].is_mine {
                                    adjacent_mines += 1;
                                }
                            }
                        }
                    }
                    
                    self.grid[i][j].adjacent_mines = adjacent_mines;
                }
            }
        }
    }

    fn flood_fill_recurse(&mut self, i: i32, j: i32) {
        if i >= 0 && i < GRID_ROWS as i32 && 
           j >= 0 && j < GRID_COLS as i32 &&
           !self.grid[i as usize][j as usize].is_mine && 
           !self.grid[i as usize][j as usize].is_flood_fill_marked {
            
            self.grid[i as usize][j as usize].is_flood_fill_marked = true;
            
            if self.grid[i as usize][j as usize].adjacent_mines == 0 {
                self.flood_fill(i, j);
            }
        }
    }

    fn flood_fill(&mut self, i: i32, j: i32) {
        if i >= 0 && i < GRID_ROWS as i32 && 
           j >= 0 && j < GRID_COLS as i32 {
            
            // Mark the tile as clicked if it isn't flagged
            if !self.grid[i as usize][j as usize].is_flagged {
                self.grid[i as usize][j as usize].is_flood_fill_marked = true;
            }
            
            // If it's an empty non-mine tile, recursively call FloodFill
            if self.grid[i as usize][j as usize].adjacent_mines == 0 && 
               !self.grid[i as usize][j as usize].is_mine {
                
                // Check all 8 surrounding cells
                for di in -1..=1 {
                    for dj in -1..=1 {
                        if di == 0 && dj == 0 {
                            continue;
                        }
                        
                        let ni = i + di;
                        let nj = j + dj;
                        
                        self.flood_fill_recurse(ni, nj);
                    }
                }
            }
        }
    }

    fn click(&mut self, i: i32, j: i32) {
        self.flood_fill(i, j);
    }

    fn print_board(&mut self) {
        wclear(self.board);
        let mut current_y = self.initial_y;
        let mut current_x = self.initial_x;
        
        // Write out the board from the starting point
        for i in 0..GRID_ROWS {
            for j in 0..GRID_COLS {
                if self.grid[i][j].is_flood_fill_marked {
                    if self.grid[i][j].is_mine {
                        // If the current space is a mine that has been clicked on,
                        // mark the game as lost
                        mvwprintw(self.board, current_y, current_x, "X");
                        self.game_lost.store(true, Ordering::Relaxed);
                    } else {
                        // Otherwise, if the space has been clicked on,
                        // print out the number of adjacent mines
                        let s = self.grid[i][j].adjacent_mines.to_string();
                        mvwprintw(self.board, current_y, current_x, &s);
                    }
                } else if self.grid[i][j].is_flagged {
                    // If the space has been flagged, mark it accordingly
                    mvwprintw(self.board, current_y, current_x, "F");
                } else {
                    // Otherwise, mark the space with a generic starting character
                    mvwprintw(self.board, current_y, current_x, "-");
                }
                
                // Jump 2 spaces to the left, because there is an extra space
                // on the screen between each horizontal tile on the grid
                current_x += 2;
            }
            
            // Increment the current row
            current_y += 1;
            
            // Start the columns back over again
            current_x = self.initial_x;
        }
        
        mvwprintw(self.board, 13, 7, "Restart-(r) \tQuit-(q)\tFlag-(f)\tClick-(enter)");
        
        // Move the cursor back to where the user had it
        wmove(self.board, self.screen_y, self.screen_x);
        
        wrefresh(self.board);
    }

    fn print_hud(&mut self) {
        wclear(self.hud);
        
        // Write out all the static info
        mvwprintw(self.hud, 1, (COLS() / 2) - 6, "MINESWEEPER");
        
        let diff = match self.difficulty {
            0 => "Easy",
            1 => "Normal",
            2 => "Hard",
            _ => "Unknown",
        };
        
        // Get the seconds count
        let seconds_val = *self.seconds.lock().unwrap();
        
        let time_str = if seconds_val % 60 < 10 {
            format!("Difficulty: {}\tBombs Remaining: {}\tTime: {}:{:02}", 
                    diff, self.bombs_remaining, seconds_val / 60, seconds_val % 60)
        } else {
            format!("Difficulty: {}\tBombs Remaining: {}\tTime: {}:{}",
                    diff, self.bombs_remaining, seconds_val / 60, seconds_val % 60)
        };
        
        mvwprintw(self.hud, 3, (COLS() / 2) - 30, &time_str);
        
        wrefresh(self.hud);
    }

    fn initialize_screens(&mut self) {
        // Setup ncurses
        initscr();
        cbreak();
        noecho();
        keypad(stdscr(), true);
        
        self.hud = newwin(5, COLS(), 0, 0);
        self.board = newwin(LINES() - 5, COLS(), 5, 0);
        
        nodelay(self.board, true);
        keypad(self.board, true);
        
        // Make getch non-blocking but with a timeout to prevent excessive CPU usage
        timeout(50);
        
        // Refresh both windows
        wrefresh(self.hud);
        wrefresh(self.board);
    }

    fn start_timer(&mut self) {
        if !self.timer_started {
            // Clone values for the thread
            let seconds_clone = Arc::clone(&self.seconds);
            let game_won_clone = Arc::clone(&self.game_won);
            let game_lost_clone = Arc::clone(&self.game_lost);
            
            // Start the timer thread
            thread::spawn(move || {
                let start_time = Instant::now();
                loop {
                    thread::sleep(Duration::from_secs(1));
                    
                    // Update seconds
                    let elapsed = start_time.elapsed().as_secs() as i32;
                    *seconds_clone.lock().unwrap() = elapsed;
                    
                    // Check if game is still running
                    if !game_won_clone.load(Ordering::Relaxed) && 
                       !game_lost_clone.load(Ordering::Relaxed) {
                        // In the C version, PrintHud would be called here
                        // In Rust, we'll handle this differently in the event loop
                    }
                }
            });
            
            self.timer_started = true;
        }
    }

    fn new_game(&mut self) {
        // Set the bomb count based on difficulty
        self.number_of_bombs = match self.difficulty {
            0 => 5,   // Easy
            1 => 15,  // Normal
            2 => 25,  // Hard
            _ => 5,   // Default to easy
        };
        
        self.initialize_grid();
        self.place_bombs();
        self.calculate_adjacent_bombs();
        
        // Set the initial bombs remaining number
        self.bombs_remaining = self.number_of_bombs;
        
        // Set the initial starting point of the board on screen
        self.initial_y = 1;
        self.initial_x = (COLS() / 2) - GRID_COLS as i32;
        
        // Copies of the starting point for moving around
        self.screen_y = self.initial_y;
        self.screen_x = self.initial_x;
        
        // Location on the board that the cursor is hovering over
        self.board_y = 0;
        self.board_x = 0;
        
        self.game_lost.store(false, Ordering::Relaxed);
        self.game_won.store(false, Ordering::Relaxed);
        
        // Zero out the correct flag count
        self.bombs_correctly_flagged = 0;
        
        // Zero out the seconds counter
        *self.seconds.lock().unwrap() = 0;
        
        self.print_hud();
        self.print_board();
        
        // Main event loop
        loop {
            // This is the main event loop. It continually gets user input while
            // the game is running in a non blocking fashion.
            let key = wgetch(self.board);
            
            // Only process input if we got a real keypress
            if key != -1 {
                // Respond to user arrow and keyboard inputs
                if key == KEY_LEFT && self.board_x > 0 {
                    self.board_x -= 1;
                    self.screen_x -= 2;
                }
                
                if key == KEY_RIGHT && self.board_x < GRID_COLS as i32 - 1 {
                    self.board_x += 1;
                    self.screen_x += 2;
                }
                
                if key == KEY_UP && self.board_y > 0 {
                    self.board_y -= 1;
                    self.screen_y -= 1;
                }
                
                if key == KEY_DOWN && self.board_y < GRID_ROWS as i32 - 1 {
                    self.board_y += 1;
                    self.screen_y += 1;
                }
                
                // Move the cursor to the new position
                wmove(self.board, self.screen_y, self.screen_x);
                wrefresh(self.board);
                
                if key == 10 {  // Enter key
                    // When the user presses enter over a space on the grid,
                    // execute the click function for that space.
                    self.click(self.board_y, self.board_x);
                    self.print_board();
                    self.print_hud();
                }
                
                if key == 'f' as i32 {
                    // Either flag or unflag the current space.
                    let y = self.board_y as usize;
                    let x = self.board_x as usize;
                    
                    if !self.grid[y][x].is_flood_fill_marked {
                        if !self.grid[y][x].is_flagged {
                            self.grid[y][x].is_flagged = true;
                            self.bombs_remaining -= 1;
                            
                            if self.grid[y][x].is_mine {
                                self.bombs_correctly_flagged += 1;
                            }
                        } else {
                            self.grid[y][x].is_flagged = false;
                            
                            if self.grid[y][x].is_mine {
                                self.bombs_correctly_flagged -= 1;
                            }
                            self.bombs_remaining += 1;
                        }
                        
                        if self.bombs_correctly_flagged == self.number_of_bombs {
                            self.game_won.store(true, Ordering::Relaxed);
                        }
                        
                        self.print_board();
                        self.print_hud();
                    }
                }
                
                // Check exit conditions
                if key == 'q' as i32 || key == 'r' as i32 || 
                   self.game_lost.load(Ordering::Relaxed) || 
                   self.game_won.load(Ordering::Relaxed) {
                    
                    if self.game_won.load(Ordering::Relaxed) {
                        // Calculate score based on time and difficulty
                        let game_seconds = *self.seconds.lock().unwrap();
                        self.score = match self.difficulty {
                            0 => 250 - game_seconds,  // Easy
                            1 => 500 - game_seconds,  // Normal
                            2 => 1000 - game_seconds, // Hard
                            _ => 250 - game_seconds,  // Default to easy
                        };
                        
                        wclear(self.hud);
                        wclear(self.board);
                        
                        mvwprintw(self.board, 1, (COLS() / 2) - 10, "You Won!");
                        
                        let score_text = format!("Your score was {}", self.score);
                        mvwprintw(self.board, 3, (COLS() / 2) - 10, &score_text);
                        
                        wrefresh(self.hud);
                        wrefresh(self.board);
                        
                        // Wait a moment to show the win message
                        thread::sleep(Duration::from_millis(1000));
                        
                        // Ask the user if they want to play again
                        wclear(self.board);
                        
                        mvwprintw(self.board, 5, (COLS() / 2) - 19, "Press (r) to play again or (q) to quit");
                        wrefresh(self.board);
                        
                        // Wait for user input
                        loop {
                            let key = getch();
                            if key != -1 && (key == 'r' as i32 || key == 'q' as i32) {
                                if key == 'r' as i32 {
                                    // Reset the game state for a new game
                                    self.reset_game();
                                    break;
                                } else {
                                    return;
                                }
                            }
                            // Small delay to prevent excessive CPU usage
                            thread::sleep(Duration::from_millis(50));
                        }
                    }
                    
                    if self.game_lost.load(Ordering::Relaxed) {
                        // Show the mine that was hit
                        self.print_board();
                        
                        // Wait for 3/4 second to show the user the mine they hit
                        thread::sleep(Duration::from_millis(750));
                        
                        // Ask the user if they want to play again
                        wclear(self.hud);
                        wclear(self.board);
                        
                        mvwprintw(self.board, 1, (COLS() / 2) - 5, "Game Over");
                        
                        mvwprintw(self.board, 3, (COLS() / 2) - 19, "Press (r) to play again or (q) to quit");
                        
                        wrefresh(self.hud);
                        wrefresh(self.board);
                        
                        // Wait for user input
                        loop {
                            let key = getch();
                            if key != -1 && (key == 'r' as i32 || key == 'q' as i32) {
                                if key == 'r' as i32 {
                                    // Reset the game state for a new game
                                    self.reset_game();
                                    break;
                                } else {
                                    return;
                                }
                            }
                            // Small delay to prevent excessive CPU usage
                            thread::sleep(Duration::from_millis(50));
                        }
                    }
                    
                    if key == 'r' as i32 {
                        // Reset the game state for a new game
                        self.reset_game();
                        continue; // Continue the main loop
                    }
                    
                    if key == 'q' as i32 {
                        return;
                    }
                }
            }
            
            // Update HUD every so often to show timer
            self.print_hud();
            
            // Small delay to prevent excessive CPU usage
            thread::sleep(Duration::from_millis(50));
            
            // Refresh the board window to ensure updates are visible
            wrefresh(self.board);
        }
    }
    
    fn reset_game(&mut self) {
        self.initialize_grid();
        self.place_bombs();
        self.calculate_adjacent_bombs();
        self.bombs_remaining = self.number_of_bombs;
        self.bombs_correctly_flagged = 0;
        self.board_x = 0;
        self.board_y = 0;
        self.screen_x = self.initial_x;
        self.screen_y = self.initial_y;
        self.game_won.store(false, Ordering::Relaxed);
        self.game_lost.store(false, Ordering::Relaxed);
        *self.seconds.lock().unwrap() = 0;
        self.print_hud();
        self.print_board();
    }
    
    fn usage() {
        println!("Usage: minesweeper");
        println!("\t   -e (Easy)");
        println!("\t   -n (Normal)");
        println!("\t   -h (Hard)");
        println!("\t   -s (View High Scores)");
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    
    // Error check the inputs
    if args.len() > 2 || args.len() == 1 {
        GameState::usage();
        return;
    }
    
    if args[1].len() != 2 || !args[1].starts_with('-') {
        GameState::usage();
        return;
    }
    
    let mut game = GameState::new();
    
    // Set difficulty based on user flag
    match args[1].chars().nth(1).unwrap() {
        'e' => game.difficulty = 0,
        'n' => game.difficulty = 1,
        'h' => game.difficulty = 2,
        's' => {
            // TODO: Implement view scores
            // game.view_scores();
            println!("Viewing scores is not yet implemented in this Rust translation.");
            return;
        },
        _ => {
            GameState::usage();
            return;
        }
    }
    
    game.initialize_screens();
    game.start_timer();
    game.new_game();
    
    // Restore console settings
    echo();
    nocbreak();
    endwin();
}