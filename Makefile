PREFIX ?= /usr

OUT=ddb_lyrics.so
INCLUDE="-I${PREFIX}/include"
GTK?=`pkg-config --cflags --libs gtk+-2.0`
CC?=gcc
CFLAGS+=-Wall -fPIC -D_GNU_SOURCE ${GTK}
LDFLAGS+=-shared

SOURCES=lyrics.c

OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(OUT)

$(OUT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm $(OBJECTS) $(OUT)
