CFLAGS += `pkg-config libusb-1.0 --cflags --libs` -Wall -g $(shell dpkg-buildflags --get CFLAGS)
LDFLAGS += `pkg-config libusb-1.0 --libs` -g $(shell dpkg-buildflags --get LDFLAGS)

all:
	$(CC) $(CFLAGS) $(LDFLAGS) novena-usb-hub.c -o novena-usb-hub

clean:
	rm -f novena-disable-ssp

install:
	mkdir -p $(DESTDIR)/usr/bin
	mkdir -p $(DESTDIR)/usr/share/man/man1
	cp novena-usb-hub $(DESTDIR)/usr/bin/
	cp novena-usb-hub.1 $(DESTDIR)/usr/share/man/man1
