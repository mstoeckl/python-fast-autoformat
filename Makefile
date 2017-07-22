
all: pfa

pfa: pfa.c
	gcc -Wall -ansi -pedantic -std=c99 -fno-omit-frame-pointer -Os pfa.c -o pfa 

clean:
	rm *.o pfa
