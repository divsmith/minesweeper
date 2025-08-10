package main

import (
	"database/sql"
	"fmt"
	"math/rand"
	"os"
	"time"

	_ "github.com/glebarez/go-sqlite"
	"github.com/nsf/termbox-go"
)

const (
	NameLength = 256
	GridRows   = 10
	GridCols   = 10
)

type Difficulty int

const (
	Easy Difficulty = iota
	Normal
	Hard
	Scores
)

type Tile struct {
	IsMine            bool
	IsFlagged         bool
	Is3BVMarked       bool
	IsFloodFillMarked bool
	AdjacentMines     int
}

type Game struct {
	grid                  [GridRows][GridCols]Tile
	res                   int
	score                 int
	boardX                int
	boardY                int
	seconds               int
	screenX               int
	screenY               int
	db                    *sql.DB
	initialX              int
	initialY              int
	gameWon               bool
	gameLost              bool
	difficulty            Difficulty
	numberOfBombs         int
	bombsRemaining        int
	name                  [NameLength]byte
	bombsCorrectlyFlagged int
	timerStarted          bool
	timerChan             chan bool
	quitChan              chan bool
}

func main() {
	if len(os.Args) > 2 || len(os.Args) == 1 {
		usage()
	}

	if len(os.Args[1]) != 2 || os.Args[1][0] != '-' {
		usage()
	}

	// Check if database exists
	if _, err := os.Stat("scores.db"); err == nil {
		db, err := sql.Open("sqlite", "./scores.db")
		if err != nil {
			fmt.Fprintf(os.Stderr, "Can't open database: %s\n", err)
			os.Exit(1)
		}
		defer db.Close()

		// Initialize termbox (only once)
		if !termbox.IsInit {
			err := termbox.Init()
			if err != nil {
				panic(err)
			}
			defer termbox.Close()

			// Set up termbox options
			termbox.SetInputMode(termbox.InputEsc)
		}

		// Main game loop
		for {
			game := &Game{
				db:        db,
				timerChan: make(chan bool),
				quitChan:  make(chan bool),
			}

			// Set difficulty based on user flag
			switch os.Args[1][1] {
			case 'e':
				game.difficulty = Easy
			case 'n':
				game.difficulty = Normal
			case 'h':
				game.difficulty = Hard
			case 's':
				game.viewScores()
				break
			default:
				usage()
			}

			// Break the loop if we're viewing scores
			if os.Args[1][1] == 's' {
				break
			}

			// Start timer
			game.startTimer()

			// Start the game
			shouldQuit := game.newGame()

			// Close the game properly
			game.close()

			// If the game indicated we should quit, break the loop
			if shouldQuit {
				break
			}
		}
	} else {
		// If not, initialize it
		db, err := sql.Open("sqlite", "./scores.db")
		if err != nil {
			fmt.Fprintf(os.Stderr, "Can't open database: %s\n", err)
			os.Exit(1)
		}
		defer db.Close()

		// Create the scores schema
		sqlStmt := `
		CREATE TABLE scores (
			id INTEGER PRIMARY KEY AUTOINCREMENT UNIQUE,
			name VARCHAR(30),
			score INTEGER
		);`
		_, err = db.Exec(sqlStmt)
		if err != nil {
			fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
			os.Exit(1)
		}

		// Initialize termbox (only once)
		if !termbox.IsInit {
			err := termbox.Init()
			if err != nil {
				panic(err)
			}
			defer termbox.Close()

			// Set up termbox options
			termbox.SetInputMode(termbox.InputEsc)
		}

		// Main game loop
		for {
			game := &Game{
				db:        db,
				timerChan: make(chan bool),
				quitChan:  make(chan bool),
			}

			// Set difficulty based on user flag
			switch os.Args[1][1] {
			case 'e':
				game.difficulty = Easy
			case 'n':
				game.difficulty = Normal
			case 'h':
				game.difficulty = Hard
			case 's':
				game.viewScores()
				break
			default:
				usage()
			}

			// Break the loop if we're viewing scores
			if os.Args[1][1] == 's' {
				break
			}

			// Start timer
			game.startTimer()

			// Start the game
			shouldQuit := game.newGame()

			// Close the game properly
			game.close()

			// If the game indicated we should quit, break the loop
			if shouldQuit {
				break
			}
		}
	}
}

