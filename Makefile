
CC=gcc
CFLAGS = -o
EXECNAME=lab3a

default: lab3a.c
	$(CC) $(CFLAGS) $(EXECNAME) lab3a.c
	chmod +x ./$(EXECNAME)

clean:
	rm -f $(EXECNAME) *.tar.gz *~