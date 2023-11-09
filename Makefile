CFLAGS  = -Wall -Werror -pedantic -fsanitize=address,undefined
SRC     = test.c hashdb.c
CC      = gcc

all: $(SRC)
	$(CC) $(CFLAGS) $^
