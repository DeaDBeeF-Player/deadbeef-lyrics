PREFIX ?= /usr

INCLUDE="-I${PREFIX}/include"
GTK=`pkg-config --cflags --libs gtk+-2.0`
CC?=gcc
CFLAGS=-Wall -g -fPIC -D_GNU_SOURCE

all:
		${CC} ${CFLAGS} ${GTK} ${INCLUDE} -o lyrics.so -shared lyrics.c

clean:
		rm lyrics.so
