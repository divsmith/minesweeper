build: 
	CGO_ENABLED=0 go build -o minesweeper

run: build
	./minesweeper

test:
	CGO_ENABLED=0 go test -v

clean:
	rm -f minesweeper

install-deps:
	go mod tidy

.PHONY: build run test clean install-deps