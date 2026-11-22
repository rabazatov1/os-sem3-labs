CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude

BUILD := build
HEADERS := $(wildcard include/*.h)

all: $(BUILD)/libimpl1.so $(BUILD)/libimpl2.so

$(BUILD)/libimpl1.so: src/impl1.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -lm -o $@

$(BUILD)/libimpl2.so: src/impl2.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -lm -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
