# Default timeout value (in seconds)
TIMEOUT ?= 30

# Build the project in debug mode
build:
	cargo build
	cp target/debug/minesweeper .

# Build the project in release mode
release:
	cargo build --release
	cp target/release/minesweeper .

# Build and run in easy mode
run-easy:
	timeout $(TIMEOUT)s cargo run -- -e

# Build and run in normal mode
run-normal:
	timeout $(TIMEOUT)s cargo run -- -n

# Build and run in hard mode
run-hard:
	timeout $(TIMEOUT)s cargo run -- -h

# Build and run to view scores
run-scores:
	cargo run -- -s

# Clean build artifacts
clean:
	cargo clean
	rm -f minesweeper

.PHONY: build release run-easy run-normal run-hard run-scores clean