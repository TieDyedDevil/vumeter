
CC ?= gcc
CFLAGS = -Wall -g -std=c11 -U__STRICT_ANSI__

all: vumeter

clean:
	rm -f vumeter

vumeter: vumeter.c Makefile
	$(CC) $(CFLAGS) vumeter.c -o vumeter -lm \
		`sdl2-config --cflags --libs` \
		`pkg-config SDL2_ttf --cflags --libs`

install: vumeter
	install vumeter /usr/local/bin/
	install -d /usr/local/share/doc/vumeter/
	install -m 644 README.md TODO LICENSE /usr/local/share/doc/vumeter/

uninstall:
	rm -rf /usr/local/bin/vumeter /usr/local/share/doc/vumeter/
