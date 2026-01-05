# Target file to be compiled by default
MAIN = PoolGame

# CC will be compiler to use
CC = gcc

# CFLAGS are the option passed to the compiler
CFLAGS = -Wall -lpthread -lrt -lm

# OBJS are the object files to be linked
OBJ1 = ptask
OBJS = $(MAIN).o $(OBJ1).o

# Dependencies
$(MAIN): $(MAIN).o ptask.o
		$(CC) -o $(MAIN) $(MAIN).o ptask.o `allegro-config --libs` $(CFLAGS)

$(MAIN).o: $(MAIN).c
		$(CC) -c $(MAIN).c

ptask.o: ptask.c
		$(CC) -c ptask.c
