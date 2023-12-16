CC = gcc
CFLAGS = -I/opt/homebrew/Cellar/sdl2/2.28.5/include/SDL2/
LDFLAGS = -L/opt/homebrew/Cellar/sdl2/2.28.5/lib -lSDL2
TARGET = chip8
SRC = chip8.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)