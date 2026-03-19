CC     = gcc
CFLAGS = -std=c11 -Wall -O2 -Isrc
LDFLAGS = -lws2_32
TARGET  = can_monitor.exe

SRCS = src/main.c src/connection.c src/gvret_parser.c src/frame_store.c src/display.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
