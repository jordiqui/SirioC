CC ?= gcc
CXX ?= g++
CFLAGS ?= -O2 -std=c11
CXXFLAGS ?= -O2 -std=c++20
CFLAGS += -I../vendor/fathom
CXXFLAGS += -I../vendor/fathom
CFLAGS += -Iengine -Iengine/core -Iengine/eval -Iengine/nn -Iengine/search -Iengine/tb -Iengine/uci -Iengine/util
CXXFLAGS += -Iengine -Iengine/core -Iengine/eval -Iengine/nn -Iengine/search -Iengine/tb -Iengine/uci -Iengine/util
CFLAGS += -pthread
CXXFLAGS += -pthread

SRCS = \
    engine/SirioC.c \
    engine/core/attacks.c \
    engine/search/bench.c \
    engine/core/bits.c \
    engine/core/board.c \
    engine/eval/eval.c \
    engine/core/history.c \
    engine/core/move.c \
    engine/core/movegen.c \
    engine/core/movepick.c \
    engine/search/perft.c \
    engine/core/random.c \
    engine/search/search.c \
    engine/search/see.c \
    engine/tb/tb.c \
    engine/search/thread.c \
    engine/search/transposition.c \
    engine/uci/uci.c \
    engine/util/util.c \
    engine/nn/accumulator.c \
    engine/nn/evaluate.c \
    engine/nn/nnue_paths.c

CPPSRCS = \
    engine/core/zobrist.cpp \
    engine/util/path.cpp

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

