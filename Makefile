CC = cc
TARGET = iddb
SOURCE = *.c
DESTDIR = /usr/bin
CFLAGS ?= -Wall -pedantic -g -lsqlite3 -O2

all:
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install:
	cp $(TARGET) $(DESTDIR)
