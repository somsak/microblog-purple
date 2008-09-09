#
# Microblog protocol plug-in
#

all: build

include ../global.mak

TARGETS = libtwitter$(PLUGIN_SUFFIX)

#PURPLE_CFLAGS = $(shell pkg-config --cflags purple)
#PURPLE_LIBS = $(shell pkg-config --libs purple)

LD = $(CC)
CFLAGS = $(PURPLE_CFLAGS) -g
TWITTER_C_SRC = twitter.c mb_util.c mb_http.c mb_net.c
TWITTER_H_SRC = $(TWITTER_C_SRC:%.c=%.h)
TWITTER_IMG = twitter16.png twitter22.png twitter48.png
TWITTER_OBJ = $(TWITTER_C_SRC:%.c=%.o)
DISTFILES = $(TWITTER_C_SRC) $(TWITTER_H_SRC) $(TWITTER_IMG) Makefile

OBJECTS = $(TWITTER_OBJ)

.PHONY: all clean install 

build: $(TARGETS)

install: $(TARGETS)
	rm -f $(PURPLE_PLUGIN_DIR)/libtwitter$(PLUGIN_SUFFIX)
	install -m 0755 -d $(PURPLE_PLUGIN_DIR)
	cp libtwitter$(PLUGIN_SUFFIX) $(PURPLE_PLUGIN_DIR)/libtwitter$(PLUGIN_SUFFIX)
	for dir in 16 22 48; do \
		(install -m 0755 -d $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir && \
			install -m 0644 twitter$$dir.png $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir/twitter.png ) \
	done

uninstall: 
	rm -f $(PURPLE_PLUGIN_DIR)/libtwitter$(PLUGIN_SUFFIX)
	for dir in 16 22 48; do \
		(rm -f $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir/twitter.png ) \
	done


clean:
	rm -f $(TARGETS) $(OBJECTS)

libtwitter$(PLUGIN_SUFFIX): $(TWITTER_OBJ)
	$(LD) $(LDFLAGS) -shared $(TWITTER_OBJ) $(PURPLE_LIBS) -lefence $(DLL_LD_FLAGS) -o libtwitter$(PLUGIN_SUFFIX)

mb_http.o: mb_http.c mb_http.h
mb_net.o: mb_net.c mb_net.h mb_http.h twitter.h
mb_util.o: mb_util.c
twitter.o: twitter.c mb_net.h mb_http.h twitter.h mb_util.h
