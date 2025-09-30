CC ?= gcc
CFLAGS ?= -O2 -std=c11
SRCS = \
    attacks.c \
    bench.c \
    SirioC.c \
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

SirioC-0.1.0: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

bench: SirioC-0.1.0
	@printf "uci\nisready\nucinewgame\nposition startpos\ngo depth 1\nquit\n" | ./SirioC-0.1.0 --uci > .sirio_bench.log
	@cat .sirio_bench.log
	@awk '/^bestmove/ { bm=$$2 } END { if (bm == "" || bm == "0000" || bm == "(none)") { printf("bench: invalid bestmove %s\n", bm); exit 1 } }' .sirio_bench.log
	@rm -f .sirio_bench.log

clean:
	rm -f $(OBJS) SirioC-0.1.0

.PHONY: clean bench

