CC ?= gcc
CXX ?= g++
CFLAGS ?= -O2 -std=c11
CXXFLAGS ?= -O2 -std=c++20
CFLAGS += -I../vendor/fathom
CXXFLAGS += -I../vendor/fathom
CFLAGS += -pthread
CXXFLAGS += -pthread

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
    nn/accumulator.c \
    nn/evaluate.c

CPPSRCS = \
    zobrist.cpp

VENDOR_SRCS = \
    ../vendor/fathom/tbprobe.c

OBJS = $(SRCS:.c=.o) $(CPPSRCS:.cpp=.o) $(VENDOR_SRCS:../vendor/fathom/%.c=%.fathom.o)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.fathom.o: ../vendor/fathom/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

SirioC-0.1.0: $(OBJS)
	$(CXX) $(CFLAGS) $(CXXFLAGS) -o $@ $(OBJS)

bench: SirioC-0.1.0
	@printf "uci\nisready\nucinewgame\nposition startpos\ngo depth 1\nquit\n" | ./SirioC-0.1.0 --uci > .sirio_bench.log
	@cat .sirio_bench.log
	@awk '/^bestmove/ { bm=$$2 } END { if (bm == "" || bm == "0000" || bm == "(none)") { printf("bench: invalid bestmove %s\n", bm); exit 1 } }' .sirio_bench.log
	@rm -f .sirio_bench.log

clean:
	rm -f $(OBJS) SirioC-0.1.0

.PHONY: clean bench

