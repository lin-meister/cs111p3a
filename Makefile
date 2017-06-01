
CC=gcc
CFLAGS=-lm -o
EXECNAME=lab3a

default: lab3a.c
	$(CC) lab3a.c $(CFLAGS) $(EXECNAME)
	chmod +x ./$(EXECNAME)

dist:
	tar -cvzf lab3a-104132751.tar.gz lab3a.c README Makefile ext2_fs.h

clean:
	rm -f $(EXECNAME) *.tar.gz *~
