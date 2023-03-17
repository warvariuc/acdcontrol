RELEASE_FILES=acdcontrol.cpp acdcontrol.init acdcontrol.sysconfig COPYING COPYRIGHT Makefile VERSION
VERSION=$(shell cat VERSION)
VERNAME=acdcontrol-$(VERSION)
DIRNAME=/tmp/$(VERNAME)

acdcontrol: acdcontrol.cpp

release:
	mkdir -p $(DIRNAME)
	rm -rf $(DIRNAME)/*
	cp $(RELEASE_FILES) $(DIRNAME)
	tar cvfz $(VERNAME).tar.gz -C /tmp $(VERNAME) 

upload:
	curl -T $(VERNAME).tar.gz ftp://anonymous@upload.sourceforge.net/incoming/

install:
	install -m 0644 -o root -g root 69-apple-cinema.rules /etc/udev/rules.d
	install -m 0755 -o root -g root acdcontrol /usr/local/bin
