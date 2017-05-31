
CC=gcc
CFLAGS=-lm -o
EXECNAME=lab3a

default: lab3a.c
	$(CC) lab3a.c $(CFLAGS) $(EXECNAME)
	chmod +x ./$(EXECNAME)

clean:
	rm -f $(EXECNAME) *.tar.gz *~