func usage() {
	fmt.Printf("Usage: minesweeper [option]\n")
	fmt.Printf("\t-e (Easy: 5 mines)\n")
	fmt.Printf("\t-n (Normal: 15 mines)\n")
	fmt.Printf("\t-h (Hard: 25 mines)\n")
	fmt.Printf("\t-s (View High Scores)\n")
	os.Exit(1)
}

func (g *Game) newGame() bool {
	// Set the bomb count based on difficulty
	switch g.difficulty {
	case Easy:
		g.numberOfBombs = 5
	case Normal:
		g.numberOfBombs = 15
	case Hard:
		g.numberOfBombs = 25
	}

	g.initializeGrid()
	g.placeBombs()
	g.calculateAdjacentBombs()

	// Set the initial bombs remaining number
	g.bombsRemaining = g.numberOfBombs

	// Set the initial starting point of the board on screen below the HUD
	g.initialY = 5
	// We'll calculate initialX based on terminal width when rendering

	// Copies of the starting point for moving around
	g.screenY = g.initialY
	g.screenX = g.initialX

	// Location on the board that the cursor is hovering over
	g.boardY = 0
	g.boardX = 0

	g.gameLost = false
	g.gameWon = false

	// Zero out the correct flag count
	g.bombsCorrectlyFlagged = 0

	// Zero out the seconds counter
	g.seconds = 0

	g.printHud()
	g.printBoard()

	// Main event loop
	for {
		switch ev := termbox.PollEvent(); ev.Type {
		case termbox.EventKey:
			switch ev.Key {
			case termbox.KeyArrowLeft:
				if g.boardX > 0 {
					g.boardX--
					g.screenX -= 2
				}
			case termbox.KeyArrowRight:
				if g.boardX < GridCols-1 {
					g.boardX++
					g.screenX += 2
				}
			case termbox.KeyArrowUp:
				if g.boardY > 0 {
					g.boardY--
					g.screenY--
				}
			case termbox.KeyArrowDown:
				if g.boardY < GridRows-1 {
					g.boardY++
					g.screenY++
				}
			case termbox.KeyEnter, termbox.KeyCtrlJ:
				// When the user presses enter over a space on the grid,
				// execute the click function for that space
				g.click(g.boardY, g.boardX)
			case termbox.KeyCtrlF:
				// Either flag or unflag the current space
				if !g.grid[g.boardY][g.boardX].IsFloodFillMarked {
					if !g.grid[g.boardY][g.boardX].IsFlagged {
						g.grid[g.boardY][g.boardX].IsFlagged = true
						g.bombsRemaining--

						if g.grid[g.boardY][g.boardX].IsMine {
							g.bombsCorrectlyFlagged++
						}
					} else {
						g.grid[g.boardY][g.boardX].IsFlagged = false

						if g.grid[g.boardY][g.boardX].IsMine {
							g.bombsCorrectlyFlagged--
						}
						g.bombsRemaining++
					}

					// Check if all bombs have been correctly flagged
					if g.bombsCorrectlyFlagged == g.numberOfBombs {
						g.gameWon = true
					}
				}
			case termbox.KeyCtrlQ:
				// Quit the game
				return true
			case termbox.KeyCtrlR:
				// Restart the game
				return false
			}

			// Check for win/loss conditions
			if g.gameLost || g.gameWon {
				break
			}

			// Refresh the board on every user event
			g.printBoard()

		case termbox.EventResize:
			// Handle terminal resize
			g.printHud()
			g.printBoard()
		case termbox.EventError:
			panic(ev.Err)
		}

		// Check win/loss after each event
		if g.gameLost || g.gameWon {
			break
		}
	}

	// Once outside of the event loop, check to see whether the user won or lost
	if g.gameWon {
		// Signal the timer to stop
		select {
		case g.quitChan <- true:
		default:
		}

		// Compute the score based on the time and difficulty
		switch g.difficulty {
		case Easy:
			g.score = 250 - g.seconds
		case Normal:
			g.score = 500 - g.seconds
		case Hard:
			g.score = 1000 - g.seconds
		}

		// Tell the user they won and show them their score
		termbox.Clear(termbox.ColorDefault, termbox.ColorDefault)
		g.printWinMessage()
		termbox.Flush()

		// Check whether the score is high enough to be saved
		g.checkHighScore()

		// Show the win message again to display the restart/quit prompt
		g.printWinMessage()

		// Ask the user if they want to play again
		for {
			switch ev := termbox.PollEvent(); ev.Type {
			case termbox.EventKey:
				switch ev.Key {
				case termbox.KeyCtrlR:
					// Restart the game
					return false
				case termbox.KeyCtrlQ:
					// Quit the game
					return true
				}
			}
		}
	}

	if g.gameLost {
		// Signal the timer to stop
		select {
		case g.quitChan <- true:
		default:
		}

		// Wait for 3/4 second to show the user the mine they hit
		time.Sleep(750 * time.Millisecond)

		// Ask the user if they want to play again
		g.printGameOverMessage()
		termbox.Flush()

		for {
			switch ev := termbox.PollEvent(); ev.Type {
			case termbox.EventKey:
				switch ev.Key {
				case termbox.KeyCtrlR:
					// Restart the game
					return false
				case termbox.KeyCtrlQ:
					// Quit the game
					return true
				}
			}
		}
	}

	// Default return (should not reach here)
	return true
}

