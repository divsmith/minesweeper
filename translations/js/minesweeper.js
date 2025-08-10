/**
 * Minesweeper Game Implementation
 * Vanilla JavaScript version
 */

class Tile {
    constructor() {
        this.isMine = false;       // Whether tile contains a mine
        this.isFlagged = false;    // Whether tile is flagged
        this.isRevealed = false;   // Whether tile is revealed
        this.adjacentMines = 0;    // Count of adjacent mines
    }

    /**
     * Reveal the tile
     */
    reveal() {
        if (!this.isFlagged) {
            this.isRevealed = true;
        }
    }

    /**
     * Toggle flag on the tile
     */
    toggleFlag() {
        if (!this.isRevealed) {
            this.isFlagged = !this.isFlagged;
        }
    }
}

class Game {
    constructor(rows, cols, mines) {
        this.rows = rows;
        this.cols = cols;
        this.mineCount = mines;
        this.board = [];            // Grid of Tile objects
        this.gameState = 'idle';    // idle/playing/won/lost
        this.timer = null;          // Game timer reference
        this.bombsRemaining = mines;    // Remaining bombs to flag
        this.startTime = null;      // Game start timestamp
        this.elapsedTime = 0;       // Elapsed time in seconds
        this.timerElement = null;   // DOM element for timer
        this.bombCountElement = null; // DOM element for bomb count

        this.initializeBoard();
    }

    /**
     * Initialize the game board
     */
    initializeBoard() {
        this.board = [];
        for (let row = 0; row < this.rows; row++) {
            this.board[row] = [];
            for (let col = 0; col < this.cols; col++) {
                this.board[row][col] = new Tile();
            }
        }
        this.bombsRemaining = this.mineCount;
    }

    /**
     * Place mines randomly on the board
     */
    placeMines() {
        let minesPlaced = 0;
        while (minesPlaced < this.mineCount) {
            const row = Math.floor(Math.random() * this.rows);
            const col = Math.floor(Math.random() * this.cols);
            
            if (!this.board[row][col].isMine) {
                this.board[row][col].isMine = true;
                minesPlaced++;
            }
        }
    }

    /**
     * Calculate adjacent mines for all tiles
     */
    calculateAdjacentMines() {
        for (let row = 0; row < this.rows; row++) {
            for (let col = 0; col < this.cols; col++) {
                if (!this.board[row][col].isMine) {
                    let count = 0;
                    
                    // Check all 8 surrounding tiles
                    for (let r = -1; r <= 1; r++) {
                        for (let c = -1; c <= 1; c++) {
                            if (r === 0 && c === 0) continue;
                            
                            const newRow = row + r;
                            const newCol = col + c;
                            
                            if (
                                newRow >= 0 && newRow < this.rows &&
                                newCol >= 0 && newCol < this.cols &&
                                this.board[newRow][newCol].isMine
                            ) {
                                count++;
                            }
                        }
                    }
                    
                    this.board[row][col].adjacentMines = count;
                }
            }
        }
    }

    /**
     * Handle left-click (reveal tile)
     * @param {number} row - Row index
     * @param {number} col - Column index
     */
    handleLeftClick(row, col) {
        if (this.gameState === 'idle') {
            this.startGame();
        }
        
        if (this.gameState !== 'playing') return;
        this.revealTile(row, col);
    }

    /**
     * Handle right-click (toggle flag)
     * @param {number} row - Row index
     * @param {number} col - Column index
     */
    handleRightClick(row, col) {
        if (this.gameState !== 'playing') return;
        this.toggleFlag(row, col);
        
        // Update bomb counter in DOM
        if (this.bombCountElement) {
            this.bombCountElement.textContent = this.bombsRemaining;
        }
    }

    /**
     * Reveal a tile with flood fill algorithm
     * @param {number} row - Row index
     * @param {number} col - Column index
     */
    revealTile(row, col) {
        if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
        
        const tile = this.board[row][col];
        if (tile.isRevealed || tile.isFlagged) return;
        
        tile.reveal();
        
        // Update tile in DOM
        const tileElement = document.querySelector(`.tile[data-row="${row}"][data-col="${col}"]`);
        if (tileElement) {
            tileElement.classList.add('revealed');
            if (tile.isMine) {
                tileElement.classList.add('mine');
            } else if (tile.adjacentMines > 0) {
                tileElement.textContent = tile.adjacentMines;
                tileElement.dataset.value = tile.adjacentMines;
            }
        }
        
        // Game over if mine is revealed
        if (tile.isMine) {
            this.endGame(false);
            return;
        }
        
        // Recursive flood fill for empty tiles
        if (tile.adjacentMines === 0) {
            for (let r = -1; r <= 1; r++) {
                for (let c = -1; c <= 1; c++) {
                    if (r === 0 && c === 0) continue;
                    this.revealTile(row + r, col + c);
                }
            }
        }
        
        // Check win condition after reveal
        this.checkWinCondition();
    }

