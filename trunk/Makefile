CC = gcc
MICROBLOG_CFLAGS = -DPURPLE_PLUGINS -DENABLE_NLS -Wall -pthread -I. -g -O2 -pipe -fPIC -DPIC
PURPLE_CFLAGS = $(shell pkg-config --cflags purple)

DEB_PACKAGE_DIR = ./debdir

#Standard stuff here
.PHONY: all clean install sourcepackage

all:    libtwitter.so

install:
	cp libtwitter.so /usr/lib/purple-2/
	cp twitter16.png /usr/share/pixmaps/pidgin/protocols/16/twitter.png
	cp twitter22.png /usr/share/pixmaps/pidgin/protocols/22/twitter.png
	cp twitter48.png /usr/share/pixmaps/pidgin/protocols/48/twitter.png

installers:     pidgin-microblog.deb microblog-purple.tar.bz2

clean:
	rm -f libtwitter.so pidgin-twitterchat.deb pidgin-twitterchat.tar.bz2 pidgin-twitterchat-source.tar.bz2

libtwitter.so: twitter.c
	${CC} ${MICROBLOG_CFLAGS} ${PURPLE_CFLAGS} -shared twitter.c -o libtwitter.so

pidgin-microblog.deb:        libtwitter.so
	echo "Dont forget to update version number"
	mkdir -p ${DEB_PACKAGE_DIR}
	cp libtwitter.so ${DEB_PACKAGE_DIR}/usr/lib/purple-2/
	cp twitter16.png ${DEB_PACKAGE_DIR}/usr/share/pixmaps/pidgin/protocols/16/twitter.png
	cp twitter22.png ${DEB_PACKAGE_DIR}/usr/share/pixmaps/pidgin/protocols/22/twitter.png
	cp twitter48.png ${DEB_PACKAGE_DIR}/usr/share/pixmaps/pidgin/protocols/48/twitter.png
	dpkg-deb --build ${DEB_PACKAGE_DIR} $@ > /dev/null

pidgin-microblog.tar.bz2:    pidgin-microblog.deb
	tar --bzip2 --directory ${DEB_PACKAGE_DIR} -cf $@ usr/

sourcepackage:  twitter.c Makefile twitter16.png twitter22.png twitter48.png COPYING twitter.nsi
	tar --bzip2 -cf microblog-purple.tar.bz2 $^
