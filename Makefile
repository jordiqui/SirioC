ifeq ($(OS),Windows_NT)
        EXE := SirioC.exe
else
        EXE := SirioC
endif

DEFAULT_NET = nn-6877cd24400e.nnue
DOWNLOAD_BASE_URL = https://raw.githubusercontent.com/official-stockfish/networks/master

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

OPTIMIZE = -O3 -fno-stack-protector -fno-math-errno -funroll-loops -fno-exceptions -flto

COMMON_FLAGS = -s -pthread -DNDEBUG -DEvalFile=\"$(EVALFILE)\" $(OPTIMIZE)

CXXFLAGS += -std=c++17 $(COMMON_FLAGS)
CFLAGS += $(COMMON_FLAGS)

CXX_LTO_PARTITION = -flto-partition=one
ifneq ($(filter clang%,$(notdir $(CXX))),)
    CXX_LTO_PARTITION =
endif
CXXFLAGS += $(CXX_LTO_PARTITION)

C_LTO_PARTITION = -flto-partition=one
ifneq ($(filter clang%,$(notdir $(CC))),)
    C_LTO_PARTITION =
endif
CFLAGS += $(C_LTO_PARTITION)

ifeq ($(OS),Windows_NT)
    CXXFLAGS += -static
    CFLAGS += -static
endif

# Enlazado robusto (evita dependencias dinámicas de MinGW si usas GCC/Clang)
LDFLAGS += -Wl,--gc-sections
# Si usas Clang/LLD, puedes activar estático de C++ en entornos CI/MSYS2:
# LDFLAGS += -static -static-libgcc -static-libstdc++

ifeq ($(build),)
	build = native
endif

ifeq ($(build), native)
    ARCH_NAME := native
    CXXFLAGS += -march=native
    CFLAGS += -march=native
else ifeq ($(build), x86-64-sse41-popcnt)
    ARCH_NAME := sse41-popcnt
    CFLAGS    += -msse4.1 -mpopcnt -mno-avx -mno-avx2 -mno-bmi -mno-bmi2 -mno-fma -mno-f16c
else ifeq ($(findstring sse2, $(build)), sse2)
        ARCH_NAME := sse2
        CXXFLAGS += $(MSSE2)
        CFLAGS += $(MSSE2)
else ifeq ($(findstring ssse3, $(build)), ssse3)
        ARCH_NAME := ssse3
        CXXFLAGS += $(MSSSE3)
        CFLAGS += $(MSSSE3)
else ifeq ($(findstring avx2, $(build)), avx2)
        ARCH_NAME := avx2
        CXXFLAGS += $(MAVX2)
        CFLAGS += $(MAVX2)
else ifeq ($(findstring avx512, $(build)), avx512)
        ARCH_NAME := avx512
        CXXFLAGS += $(MAVX512)
        CFLAGS += $(MAVX512)
endif

# --- SirioC: Ivy-safe target (SSE4.1 + POPCNT, sin AVX2/BMI2/FMA) ---
ifeq ($(build),x86-64-sse41-popcnt)
    ARCH_NAME := sse41-popcnt
    CXXFLAGS  += -msse4.1 -mpopcnt -mno-avx -mno-avx2 -mno-bmi -mno-bmi2 -mno-fma -mno-f16c
    # Marca de compilación para enlazar selects/dispatch a este perfil
    CXXFLAGS  += -DSIRIOC_REQUIRE_SSE41=1
endif

# Fallback seguro si alguien pasa un build no soportado
ifeq ($(ARCH_NAME),)
    $(warning Unknown build '$(build)'; falling back to sse2)
    ARCH_NAME := sse2
    CXXFLAGS  += -msse2 -DSIRIOC_REQUIRE_SSE2=1
    CFLAGS    += -msse2 -DSIRIOC_REQUIRE_SSE2=1
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
			echo "Downloading NNUE network $(notdir $(EVALFILE)) from official Stockfish repository..."; \
			mkdir -p "$(dir $(EVALFILE))"; \
			tmp_file="$(EVALFILE).tmp"; \
			if curl -sSfL -o "$$tmp_file" "$(DOWNLOAD_BASE_URL)/$(notdir $(EVALFILE))"; then \
				mv "$$tmp_file" "$(EVALFILE)"; \
			else \
				rm -f "$$tmp_file"; \
				echo "Failed to download $(notdir $(EVALFILE)) from $(DOWNLOAD_BASE_URL)"; \
				exit 1; \
			fi; \
		fi
endif
