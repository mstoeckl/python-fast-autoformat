
all: pfa

realtime: realtime.c
	clang-format -i pfa.c && gcc -Wall -ansi -pedantic -std=c89 -fno-omit-frame-pointer -Os pfa.c -o pfa

clean:
	rm *.o pfa
