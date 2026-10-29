CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror
RANDO := ../../rando
BUILD := build

UNIT_TESTS :=

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))

.PHONY: test e2e clean

test: $(UNIT_BINS)
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@echo "test: $(words $(UNIT_TESTS)) unit tests ran"

e2e:
	@./run_e2e.sh

$(BUILD)/test_%: unit/test_%.c $(RANDO_SRC) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -o $@ $< $(RANDO_SRC)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
