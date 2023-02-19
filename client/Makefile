CC=gcc
CFLAGS=-pthread $(shell sdl2-config --cflags)
LDFLAGS=$(shell sdl2-config --libs) -lSDL2_ttf
EXEC=client

all: $(EXEC)

$(EXEC): client.o
	$(CC) -o $@ $^ $(LDFLAGS)

client.o: client.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -f *.o $(EXEC)