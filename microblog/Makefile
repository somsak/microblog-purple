#
# Microblog protocol plug-in
#

all: build

include ../global.mak

TARGETS = liboldtwitter$(PLUGIN_SUFFIX) libtwitter$(PLUGIN_SUFFIX) libidentica$(PLUGIN_SUFFIX) liblaconica$(PLUGIN_SUFFIX)
PRPLS = twitter identica laconica

#PURPLE_CFLAGS = $(shell pkg-config --cflags purple)
#PURPLE_LIBS = $(shell pkg-config --libs purple)

CFLAGS := $(PURPLE_CFLAGS)
LD = $(CC)

OLDTWITTER_C_SRC = dummy_twitterim.c
OLDTWITTER_OBJ = $(OLDTWITTER_C_SRC:%.c=%.o)


TWITTER_C_SRC = twitter.c mb_util.c mb_http.c mb_net.c mb_cache.c twitterim.c tw_util.c tw_cmd.c
TWITTER_H_SRC = twitter.h mb_util.h mb_http.h mb_net.h tw_cmd.h
TWITTER_IMG = twitter16.png twitter22.png twitter48.png
TWITTER_OBJ = $(TWITTER_C_SRC:%.c=%.o)

IDENTICA_C_SRC = identica.c mb_util.c mb_http.c mb_net.c mb_cache.c twitter.c tw_util.c
IDENTICA_H_SRC = $(TWITTER_H_SRC) 
IDENTICA_IMG = identica16.png identica22.png identica48.png
IDENTICA_OBJ = $(IDENTICA_C_SRC:%.c=%.o)

laconica.o: identica.c
	$(COMPILE.c) $(OUTPUT_OPTION) -DLACONICA $<

LACONICA_C_SRC = mb_util.c mb_http.c mb_net.c mb_cache.c twitter.c tw_util.c
LACONICA_H_SRC = $(TWITTER_H_SRC)
LACONICA_IMG = laconica16.png laconica22.png laconica48.png
LACONICA_OBJ = $(LACONICA_C_SRC:%.c=%.o) laconica.o

DISTFILES = $(OLDTWITTER_C_SRC) \
$(TWITTER_C_SRC) $(TWITTER_H_SRC) $(TWITTER_IMG) \
$(IDENTICA_H_SRC) $(IDENTICA_C_SRC) $(IDENTICA_IMG) \
$(LACONICA_H_SRC) $(LACONICA_C_SRC) $(LACONICA_IMG) \
Makefile

OBJECTS = $(OLDTWITTER_OBJ) $(TWITTER_OBJ) $(IDENTICA_OBJ) $(LACONICA_OBJ)

.PHONY: all clean install uninstall

build: $(TARGETS)

install: $(TARGETS)
	for prpl in $(PRPLS); do \
		rm -f $(PURPLE_PLUGIN_DIR)/lib$$prpl$(PLUGIN_SUFFIX); \
	done
	install -m 0755 -d $(PURPLE_PLUGIN_DIR)
	cp liboldtwitter$(PLUGIN_SUFFIX) $(PURPLE_PLUGIN_DIR)/liboldtwitter$(PLUGIN_SUFFIX)
	for prpl in $(PRPLS); do \
		( cp lib$$prpl$(PLUGIN_SUFFIX) $(PURPLE_PLUGIN_DIR)/lib$$prpl$(PLUGIN_SUFFIX) && \
		for dir in 16 22 48; do \
			(install -m 0755 -d $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir && \
				install -m 0644 $$prpl$$dir.png $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir/$$prpl.png ) \
		done ) \
	done

uninstall: 
	for prpl in $(PRPLS); do \
		rm -f $(PURPLE_PLUGIN_DIR)/lib$$prpl$(PLUGIN_SUFFIX); \
	done
	rm -f $(PURPLE_PLUGIN_DIR)/liboldtwitter$(PLUGIN_SUFFIX)
	for dir in 16 22 48; do \
		rm -f $(PURPLE_PROTOCOL_PIXMAP_DIR)/$$dir/{twitter,identica,laconica}.png ; \
	done


clean:
	rm -f $(TARGETS) $(OBJECTS)

liboldtwitter$(PLUGIN_SUFFIX): $(OLDTWITTER_OBJ)
	$(LD) $(LDFLAGS) -shared $(OLDTWITTER_OBJ) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o $@

libtwitter$(PLUGIN_SUFFIX): $(TWITTER_OBJ)
	$(LD) $(LDFLAGS) -shared $(TWITTER_OBJ) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o $@

libidentica$(PLUGIN_SUFFIX): $(IDENTICA_OBJ)
	$(LD) $(LDFLAGS) -shared $(IDENTICA_OBJ) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o $@

liblaconica$(PLUGIN_SUFFIX): $(LACONICA_OBJ)
	$(LD) $(LDFLAGS) -shared $(LACONICA_OBJ) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o $@

mbchprpl$(EXE_SUFFIX): mbchprpl.o
	$(CC) $< $(LIB_PATHS) $(LIBS) $(LDFLAGS) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o $@ 

xml_tester$(EXE_SUFFIX): xml_tester.c
	$(CC) $< $(LIB_PATHS) $(LIBS) $(LDFLAGS) $(PURPLE_LIBS) $(DLL_LD_FLAGS) -o $@ 

mb_http.o: mb_http.c mb_http.h
mb_net.o: mb_net.c mb_net.h mb_http.h
mb_util.o: mb_util.c
twitter.o: twitter.c mb_net.h mb_http.h twitter.h mb_util.h
twitterim.o: twitter.o
identica.o: twitter.o
