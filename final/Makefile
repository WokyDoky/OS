CC = gcc
CFLAGS = -g -O0 -Wall $(shell pkg-config fuse --cflags)
LIBS = $(shell pkg-config fuse --libs)
TARGET = utepfs
SOURCES = utepfs.c implementation.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)