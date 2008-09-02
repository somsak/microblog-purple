#
# Top-level makefile
#

include version.mak

SUBDIRS = microblog twitgin

.PHONY: all install clean build distdir

default: build

build install clean: 
	for dir in $(SUBDIRS); do \
		make -C "$$dir" $@; \
	done
	
distdir: 
	rm -rf $(PACKAGE)-$(VERSION)
	mkdir $(PACKAGE)-$(VERSION)
	for dir in $(SUBDIRS); do \
		make -C "$$dir" dist; \
	done

dist: distdir
	tar -zcvf $(PACKAGE)-$(VERSION).tar.gz $(PACKAGE)-$(VERSION)
	rm -rf $(PACKAGE)-$(VERSION)