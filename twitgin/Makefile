#
# Microblog protocol plug-in
#

include ../global.mak

TARGETS = twitgin$(PLUGIN_SUFFIX)

INCLUDE_PATHS +=	-I. \
			-I$(GTK_TOP)/include \
			-I$(GTK_TOP)/include/glib-2.0 \
			-I$(GTK_TOP)/lib/glib-2.0/include \
			-I$(PURPLE_TOP) \
			-I$(PURPLE_TOP)/win32 \
			-I$(PIDGIN_TREE_TOP)

TWITGIN_INC_PATHS += -I$(GTK_TOP)/include/gtk-2.0 \
			-I$(GTK_TOP)/include/pango-1.0 \
			-I$(GTK_TOP)/include/atk-1.0 \
			-I$(GTK_TOP)/include/cairo \
			-I$(GTK_TOP)/lib/gtk-2.0/include \
			-I$(PIDGIN_TOP) \
			-I$(PIDGIN_TOP)/win32 
			
LIB_PATHS += -L$(GTK_TOP)/lib \
			-L$(PURPLE_TOP) \
			-L$(PIDGIN_TOP)
			
LIBS =	-lgtk-win32-2.0 \
			-lglib-2.0 \
			-lgdk-win32-2.0 \
			-lgobject-2.0 \
			-lintl \
			-lpurple \
			-lpidgin

LD = $(CC)
CFLAGS = -DMBPURPLE_VERSION=\"$(VERSION)\" $(PURPLE_CFLAGS) $(TWITGIN_INC_PATHS)
TWITGIN_C_SRC = twitgin.c
TWITGIN_H_SRC = $(TWITGIN_C_SRC:%.c=%.h)
TWITGIN_OBJ = $(TWITGIN_C_SRC:%.c=%.o)

OBJECTS = $(TWITGIN_OBJ)

.PHONY: clean install build

build: $(TARGETS)

install: $(TARGETS)
	rm -f $(PURPLE_PLUGIN_DIR)/twitgin$(PLUGIN_SUFFIX)
	install -m 0755 -d $(PURPLE_PLUGIN_DIR)
	# for some reason, install -m0644 doesn't works on Windows (Access Denied when loading dll), use cp instead
	cp twitgin$(PLUGIN_SUFFIX) $(PURPLE_PLUGIN_DIR)/twitgin$(PLUGIN_SUFFIX)

clean:
	rm -f $(TARGETS) $(OBJECTS)

twitgin$(PLUGIN_SUFFIX): $(TWITGIN_OBJ)
	$(LD) $(LDFLAGS) -shared $(TWITGIN_OBJ) $(LIB_PATHS) $(LIBS) $(DLL_LD_FLAGS) -o twitgin$(PLUGIN_SUFFIX)

dist: $(H_SRC) $(C_SRC) Makefile $(IMG_SRC)
	rm -rf $(SRCNAME)-$(VERSION)
	mkdir -p $(SRCNAME)-$(VERSION)
	cp $^ $(SRCNAME)-$(VERSION)/
	tar -zcvf $(SRCNAME)-$(VERSION).tar.gz $(SRCNAME)-$(VERSION)/
