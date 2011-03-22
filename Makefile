PREFIX ?= /usr

INCLUDE="-I${PREFIX}/include"
GTK=`pkg-config --cflags --libs gtk+-2.0`
PCRE="-lpcre"
CURL?="-lcurl"
CC?=gcc

all:
		${CC} ${GTK} ${PCRE} ${CURL} ${INCLUDE} -o lyrics.so -shared lyrics.c

clean:
		rm lyrics.so
