CFLAGS=`pkg-config libusb-1.0 --cflags` -Wall -g
LIBS=`pkg-config libusb-1.0 --libs`
all:
	$(CC) novena-usb-hub.c -o novena-usb-hub $(CFLAGS) $(LIBS)

clean:
	rm -f novena-usb-hub
