CC = cc
TARGET = iddb
SOURCE = *.c
DESTDIR = /usr/bin
CFLAGS ?= -Wall -g -lsqlite3 -O2 -pedantic

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) $(DESTDIR)
