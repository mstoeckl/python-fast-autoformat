
all: pfa

realtime: realtime.c
	gcc -Wall -ansi -pedantic -std=c89 -fno-omit-frame-pointer -fsanitize=address -Os pfa.c -o pfa 

clean:
	rm *.o pfa
