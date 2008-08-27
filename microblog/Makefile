
VERSION = 0.2.1
TARGETS = libtwitter.so
PREFIX = /usr

PURPLE_CFLAGS = $(shell pkg-config --cflags purple)
PURPLE_LIBS = $(shell pkg-config --libs purple)

LD = $(CC)
CFLAGS = -DPURPLE_PLUGINS -DENABLE_NLS -Wall -pthread -I. -g -O2 -pipe -fPIC -DPIC -DMBPURPLE_VERSION=\"$(VERSION)\"
CFLAGS += $(PURPLE_CFLAGS)
TWITTER_SRC = twitter.c mb_util.c mb_http.c mb_net.c
TWITTER_IMG = twitter16.png twitter22.png twitter48.png
TWITTER_OBJ = $(TWITTER_SRC:%.c=%.o)

OBJECTS = $(TWITTER_OBJ)

.PHONY: all clean install

all: $(TARGETS)

install: $(TARGETS)
	install -m 0755 -d $(DESTDIR)$(PREFIX)/lib/purple-2
	install -m 0755 -d $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols/16
	install -m 0755 -d $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols/22
	install -m 0755 -d $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols/48

	install -m 0644 libtwitter.so $(DESTDIR)$(PREFIX)/lib/purple-2/libtwitter.so
	install -m 0644 twitter16.png $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols/16/twitter.png
	install -m 0644 twitter22.png $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols/22/twitter.png
	install -m 0644 twitter48.png $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols/48/twitter.png

clean:
	rm -f $(TARGETS) $(OBJECTS)

libtwitter.so: $(TWITTER_OBJ)
	$(LD) $(LDFLAGS) -shared $(TWITTER_OBJ) $(PURPLE_LIBS) -o libtwitter.so
