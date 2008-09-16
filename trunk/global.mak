#
# Global header for all makefile
#

-include ../version.mak

# PIDGIN_TREE_TOP is only meaningful on Windows, point it to top directory of Pidgin. IT MUST BE A RELATIVE PATH
# It MUST realitve to each subdirectory (microblog, twitgin)
PIDGIN_TREE_TOP := ../../pidgin-2.5.0

# For Linux
DESTDIR := 
PREFIX := /usr

# Is this WIN32?
IS_WIN32 = $(shell (uname -a | grep -q -i cygwin) && echo 1 || echo 0)

# for override those attributes
-include ../local.mak

ifeq ($(strip $(IS_WIN32)), 1)
# WIN32
# Use makefile and headers supplied by Pidgin 

#include global makefile for WIN32 build
include $(PIDGIN_TREE_TOP)/libpurple/win32/global.mak

##
## INCLUDE PATHS
##
INCLUDE_PATHS +=	-I. \
			-I$(GTK_TOP)/include \
			-I$(GTK_TOP)/include/glib-2.0 \
			-I$(GTK_TOP)/lib/glib-2.0/include \
			-I$(PURPLE_TOP) \
			-I$(PURPLE_TOP)/win32 \
			-I$(PIDGIN_TREE_TOP)


LIBS += -lglib-2.0 \
			-lintl \
			-lws2_32 \
			-lpurple
			
PURPLE_LIBS = -L$(GTK_TOP)/lib -L$(PURPLE_TOP) $(LIBS)
PURPLE_CFLAGS = -DPURPLE_PLUGINS -DENABLE_NLS -Wall -DMBPURPLE_VERSION=\"$(VERSION)\" $(INCLUDE_PATHS)

PURPLE_PROTOCOL_PIXMAP_DIR = $(PURPLE_INSTALL_DIR)/pixmaps/pidgin/protocols
PURPLE_PLUGIN_DIR = $(PURPLE_INSTALL_PLUGINS_DIR)
PLUGIN_SUFFIX := .dll

#include $(PIDGIN_COMMON_RULES)

else
# LINUX and others, use pkg-config
PURPLE_LIBS = $(shell pkg-config --libs purple)
PURPLE_CFLAGS = $(CFLAGS) -DPURPLE_PLUGINS -DENABLE_NLS -DMBPURPLE_VERSION=\"$(VERSION)\"
PURPLE_CFLAGS += $(shell pkg-config --cflags purple)
PURPLE_CFLAGS += -Wall -pthread -I. -g -O2 -pipe -fPIC -DPIC 
PLUGIN_SUFFIX := .so

PURPLE_PROTOCOL_PIXMAP_DIR := $(DESTDIR)$(PREFIX)/share/pixmaps/pidgin/protocols
PURPLE_PLUGIN_DIR := $(DESTDIR)$(LIBDIR)/purple-2

PIDGIN_LIBS = $(shell pkg-config --libs pidgin)
PIDGIN_CFLAGS = $(CFLAGS) -DPIDGIN_PLUGINS -DENABLE_NLS -DMBPURPLE_VERSION=\"$(VERSION)\"
PIDGIN_CFLAGS += $(shell pkg-config --cflags pidgin)
PIDGIN_CFLAGS += -Wall -pthread -I. -g -O2 -pipe -fPIC -DPIC 

LDFLAGS := $(shell (echo $(PIDGIN_CFLAGS) $(PURPLE_CFLAGS)| tr ' ' '\n' | awk '!a[$$0]++' | tr '\n' ' '))
endif

dist: $(DISTFILES)
	mkdir ../$(PACKAGE)-$(VERSION)/`basename $$PWD`
	cp -f $(DISTFILES) ../$(PACKAGE)-$(VERSION)/`basename $$PWD`/
