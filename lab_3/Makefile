CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LDLIBS += -pthread

BUILD := build
APP := $(BUILD)/bitonic
OBJS := $(BUILD)/main.o $(BUILD)/bitonic.o
HDRS := $(wildcard include/*.h)

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: src/%.c $(HDRS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -c $< -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
