# Project: IOS Project 2
# File: Makefile
# Title: The Senate Bus Problem (modified)
# Description: -
# Author: Michal Pospíšil (xpospi95@stud.fit.vutbr.cz)

##### VARIABLES
CFLAGS = -std=gnu99 -Wall -Werror -Wextra -pedantic
EXTRAFLAGS = -pthread -lrt

##### TARGETS - first target is default
all: proj2

proj2: proj2.o
	gcc $^ $(CFLAGS) $(EXTRAFLAGS) -o proj2

clean:
	rm -f *.o proj2.out

pack:
	zip proj2.zip *.c *.h Makefile

##### OBJECT FILES
proj2.o: proj2.c
	gcc -c $^ $(CFLAGS) $(EXTRAFLAGS)
