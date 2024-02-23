all: anshell

anshell: anshell.c
	gcc -g -o $@ $<

clean:
	rm -rf anshell *.dSYM