    /**
     * Toggle flag on a tile
     * @param {number} row - Row index
     * @param {number} col - Column index
     */
    toggleFlag(row, col) {
        if (row < 0 || row >= this.rows || col < 0 || col >= this.cols) return;
        
        const tile = this.board[row][col];
        if (tile.isRevealed) return;
        
        tile.toggleFlag();
        this.bombsRemaining += tile.isFlagged ? -1 : 1;
        
        // Update tile in DOM
        const tileElement = document.querySelector(`.tile[data-row="${row}"][data-col="${col}"]`);
        if (tileElement) {
            tileElement.classList.toggle('flagged', tile.isFlagged);
        }
    }

    /**
     * Check win condition
     */
    checkWinCondition() {
        let unrevealedSafeTiles = 0;
        
        for (let row = 0; row < this.rows; row++) {
            for (let col = 0; col < this.cols; col++) {
                const tile = this.board[row][col];
                if (!tile.isMine && !tile.isRevealed) {
                    unrevealedSafeTiles++;
                }
            }
        }
        
        if (unrevealedSafeTiles === 0 && this.bombsRemaining === 0) {
            this.endGame(true);
        }
    }

    /**
     * Start the game
     */
    startGame() {
        this.gameState = 'playing';
        this.placeMines();
        this.calculateAdjacentMines();
        this.startTimer();
        
        // Set DOM references
        this.timerElement = document.getElementById('timer');
        this.bombCountElement = document.getElementById('bomb-count');
        
        if (this.bombCountElement) {
            this.bombCountElement.textContent = this.bombsRemaining;
        }
    }

    /**
     * Start the game timer
     */
    startTimer() {
        this.startTime = Date.now();
        this.timer = setInterval(() => {
            this.elapsedTime = Math.floor((Date.now() - this.startTime) / 1000);
            if (this.timerElement) {
                this.timerElement.textContent = this.elapsedTime;
            }
        }, 1000);
    }

    /**
     * End the game
     * @param {boolean} isWin - Whether the player won
     */
    endGame(isWin) {
        clearInterval(this.timer);
        this.gameState = isWin ? 'won' : 'lost';
        
        // Reveal all mines on loss
        if (!isWin) {
            for (let row = 0; row < this.rows; row++) {
                for (let col = 0; col < this.cols; col++) {
                    if (this.board[row][col].isMine) {
                        this.board[row][col].isRevealed = true;
                        const tileElement = document.querySelector(`.tile[data-row="${row}"][data-col="${col}"]`);
                        if (tileElement) {
                            tileElement.classList.add('mine', 'revealed');
                        }
                    }
                }
            }
        }
        
        // Show game over message
        alert(isWin ? 'You won!' : 'Game over!');
    }

    /**
     * Set game difficulty
     * @param {string} difficulty - Difficulty level (easy/medium/hard)
     */
    setDifficulty(difficulty) {
        const mineCounts = {
            easy: 5,
            medium: 15,
            hard: 25
        };
        
        if (mineCounts[difficulty] !== undefined) {
            this.mineCount = mineCounts[difficulty];
            this.restart();
        }
    }

    /**
     * Restart the game
     */
    restart() {
        clearInterval(this.timer);
        this.gameState = 'idle';
        this.elapsedTime = 0;
        this.initializeBoard();
        
        // Reset DOM elements
        if (this.timerElement) {
            this.timerElement.textContent = '0';
        }
        if (this.bombCountElement) {
            this.bombCountElement.textContent = this.bombsRemaining;
        }
        
        // Reset tiles in DOM
        const tiles = document.querySelectorAll('.tile');
        tiles.forEach(tile => {
            tile.className = 'tile';
            tile.textContent = '';
            delete tile.dataset.value;
        });
    }
}

// Export for browser or Node.js environments
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { Tile, Game };
} else {
    window.Minesweeper = { Tile, Game };
}