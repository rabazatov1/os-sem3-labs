CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

BUILD := build
HEADERS := $(wildcard include/*.h)

all: $(BUILD)/parent $(BUILD)/child

$(BUILD)/parent: $(BUILD)/parent.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/child: $(BUILD)/child.o $(BUILD)/sum.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
