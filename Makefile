PREFIX?=/usr

OUT=ddb_lyrics.so
INCLUDE="-I${PREFIX}/include"
GTK_INCLUDE?=`pkg-config --cflags gtk+-2.0`
GTK_LIBS?=`pkg-config --libs gtk+-2.0`
CC?=gcc
CFLAGS+=-Wall -fPIC -D_GNU_SOURCE ${INCLUDE} ${GTK_INCLUDE}
LDFLAGS+=-shared ${GTK_LIBS}

SOURCES=lyrics.c

OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(OUT)

$(OUT): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -c -o $@

clean:
	rm $(OBJECTS) $(OUT)
