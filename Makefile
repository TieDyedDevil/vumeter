
CC = gcc
CFLAGS = -Wall -g -std=c11 -U__STRICT_ANSI__

all: vumeter

clean:
	rm -f vumeter

vumeter: vumeter.c
	$(CC) $(CFLAGS) vumeter.c -o vumeter -lm `sdl2-config --cflags --libs` `pkg-config SDL2_ttf --cflags --libs`

install: vumeter
	install vumeter /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/vumeter
