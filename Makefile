# Compiler and flags
#
# macOS: mpic++ (just a convenient, already-installed C++ wrapper -- nothing here actually
# calls MPI) against Homebrew's sqlite, since Apple's own /usr/include/sqlite3.h isn't suitable
# for linking against here.
#
# Linux (e.g. a throwaway AWS box for a fillRows() run): plain g++ against the system sqlite3
# dev package (`apt install libsqlite3-dev`). This code uses <format>/<bit>/std::lcm, so the
# default g++ has to be GCC >= 13 -- Ubuntu 20.04's default g++-9 is too old. If `g++ --version`
# on the box is older than that, install one (e.g. `add-apt-repository ppa:ubuntu-toolchain-r/test
# && apt install g++-13`) and override: `make CXX=g++-13`. Also needs -lpthread explicitly for
# std::thread, which macOS/clang links in implicitly but glibc does not.
# Note: plain `=`, not `?=` -- GNU Make ships its own built-in default for CXX (c++), so `?=`
# would silently no-op here and never pick mpic++/g++ up. A command-line `make CXX=...` still
# overrides this either way; only `override CXX = ...` (not used here) would prevent that.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    CXX = mpic++
    SQLITE_PREFIX = $(shell brew --prefix sqlite 2>/dev/null)
else
    CXX = g++
    SQLITE_PREFIX = /usr
endif

CXXFLAGS  = -Wall -O3 -std=c++20 -I$(SQLITE_PREFIX)/include -MMD -MP
DEBUG_CXXFLAGS = -Wall -g -O0 -std=c++20 -I$(SQLITE_PREFIX)/include -MMD -MP -fsanitize=address -fno-omit-frame-pointer
LDFLAGS   = -L$(SQLITE_PREFIX)/lib
LDLIBS    = -lsqlite3 -lpthread

# Targets
TARGETS   = analyze

ANALYZE_SRCS = main.cpp database.cpp poker.cpp uth.cpp
ANALYZE_OBJS = $(ANALYZE_SRCS:.cpp=.o)
ANALYZE_DEBUG_OBJS = $(ANALYZE_SRCS:.cpp=.dbg.o)

# Default build rule
all: $(TARGETS)

analyze: $(ANALYZE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# Debug build: no optimization, debug symbols, AddressSanitizer.
# Uses separate *.dbg.o objects so it never clobbers (or is clobbered by)
# the optimized release build's *.o files.
debug: analyze_debug

analyze_debug: $(ANALYZE_DEBUG_OBJS)
	$(CXX) $(DEBUG_CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.dbg.o: %.cpp
	$(CXX) $(DEBUG_CXXFLAGS) -c $< -o $@

# Generic compilation rule
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Auto-generated header dependencies (created by -MMD -MP above)
DEPS = $(ANALYZE_OBJS:.o=.d) $(ANALYZE_DEBUG_OBJS:.o=.d)
-include $(DEPS)

# Cleanup
clean:
	rm -f *.o *.dbg.o *.d $(TARGETS) analyze_debug
