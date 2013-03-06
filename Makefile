CFLAGS=`pkg-config libusb-1.0 --cflags` -Wall -g
LIBS=`pkg-config libusb-1.0 --libs`
all:
	$(CC) novena-hub.c -o novena-hub $(CFLAGS) $(LIBS)
