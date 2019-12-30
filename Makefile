
CC ?= gcc
CFLAGS = -Wall -g

all: vumeter

clean:
	rm -f vumeter

vumeter: vumeter.c
	$(CC) $(CFLAGS) vumeter.c -o vumeter -lm `sdl2-config --cflags --libs`

install: vumeter
	install vumeter /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/vumeter
