CFLAGS  = -Wall -Werror -pedantic -fsanitize=address,undefined
FFLAGS	= -Wall -Werror -pedantic
SRC     = test.c hashdb.c
CC      = gcc

all: $(SRC)
	$(CC) $(CFLAGS) $^

fast:
	$(CC) $(CFLAGS) $(SRC)
