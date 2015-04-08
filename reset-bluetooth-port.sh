#!/bin/sh
if [ "$1" = "post" ]
then
	/usr/bin/novena-usb-hub -d u3
	/usr/bin/novena-usb-hub -e u3
fi
