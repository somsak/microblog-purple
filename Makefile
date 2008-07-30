CC = gcc
MICROBLOG_CFLAGS = -DPURPLE_PLUGINS -DENABLE_NLS -Wall -pthread -I. -g -O2 -pipe -fPIC -DPIC
PURPLE_CFLAGS = $(shell pkg-config --cflags purple)

#Standard stuff here
.PHONY: all clean install

all:    libtwitter.so

install:
	mkdir -p $(DESTDIR)/usr/lib/purple-2
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48
	cp libtwitter.so $(DESTDIR)/usr/lib/purple-2/
	cp twitter16.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16/twitter.png
	cp twitter22.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22/twitter.png
	cp twitter48.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48/twitter.png

clean:
	rm -f libtwitter.so

libtwitter.so: twitter.c util.c
	${CC} ${MICROBLOG_CFLAGS} ${PURPLE_CFLAGS} -shared twitter.c util.c -o libtwitter.so
