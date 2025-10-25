# SirioC Makefile

CXX ?= g++
CXXFLAGS ?= -std=c++20 -Wall -Wextra -Wpedantic -O2
CPPFLAGS ?=
LDFLAGS ?=
INCLUDES := -Iinclude

SRCDIR := src
TESTDIR := tests
BUILDDIR := build
OBJDIR := $(BUILDDIR)/obj
BINDIR := $(BUILDDIR)/bin

CORE_SRCS := $(filter-out $(SRCDIR)/main.cpp,$(wildcard $(SRCDIR)/*.cpp))
CORE_OBJS := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/src/%.o,$(CORE_SRCS))
MAIN_OBJ := $(OBJDIR)/src/main.o
TEST_SRCS := $(wildcard $(TESTDIR)/*.cpp)
TEST_OBJS := $(patsubst $(TESTDIR)/%.cpp,$(OBJDIR)/tests/%.o,$(TEST_SRCS))

TARGET := $(BINDIR)/sirio
TEST_TARGET := $(BINDIR)/sirio_tests

.PHONY: all clean test dirs sirio sirio_tests

all: $(TARGET)

sirio: $(TARGET)

sirio_tests: $(TEST_TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

$(TARGET): $(CORE_OBJS) $(MAIN_OBJ) | dirs
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJS) | dirs
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(OBJDIR)/src/%.o: $(SRCDIR)/%.cpp | dirs
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR)/tests/%.o: $(TESTDIR)/%.cpp | dirs
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

dirs:
	@mkdir -p $(OBJDIR)/src $(OBJDIR)/tests $(BINDIR)
