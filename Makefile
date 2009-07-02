#
# Top-level makefile
#

include version.mak

SUBDIRS = microblog twitgin certs
DISTFILES = COPYING global.mak Makefile mbpurple.nsi README.txt version.mak

.PHONY: all install clean build distdir

default: build

build install uninstall clean : 
	for dir in $(SUBDIRS); do \
		make -C "$$dir" $@ || exit 1; \
	done

distdir: 
	rm -rf $(PACKAGE)-$(VERSION)
	mkdir $(PACKAGE)-$(VERSION)
	for dir in $(SUBDIRS); do \
		make -C "$$dir" dist; \
	done
	cp -f $(DISTFILES) $(PACKAGE)-$(VERSION)/

dist: distdir
	tar -zcvf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	rm -rf $(PACKAGE)-$(VERSION)

windist: pidgin-microblog-$(VERSION).exe

pidgin-microblog.exe: build mbpurple.nsi
	makensis /DPRODUCT_VERSION=$(VERSION) mbpurple.nsi

pidgin-microblog-$(VERSION).exe: pidgin-microblog.exe
	mv -f $< $@
	
zipdist: pidgin-microblog-$(VERSION).zip

pidgin-microblog.zip: build
	rm -rf $(PACKAGE)-$(VERSION)
	mkdir $(PACKAGE)-$(VERSION)
	PURPLE_PLUGIN_DIR=$(PWD)/$(PACKAGE)-$(VERSION)/plugins && \
		PURPLE_INSTALL_DIR=$(PWD)/$(PACKAGE)-$(VERSION) && \
		mkdir -p $$PURPLE_PLUGIN_DIR && \
		for dir in $(SUBDIRS); do \
			make -C "$$dir" install PURPLE_INSTALL_DIR=$$PURPLE_INSTALL_DIR PURPLE_PLUGIN_DIR=$$PURPLE_PLUGIN_DIR; \
		done
	zip -r $@ $(PACKAGE)-$(VERSION)
	rm -rf $(PACKAGE)-$(VERSION)

pidgin-microblog-$(VERSION).zip: pidgin-microblog.zip
	mv -f $< $@

