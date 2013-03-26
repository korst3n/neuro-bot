#!/bin/bash

[ "$1" == "rm" ] && rm *.o && exit

LIBS="jansson libcurl glib-2.0 gio-2.0 python-2.7"
CFLAGS="`pkg-config $LIBS --cflags` -I. -g"
LDFLAGS="`pkg-config $LIBS --libs` -ldl -g"

gcc -c *.c $CFLAGS
gcc *.o -o neurobot $LDFLAGS

