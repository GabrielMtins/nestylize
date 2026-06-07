CC:=cc
CC_FLAGS:=-std='c99' -Wall -Wextra -pedantic -O2
INCLUDE_FLAGS:=-I./include -I./external/stb
LIBRARY_FLAGS:=-lm

PROGRAM:=nestylize

all: src/main.c include/palette.h 
	$(CC) $(INCLUDE_FLAGS) $(CC_FLAGS) src/*.c $(LIBRARY_FLAGS) -o $(PROGRAM)

clean:
	rm -f $(PROGRAM)
