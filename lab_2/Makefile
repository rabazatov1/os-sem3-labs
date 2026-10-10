CFLAGS   ?= -std=c11 -Wall -Wextra -pedantic -g
CPPFLAGS += -Iinclude -D_POSIX_C_SOURCE=200809L

BUILD  := build
PARENT := $(BUILD)/parent
CHILD  := $(BUILD)/child
HDRS   := $(wildcard include/*.h)

all: $(PARENT) $(CHILD)

$(PARENT): $(BUILD)/parent.o $(BUILD)/common.o
	$(CC) $(CFLAGS) $^ -o $@

$(CHILD): $(BUILD)/child.o $(BUILD)/common.o $(BUILD)/sum.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: src/%.c $(HDRS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $@

run: all
	./$(PARENT)

strace: all
	strace -f -e trace=process,desc ./$(PARENT)

clean:
	rm -rf $(BUILD)

.PHONY: all run strace clean
