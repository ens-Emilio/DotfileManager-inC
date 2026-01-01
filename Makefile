CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Iinclude
LDFLAGS ?=
SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
TARGET := build/dotmgr
TEST_SRCS := $(wildcard tests/*.c)
TEST_BINS := $(patsubst tests/%.c,build/tests/%,$(TEST_SRCS))

.PHONY: all clean dirs test

all: $(TARGET)

$(TARGET): dirs $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

build/%.o: src/%.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@

build/tests/%: tests/%.c build/utils.o | dirs
	$(CC) $(CFLAGS) $< build/utils.o -o $@

dirs:
	@mkdir -p build build/tests

clean:
	rm -rf build

test: $(TARGET) $(TEST_BINS)
	@for t in $(TEST_BINS); do \
		echo ">> Running $$t"; \
		"$$t"; \
	done
