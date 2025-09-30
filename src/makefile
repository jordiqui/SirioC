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
    zobrist.c \
    nn/accumulator.c \
    nn/evaluate.c

OBJS = $(SRCS:.c=.o)

sirio: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

bench: sirio
	@printf "uci\nisready\nucinewgame\nposition startpos\ngo depth 1\nquit\n" | ./sirio --uci > .sirio_bench.log
	@cat .sirio_bench.log
	@awk '/^bestmove/ { bm=$$2 } END { if (bm == "" || bm == "0000" || bm == "(none)") { printf("bench: invalid bestmove %s\n", bm); exit 1 } }' .sirio_bench.log
	@rm -f .sirio_bench.log

clean:
	rm -f $(OBJS) sirio

.PHONY: clean bench

