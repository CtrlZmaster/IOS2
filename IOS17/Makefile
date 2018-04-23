CFLAGS = -std=gnu99 -Wall -Werror -Wextra -pedantic 
EXTRAFLAGS = -pthread -lrt

all: proj2.o proj2 clean

proj2.o: 
	gcc -c proj2.c $(CFLAGS) $(EXTRAFLAGS)

proj2: proj2.o
	gcc proj2.o $(CFLAGS) $(EXTRAFLAGS) -o proj2

clean:
	rm -f *.o proj2.out

pack:
	zip proj2.zip *.c *.h Makefile
