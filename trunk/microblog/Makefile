#
# Microblog protocol plug-in
#

include ../global.mak

TARGETS = libtwitter$(PLUGIN_SUFFIX)

#PURPLE_CFLAGS = $(shell pkg-config --cflags purple)
#PURPLE_LIBS = $(shell pkg-config --libs purple)

LD = $(CC)
CFLAGS = -DMBPURPLE_VERSION=\"$(VERSION)\" $(PURPLE_CFLAGS)
TWITTER_C_SRC = twitter.c mb_util.c mb_http.c mb_net.c
TWITTER_H_SRC = $(TWITTER_C_SRC:%.c=%.h)
TWITTER_IMG = twitter16.png twitter22.png twitter48.png
TWITTER_OBJ = $(TWITTER_C_SRC:%.c=%.o)

OBJECTS = $(TWITTER_OBJ)

.PHONY: all clean install

build: $(TARGETS)

install: $(TARGETS)
	rm -f $(PURPLE_PLUGIN_DIR)/libtwitter$(PLUGIN_SUFFIX)
	install -m 0755 -d $(PURPLE_PLUGIN_DIR)
	# for some reason, install -m0644 doesn't works on Windows (Access Denied when loading dll), use cp instead
	cp libtwitter$(PLUGIN_SUFFIX) $(PURPLE_PLUGIN_DIR)/libtwitter$(PLUGIN_SUFFIX)
	for dir in 16 22 48; do \
		(install -m 0755 -d $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir && \
			install -m 0644 twitter$$dir.png $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir/twitter.png ) \
	done

clean:
	rm -f $(TARGETS) $(OBJECTS)

libtwitter$(PLUGIN_SUFFIX): $(TWITTER_OBJ)
	$(LD) $(LDFLAGS) -shared $(TWITTER_OBJ) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o libtwitter$(PLUGIN_SUFFIX)
	
pidgin-microblog.exe: $(TARGET).dll twitter.nsi
	makensis /DPRODUCT_VERSION=$(VERSION) twitter.nsi

dist: $(H_SRC) $(C_SRC) Makefile $(IMG_SRC)
	rm -rf $(SRCNAME)-$(VERSION)
	mkdir -p $(SRCNAME)-$(VERSION)
	cp $^ $(SRCNAME)-$(VERSION)/
	tar -zcvf $(SRCNAME)-$(VERSION).tar.gz $(SRCNAME)-$(VERSION)/
