package main

import (
	"database/sql"
	"os"
	"testing"

	_ "github.com/glebarez/go-sqlite"
)

// Test that the game initializes correctly
func TestGameInitialization(t *testing.T) {
	// Create a temporary database for testing
	dbFile := "test_scores.db"
	defer os.Remove(dbFile)

	game := &Game{
		timerChan: make(chan bool),
		quitChan:  make(chan bool),
	}

	// Test database initialization
	db, err := sql.Open("sqlite", dbFile)
	if err != nil {
		t.Fatalf("Failed to open database: %v", err)
	}
	game.db = db
	defer db.Close()

	// Test grid initialization
	game.initializeGrid()
	
	// Check that all tiles are initialized correctly
	for i := 0; i < GridRows; i++ {
		for j := 0; j < GridCols; j++ {
			tile := game.grid[i][j]
			if tile.IsMine || tile.IsFlagged || tile.Is3BVMarked || tile.IsFloodFillMarked || tile.AdjacentMines != 0 {
				t.Errorf("Tile [%d][%d] not initialized correctly", i, j)
			}
		}
	}
}

// Test that bombs are placed correctly
func TestPlaceBombs(t *testing.T) {
	game := &Game{
		numberOfBombs: 5,
	}

	game.initializeGrid()
	game.placeBombs()

	// Count the actual bombs placed
	bombCount := 0
	for i := 0; i < GridRows; i++ {
		for j := 0; j < GridCols; j++ {
			if game.grid[i][j].IsMine {
				bombCount++
			}
		}
	}

	if bombCount != game.numberOfBombs {
		t.Errorf("Expected %d bombs, got %d", game.numberOfBombs, bombCount)
	}
}