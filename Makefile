
all: pfa

pfa: pfa.c
	gcc -Wall -fno-omit-frame-pointer -Os pfa.c -o pfa 

clean:
	rm -f pfa
