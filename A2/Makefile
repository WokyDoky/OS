
CC = gcc

# Define compiler flags:
# -Wall: Enable all warnings about constructions that some users consider
#        questionable, and that are easy to avoid.
# -Wextra: Enable some extra warning flags that are not enabled by -Wall.
# -g: Add debugging information to the executable.
CFLAGS = -Wall -Wextra -g

SOURCES = \
    send_udp.c \
    receive_udp.c \
    reply_udp.c \
    send_receive_udp.c \
    tunneludp_over_tcp_client.c \
    tunneludp_over_tcp_server.c \
    udp_fec.c

# contains all *possible* executables.
EXECUTABLES = $(patsubst %.c,%,$(SOURCES))
EXISTING_SOURCES = $(wildcard $(SOURCES))
TARGETS_TO_BUILD = $(patsubst %.c,%,$(EXISTING_SOURCES))

all: $(TARGETS_TO_BUILD)
%: %.c
	@echo "Compiling $< -> $@"
	$(CC) $(CFLAGS) -o $@ $<

clean:
	@echo "Cleaning up generated executables..."
	# The '-f' flag prevents 'rm' from complaining if a file doesn't exist.
	rm -f $(EXECUTABLES)

.PHONY: all clean
