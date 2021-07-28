

arch = $(shell uname -m)
all:fincores

fincores:
	gcc -g fincores.c -o fincores -I/usr/include -I/usr/include/glib-2.0/ -I/usr/include/glib-2.0/gio/ -I/usr/include/glib-2.0/glib/ -I/usr/include/glib-2.0/gobject  -I/usr/lib/$(arch)-linux-gnu/glib-2.0/include/ -lglib-2.0
