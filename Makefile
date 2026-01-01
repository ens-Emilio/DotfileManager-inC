CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude
LDFLAGS ?=
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
TARGET := build/dotmgr

.PHONY: all clean dirs

all: $(TARGET)

$(TARGET): dirs $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

dirs:
	@mkdir -p build

clean:
	rm -rf build
