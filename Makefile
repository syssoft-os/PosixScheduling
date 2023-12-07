CC=gcc
CFLAGS=-g -I.
DEPS = 
OBJ = lets_schedule.o 
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

lets_schedule: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) -lm

default: lets_schedule

