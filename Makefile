CC = cc
TARGET = iddb
SOURCE = iddb.c
DESTDIR = /usr/bin
CFLAGS ?= -Wall -g -lsqlite3

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) $(DESTDIR)
