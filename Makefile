# SirioC Makefile

CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?=
LDFLAGS ?=

ifeq ($(OS),Windows_NT)
LDFLAGS += -static -static-libstdc++ -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
endif

CXX_BASENAME := $(notdir $(CXX))
ifneq (,$(filter %g++ %clang++,$(CXX_BASENAME)))
LDFLAGS += -latomic
endif
INCLUDES := -Iinclude -Ithird_party/fathom

SRCDIR := src
TESTDIR := tests
BENCHDIR := bench
BUILDDIR := build
OBJDIR := $(BUILDDIR)/obj
BINDIR := $(BUILDDIR)/bin

NNUE_SRCS := $(wildcard $(SRCDIR)/nnue/*.cpp)
ENGINE_SRCS := $(wildcard $(SRCDIR)/engine/*.cpp)
CORE_CPP_SRCS := $(filter-out $(SRCDIR)/main.cpp,$(wildcard $(SRCDIR)/*.cpp)) $(NNUE_SRCS) $(ENGINE_SRCS)
THIRDPARTY_SRCS := $(filter-out third_party/fathom/tbcore.c,$(wildcard third_party/fathom/*.c))
CORE_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/src/%.o,$(CORE_CPP_SRCS))
THIRDPARTY_OBJS := $(patsubst third_party/%.c,$(OBJDIR)/third_party/%.o,$(THIRDPARTY_SRCS))
MAIN_OBJ := $(OBJDIR)/src/main.o
TEST_SRCS := $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJS := $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/tests/%.o,$(TEST_SRCS))
BENCH_SRCS := $(wildcard $(BENCHDIR)/*.cpp)
BENCH_OBJS := $(patsubst $(BENCHDIR)/%.cpp,$(OBJDIR)/bench/%.o,$(BENCH_SRCS))

TARGET := $(BINDIR)/sirio
TEST_TARGET := $(BINDIR)/sirio_tests
BENCH_TARGET := $(BINDIR)/sirio_bench

.PHONY: all clean test dirs sirio sirio_tests sirio_bench bench

all: $(TARGET)

sirio: $(TARGET)

sirio_tests: $(TEST_TARGET)

sirio_bench: $(BENCH_TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

bench: $(BENCH_TARGET)
	$(BENCH_TARGET)

$(TARGET): $(CORE_OBJS) $(THIRDPARTY_OBJS) $(MAIN_OBJ) | dirs
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(TEST_TARGET): $(CORE_OBJS) $(THIRDPARTY_OBJS) $(TEST_OBJS) | dirs
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(BENCH_TARGET): $(CORE_OBJS) $(THIRDPARTY_OBJS) $(BENCH_OBJS) | dirs
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(OBJDIR)/src/%.o: $(SRCDIR)/%.cpp | dirs
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/tests/%.o: $(TESTDIR)/%.cpp | dirs
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/bench/%.o: $(BENCHDIR)/%.cpp | dirs
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/third_party/%.o: third_party/%.c | dirs
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

dirs:
	@mkdir -p $(OBJDIR)/src $(OBJDIR)/src/nnue $(OBJDIR)/src/engine $(OBJDIR)/tests $(OBJDIR)/bench $(OBJDIR)/third_party/fathom $(BINDIR)
