CFLAGS  += `pkg-config libusb-1.0 --cflags --libs` -Wall -g $(shell dpkg-buildflags --get CFLAGS)
LDFLAGS += `pkg-config libusb-1.0 --libs` -g $(shell dpkg-buildflags --get LDFLAGS)

DESTDIR ?=
PREFIX  ?= /usr/lib

all:
	$(CC) $(CFLAGS) $(LDFLAGS) novena-usb-hub.c -o novena-usb-hub

clean:
	rm -f novena-usb-hub

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	cp novena-usb-hub $(DESTDIR)$(PREFIX)/bin/
	cp novena-usb-hub.1 $(DESTDIR)$(PREFIX)/share/man/man1
