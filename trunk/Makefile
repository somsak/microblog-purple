CC = gcc

LIBPURPLE_CFLAGS = -I/usr/include/libpurple -I/usr/local/include/libpurple -DPURPLE_PLUGINS -DENABLE_NLS
GLIB_CFLAGS = -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include -I/usr/local/include/glib-2.0 -I/usr/local/lib/glib-2.0/include -I/usr/local/include

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
	${CC} ${LIBPURPLE_CFLAGS} -Wall -pthread ${GLIB_CFLAGS} -I. -g -O2 -pipe twitter.c -o libtwitter.so -shared -fPIC -DPIC

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
