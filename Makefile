
all: pfa/pfai pfa/pfa

pfa/pfa: pfa/pfa.c
	gcc -Wall -fno-omit-frame-pointer -Os pfa/pfa.c -o pfa/pfa


pfa/pfai: pfa/pfa
	cp pfa/pfa pfa/pfai

clean:
	rm -f pfa/pfai pfa/pfa
