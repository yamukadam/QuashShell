#Makefile
CC = gcc
CFLAGS = -Wall -Wextra -g
SRC = src/exec.c src/main.c src/jobs.c
OBJ = $(SRC:.c=.o)
TARGET = quash

all: $(TARGET)
	@echo "making"

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

src/%.o: src/ %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

test:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)


