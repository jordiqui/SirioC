ifeq ($(OS),Windows_NT)
        EXE := SirioC.exe
else
        EXE := SirioC
endif

DEFAULT_NET = net89perm.bin

ifndef EVALFILE
	EVALFILE = $(DEFAULT_NET)
	DOWNLOAD_NET = true
endif

M64     = -m64 -mpopcnt
MSSE2   = $(M64) -msse -msse2
MSSSE3  = $(MSSE2) -mssse3
MAVX2   = $(MSSSE3) -msse4.1 -mbmi -mfma -mavx2
MAVX512 = $(MAVX2) -mavx512f -mavx512bw

FILES = $(wildcard src/*.cpp) src/fathom/src/tbprobe.c

OBJS = $(FILES:.cpp=.o)
OBJS := $(OBJS:.c=.o)

OPTIMIZE = -O3 -fno-stack-protector -fno-math-errno -funroll-loops -fno-exceptions -flto -flto-partition=one

COMMON_FLAGS = -s -pthread -DNDEBUG -DEvalFile=\"$(EVALFILE)\" $(OPTIMIZE)

CXXFLAGS += -std=c++17 $(COMMON_FLAGS)
CFLAGS += $(COMMON_FLAGS)

ifeq ($(OS),Windows_NT)
    CXXFLAGS += -static
    CFLAGS += -static
endif

ifeq ($(build),)
	build = native
endif

ifeq ($(build), native)
    CXXFLAGS += -march=native
    CFLAGS += -march=native
else ifeq ($(findstring sse2, $(build)), sse2)
        CXXFLAGS += $(MSSE2)
        CFLAGS += $(MSSE2)
else ifeq ($(findstring ssse3, $(build)), ssse3)
        CXXFLAGS += $(MSSSE3)
        CFLAGS += $(MSSSE3)
else ifeq ($(findstring avx2, $(build)), avx2)
        CXXFLAGS += $(MAVX2)
        CFLAGS += $(MAVX2)
else ifeq ($(findstring avx512, $(build)), avx512)
        CXXFLAGS += $(MAVX512)
        CFLAGS += $(MAVX512)
endif

ifeq ($(build), native)
        PROPS = $(shell echo | $(CC) -march=native -E -dM -)
        ifneq ($(findstring __BMI2__, $(PROPS)),)
                ifeq ($(findstring __znver1, $(PROPS)),)
                        ifeq ($(findstring __znver2, $(PROPS)),)
                                CXXFLAGS += -DUSE_PEXT
                                CFLAGS += -DUSE_PEXT
                        else ifeq ($(shell uname), Linux)
                                ifneq ($(findstring AMD EPYC 7B, $(shell lscpu)),)
                                        CXXFLAGS += -DUSE_PEXT
                                        CFLAGS += -DUSE_PEXT
                                endif
                        endif
                endif
        endif
else ifeq ($(findstring avx512, $(build)), avx512)
        CXXFLAGS += -DUSE_PEXT -mbmi2
        CFLAGS += -DUSE_PEXT -mbmi2
else ifeq ($(findstring pext, $(build)), pext)
        CXXFLAGS += -DUSE_PEXT -mbmi2
        CFLAGS += -DUSE_PEXT -mbmi2
endif

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

make: download-net $(FILES)
	$(CXX) $(CXXFLAGS) $(FILES) -o $(EXE) -fprofile-generate="sirio_pgo" $(LDFLAGS)
ifeq ($(OS),Windows_NT)
	$(EXE) bench
else
	./$(EXE) bench
endif
	$(CXX) $(CXXFLAGS) $(FILES) -o $(EXE) -fprofile-use="sirio_pgo" $(LDFLAGS)
ifeq ($(OS),Windows_NT)
	powershell.exe -Command "Remove-Item -Recurse -Force sirio_pgo"
else
	rm -rf sirio_pgo
endif

nopgo: download-net $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(EXE) $(LDFLAGS)

clean:
	rm -f $(OBJS)

download-net:
ifdef DOWNLOAD_NET
	@if test -f "$(EVALFILE)"; then \
		echo "File $(EVALFILE) already exists, skipping download."; \
	else \
		echo Downloading net; \
		curl -sOL https://github.com/gab8192/SirioC-nets/releases/download/nets/$(EVALFILE); \
	fi
endif
