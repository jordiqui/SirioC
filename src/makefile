CC ?= gcc
CFLAGS ?= -O2 -std=c11
SRCS = \
    attacks.c \
    bench.c \
    berserk.c \
    bits.c \
    board.c \
    eval.c \
    history.c \
    move.c \
    movegen.c \
    movepick.c \
    perft.c \
    random.c \
    search.c \
    see.c \
    tb.c \
    thread.c \
    transposition.c \
    uci.c \
    util.c \
    zobrist.c

OBJS = $(SRCS:.c=.o)

sirio: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -f $(OBJS) sirio

.PHONY: clean

