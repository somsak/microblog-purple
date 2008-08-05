twitgindir = $(libdir)/pidgin

twitgin_la_LDFLAGS = -module -avoid-version

if PLUGINS

twitginpixdir = $(datadir)/pixmaps/pidgin/buttons

twitgin_LTLIBRARIES = twitgin.la

twitgin_la_SOURCES = \
	twitgin.c

endif

twitgin_la_LIBADD = $(GTK_LIBS) $(SQLITE3_LIBS)

AM_CPPFLAGS = \
	-DDATADIR=\"$(datadir)\" \
	-I$(top_srcdir)/libpurple \
	-I$(top_builddir)/libpurple \
	-I$(top_srcdir)/pidgin \
	$(DEBUG_CFLAGS) \
	$(GTK_CFLAGS) \
	$(SQLITE3_CFLAGS)

EXTRA_DIST = Makefile.mingw
