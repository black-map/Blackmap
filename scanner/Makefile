# Blackmap Scanner - Simple Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -D_DEFAULT_SOURCE

TARGET = blackmap

all:
	$(CC) $(CFLAGS) -o $(TARGET) \
		network/checksum.c \
		network/raw_socket.c \
		network/packet_builder.c \
		scanners/scanners.c \
		detection/service_detection.c \
		cli/argument_parser.c \
		cli/main.c \
		-lpthread

clean:
	rm -f $(TARGET)

.PHONY: all clean
