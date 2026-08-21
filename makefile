CC := g++
TARGET := search

# Detect the operating system
ifeq ($(OS),Windows_NT)
	# Windows
	CFLAGS := -O2 -std=c++11 -Wall -c
    LFLAGS := -lgdi32
#     LFLAGS :=
    EXTENSION := .exe
	CLEANUP := del
	CLEANUP_OBJS := del *.o
# 	CLEANUP_OBJS := del *.exe
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Darwin)
		# macOS (needs: brew install sdl2 sdl2_ttf)
		EXTENSION := .out
		CFLAGS := -O2 -std=c++11 -Wall -c $(shell sdl2-config --cflags)
		LFLAGS := $(shell sdl2-config --libs) -lSDL2_ttf
		CLEANUP := rm -f
		CLEANUP_OBJS := rm -f *.o
	else ifeq ($(UNAME_S),Linux)
		# Linux (needs: apt install libsdl2-dev libsdl2-ttf-dev)
		EXTENSION := .out
		CFLAGS := -O2 -std=c++11 -Wall -c $(shell sdl2-config --cflags)
		LFLAGS := $(shell sdl2-config --libs) -lSDL2_ttf
		CLEANUP := rm -f
		CLEANUP_OBJS := rm -f *.o
	endif
endif

# Find all source files (.cpp) and header files (.h)
SRCS := $(wildcard *.cpp) $(wildcard */*.cpp)
HDRS := $(wildcard *.h) $(wildcard */*.h)

# Create object file names based on source file names
OBJS := $(SRCS:.cpp=.o)

# Output executable
# EXECUTABLE := search.exe

# Rule to build the executable
$(TARGET)$(EXTENSION): $(OBJS)
	$(CC) -O2 -Wl,-s -o $@ $(OBJS) $(LFLAGS)

# Rule to build object files
%.o: %.cpp $(HDRS)
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(CLEANUP) $(TARGET)$(EXTENSION)
	$(CLEANUP_OBJS)
