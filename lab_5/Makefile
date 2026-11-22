CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude

BUILD := build
HEADERS := $(wildcard include/*.h)

all: $(BUILD)/libimpl1.so $(BUILD)/libimpl2.so $(BUILD)/program1 $(BUILD)/program2

$(BUILD)/libimpl1.so: src/impl1.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -lm -o $@

$(BUILD)/libimpl2.so: src/impl2.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -shared $< -lm -o $@

$(BUILD)/program1: src/program1.c src/cli.c $(HEADERS) $(BUILD)/libimpl1.so
	$(CC) $(CPPFLAGS) $(CFLAGS) src/program1.c src/cli.c -L$(BUILD) -limpl1 -Wl,-rpath,'$$ORIGIN' -lm -o $@

$(BUILD)/program2: src/program2.c src/cli.c $(HEADERS) $(BUILD)/libimpl1.so $(BUILD)/libimpl2.so
	$(CC) $(CPPFLAGS) $(CFLAGS) src/program2.c src/cli.c -ldl -lm -o $@

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean
