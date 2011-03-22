PREFIX ?= /usr

INCLUDE="-I${PREFIX}/include"
GTK=`pkg-config --cflags --libs gtk+-2.0`
PCRE="-lpcre"
CC?=gcc

all:
		${CC} ${GTK} ${PCRE} ${INCLUDE} -o lyrics.so -shared lyrics.c

clean:
		rm lyrics.so
