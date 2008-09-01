#
# Top-level makefile
#

#include global.mak

SUBDIRS = microblog twitgin

.PHONY: all install clean build

default: build

build install clean: 
	for dir in $(SUBDIRS); do \
		make -C "$$dir"; \
	done