func (g *Game) startTimer() {
	if !g.timerStarted {
		go func() {
			ticker := time.NewTicker(1 * time.Second)
			defer ticker.Stop()

			for {
				select {
				case <-ticker.C:
					g.seconds++
					g.printHud()
				case <-g.quitChan:
					return
				}
			}
		}()

		g.timerStarted = true
	}
}

func (g *Game) printBoard() {
	// Only clear the game board area, not the HUD area
	width, height := termbox.Size()
	for y := g.initialY; y < height && y < g.initialY+GridRows; y++ {
		for x := 0; x < width; x++ {
			termbox.SetCell(x, y, ' ', termbox.ColorDefault, termbox.ColorDefault)
		}
	}

	// Calculate initialX based on terminal width
	termWidth, _ := termbox.Size()
	g.initialX = (termWidth / 2) - GridCols

	// Update screen positions
	g.screenX = g.initialX + (g.boardX * 2)
	g.screenY = g.initialY + g.boardY

	currentY := g.initialY
	currentX := g.initialX

	// Write out the board from the starting point
	for i := 0; i < GridRows; i++ {
		for j := 0; j < GridCols; j++ {
			var char rune
			var fg, bg termbox.Attribute

			if g.grid[i][j].IsFloodFillMarked {
				if g.grid[i][j].IsMine {
					char = 'X'
					fg = termbox.ColorRed
					// If the current space is a mine that has been clicked on,
					// mark the game as lost
					g.gameLost = true
				} else {
					// Otherwise, if the space has been clicked on,
					// print out the number of adjacent mines
					char = rune('0' + g.grid[i][j].AdjacentMines)
					fg = termbox.ColorDefault
				}
			} else if g.grid[i][j].IsFlagged {
				// If the space has been flagged, mark it accordingly
				char = 'F'
				fg = termbox.ColorBlue
			} else {
				// Otherwise, mark the space with a generic starting character
				char = '-'
				fg = termbox.ColorDefault
			}

			termbox.SetCell(currentX, currentY, char, fg, bg)
			// Jump 2 spaces to the left, because there is an extra space
			// on the screen between each horizontal tile on the grid
			currentX += 2
		}

		// Increment the current row
		currentY++

		// Start the columns back over again
		currentX = g.initialX
	}

	// Draw instructions
	instructions := "Restart-(Ctrl+R)    Quit-(Ctrl+Q)    Flag-(Ctrl+F)    Click-(Enter)"
	instY := g.initialY + GridRows + 1
	instX := 7
	for i, char := range instructions {
		termbox.SetCell(instX+i, instY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	// Move the cursor back to where the user had it
	termbox.SetCursor(g.screenX, g.screenY)

	termbox.Flush()
}

func (g *Game) printHud() {
	// Clear the HUD area
	width, height := termbox.Size()
	for y := 0; y < 5 && y < height; y++ {
		for x := 0; x < width; x++ {
			termbox.SetCell(x, y, ' ', termbox.ColorDefault, termbox.ColorDefault)
		}
	}

	// Write out all the static info
	title := "MINESWEEPER"
	titleX := (width / 2) - (len(title) / 2)
	titleY := 1
	for i, char := range title {
		termbox.SetCell(titleX+i, titleY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	// Dynamically determine the difficulty
	var diff string
	switch g.difficulty {
	case Easy:
		diff = "Easy"
	case Normal:
		diff = "Normal"
	case Hard:
		diff = "Hard"
	}

	// Format time as MM:SS
	minutes := g.seconds / 60
	seconds := g.seconds % 60

	var hudText string
	if seconds < 10 {
		hudText = fmt.Sprintf("Difficulty: %s    Bombs Remaining: %d    Time: %d:0%d", diff, g.bombsRemaining, minutes, seconds)
	} else {
		hudText = fmt.Sprintf("Difficulty: %s    Bombs Remaining: %d    Time: %d:%d", diff, g.bombsRemaining, minutes, seconds)
	}

	hudX := (width / 2) - 30
	hudY := 3
	for i, char := range hudText {
		termbox.SetCell(hudX+i, hudY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	termbox.Flush()
}

func (g *Game) printWinMessage() {
	width, _ := termbox.Size()

	// Clear screen
	termbox.Clear(termbox.ColorDefault, termbox.ColorDefault)

	winText := "You Won!"
	winX := (width / 2) - (len(winText) / 2)
	winY := 1
	for i, char := range winText {
		termbox.SetCell(winX+i, winY, char, termbox.ColorGreen, termbox.ColorDefault)
	}

	scoreText := fmt.Sprintf("Your score was %d", g.score)
	scoreX := (width / 2) - (len(scoreText) / 2)
	scoreY := 3
	for i, char := range scoreText {
		termbox.SetCell(scoreX+i, scoreY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	restartText := "Press (Ctrl+R) to play again or (Ctrl+Q) to quit"
	restartX := (width / 2) - (len(restartText) / 2)
	restartY := 5
	for i, char := range restartText {
		termbox.SetCell(restartX+i, restartY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	termbox.Flush()
}

func (g *Game) printGameOverMessage() {
	width, _ := termbox.Size()

	// Clear screen
	termbox.Clear(termbox.ColorDefault, termbox.ColorDefault)

	gameOverText := "Game Over"
	gameOverX := (width / 2) - (len(gameOverText) / 2)
	gameOverY := 1
	for i, char := range gameOverText {
		termbox.SetCell(gameOverX+i, gameOverY, char, termbox.ColorRed, termbox.ColorDefault)
	}

	restartText := "Press (Ctrl+R) to play again or (Ctrl+Q) to quit"
	restartX := (width / 2) - (len(restartText) / 2)
	restartY := 3
	for i, char := range restartText {
		termbox.SetCell(restartX+i, restartY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	termbox.Flush()
}

func (g *Game) click(i, j int) {
	g.floodFill(i, j)

	// Check if all non-mine tiles have been revealed
	if g.allNonMinesRevealed() {
		g.gameWon = true
	}
}

// allNonMinesRevealed checks if all non-mine tiles have been revealed
func (g *Game) allNonMinesRevealed() bool {
	revealedCount := 0
	nonMineCount := 0

	for i := 0; i < GridRows; i++ {
		for j := 0; j < GridCols; j++ {
			// Count non-mine tiles
			if !g.grid[i][j].IsMine {
				nonMineCount++
				// Count revealed non-mine tiles
				if g.grid[i][j].IsFloodFillMarked {
					revealedCount++
				}
			}
		}
	}

	return revealedCount == nonMineCount
}

func (g *Game) floodFillRecurse(i, j int) {
	// Determine whether to recursively call FloodFill
	// on a given tile based on whether or not it is a mine
	// and whether it's already marked as open
	if !g.grid[i][j].IsMine && !g.grid[i][j].IsFloodFillMarked {
		g.grid[i][j].IsFloodFillMarked = true

		if g.grid[i][j].AdjacentMines == 0 {
			g.floodFill(i, j)
		}
	}
}

func (g *Game) floodFill(i, j int) {
	// Validate that i and j are within bounds
	if i < GridRows && j < GridCols {
		// Mark the tile as clicked if it isn't flagged
		if !g.grid[i][j].IsFlagged {
			g.grid[i][j].IsFloodFillMarked = true
		}

		// If it's an empty non-mine tile, recursively call
		// FloodFill through FloodFill recurse on all adjacent
		// tiles
		if g.grid[i][j].AdjacentMines == 0 && !g.grid[i][j].IsMine {
			if i != 0 {
				g.floodFillRecurse(i-1, j)
			}

			if i != GridRows-1 {
				g.floodFillRecurse(i+1, j)
			}

			if j != 0 {
				g.floodFillRecurse(i, j-1)
			}

			if j != GridCols-1 {
				g.floodFillRecurse(i, j+1)
			}

			if i < GridRows-1 && j < GridCols-1 {
				g.floodFillRecurse(i+1, j+1)
			}

			if i < GridRows-1 && j > 0 {
				g.floodFillRecurse(i+1, j-1)
			}

			if i > 0 && j < GridCols-1 {
				g.floodFillRecurse(i-1, j+1)
			}

			if i > 0 && j > 0 {
				g.floodFillRecurse(i-1, j-1)
			}
		}
	}
}

func (g *Game) checkHighScore() {
	// Query to check if score is in top 10
	row := g.db.QueryRow("SELECT COUNT(score), MIN(score) FROM scores")

	var count int
	var minScore sql.NullInt64

	err := row.Scan(&count, &minScore)
	if err != nil {
		fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
		return
	}

	if count >= 10 {
		if int64(g.score) < minScore.Int64 {
			// If there are more than 10 entries in the database and
			// this score isn't higher than the lowest of them, it's
			// not going into the database
			return
		}
	}

	// Otherwise, we can ask the user for their name
	g.promptForName()

	// And run the SQL query to add them to the database
	if count >= 10 {
		// Find the ID of the lowest score
		row := g.db.QueryRow("SELECT id FROM scores WHERE score = ? ORDER BY id LIMIT 1", minScore.Int64)
		var id int
		err := row.Scan(&id)
		if err != nil {
			fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
			return
		}

		// Delete the lowest score and insert the new one using parameterized queries
		_, err = g.db.Exec("DELETE FROM scores WHERE id = ?", id)
		if err != nil {
			fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
			return
		}

		_, err = g.db.Exec("INSERT INTO scores(name, score) VALUES(?, ?)", string(g.name[:]), g.score)
		if err != nil {
			fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
			return
		}
	} else {
		_, err = g.db.Exec("INSERT INTO scores(name, score) VALUES(?, ?)", string(g.name[:]), g.score)
		if err != nil {
			fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
			return
		}
	}

	// Give the user feedback that their score is safe
	g.printScoreSavedMessage()
	time.Sleep(2 * time.Second)
}

func (g *Game) promptForName() {
	termbox.Clear(termbox.ColorDefault, termbox.ColorDefault)
	width, _ := termbox.Size()

	promptText := "Please enter name: "
	promptX := (width / 2) - (len(promptText) / 2)
	promptY := 3

	for i, char := range promptText {
		termbox.SetCell(promptX+i, promptY, char, termbox.ColorDefault, termbox.ColorDefault)
	}

	termbox.SetCursor(promptX+len(promptText), promptY)
	termbox.Flush()

	// Read name using termbox
	name := make([]rune, 0, NameLength)
	for {
		switch ev := termbox.PollEvent(); ev.Type {
		case termbox.EventKey:
			switch ev.Key {
			case termbox.KeyEnter:
				// Convert to byte array
				nameStr := string(name)
				copy(g.name[:], nameStr)
				return
			case termbox.KeyBackspace, termbox.KeyBackspace2:
				if len(name) > 0 {
					name = name[:len(name)-1]
					// Clear the character
					termbox.SetCell(promptX+len(promptText)+len(name), promptY, ' ', termbox.ColorDefault, termbox.ColorDefault)
					termbox.SetCursor(promptX+len(promptText)+len(name), promptY)
					termbox.Flush()
				}
			default:
				if ev.Ch != 0 {
					name = append(name, ev.Ch)
					termbox.SetCell(promptX+len(promptText)+len(name)-1, promptY, ev.Ch, termbox.ColorDefault, termbox.ColorDefault)
					termbox.SetCursor(promptX+len(promptText)+len(name), promptY)
					termbox.Flush()
				}
			}
		}
	}
}

func (g *Game) printScoreSavedMessage() {
	termbox.Clear(termbox.ColorDefault, termbox.ColorDefault)
	width, _ := termbox.Size()

	message := "Your score has been saved"
	msgX := (width / 2) - (len(message) / 2)
	msgY := 1

	for i, char := range message {
		termbox.SetCell(msgX+i, msgY, char, termbox.ColorGreen, termbox.ColorDefault)
	}

	termbox.Flush()
}

func (g *Game) viewScores() {
	rows, err := g.db.Query("SELECT name, score FROM scores ORDER BY score DESC")
	if err != nil {
		fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
		os.Exit(1)
	}
	defer rows.Close()

	// Check if there are any scores
	hasScores := false

	fmt.Println("High Scores:")
	fmt.Println("------------")

	for rows.Next() {
		var name string
		var score int

		err := rows.Scan(&name, &score)
		if err != nil {
			fmt.Fprintf(os.Stderr, "SQL error: %s\n", err)
			os.Exit(1)
		}

		hasScores = true

		// Format output: name (padded to 10 chars) and score
		fmt.Printf("%-10s %d\n", name, score)
	}

	if !hasScores {
		fmt.Println("No scores yet. Play a game and add one!")
	}
}

func (g *Game) calculateAdjacentBombs() {
	// Calculate the mines adjacent to each
	// non mine position in the grid
	for i := 0; i < GridRows; i++ {
		for j := 0; j < GridCols; j++ {
			if !g.grid[i][j].IsMine {
				adjacentMines := 0

				if i != 0 {
					if g.grid[i-1][j].IsMine {
						adjacentMines++
					}
				}

				if i != GridRows-1 {
					if g.grid[i+1][j].IsMine {
						adjacentMines++
					}
				}

				if j != 0 {
					if g.grid[i][j-1].IsMine {
						adjacentMines++
					}
				}

				if j != GridCols-1 {
					if g.grid[i][j+1].IsMine {
						adjacentMines++
					}
				}

				if i < GridRows-1 && j < GridCols-1 {
					if g.grid[i+1][j+1].IsMine {
						adjacentMines++
					}
				}

				if i < GridRows-1 && j > 0 {
					if g.grid[i+1][j-1].IsMine {
						adjacentMines++
					}
				}

				if i > 0 && j < GridCols-1 {
					if g.grid[i-1][j+1].IsMine {
						adjacentMines++
					}
				}

				if i > 0 && j > 0 {
					if g.grid[i-1][j-1].IsMine {
						adjacentMines++
					}
				}

				g.grid[i][j].AdjacentMines = adjacentMines
			}
		}
	}
}

func (g *Game) close() {
	// Signal the timer to stop
	select {
	case g.quitChan <- true:
	default:
	}
}

func (g *Game) placeBombs() {
	// Randomly place numberOfBombs in the grid array
	rand.Seed(time.Now().UnixNano())
	bombRow := 0
	bombCol := 0

	for i := 0; i < g.numberOfBombs; i++ {
		bombRow = rand.Intn(GridRows)
		bombCol = rand.Intn(GridCols)

		if g.grid[bombRow][bombCol].IsMine {
			i--
		} else {
			g.grid[bombRow][bombCol].IsMine = true
		}
	}
}

func (g *Game) initializeGrid() {
	// Set the array of Tile structs to their
	// initial starting values
	for i := 0; i < GridRows; i++ {
		for j := 0; j < GridCols; j++ {
			g.grid[i][j] = Tile{
				IsMine:            false,
				IsFlagged:         false,
				Is3BVMarked:       false,
				IsFloodFillMarked: false,
				AdjacentMines:     0,
			}
		}
	}
}
