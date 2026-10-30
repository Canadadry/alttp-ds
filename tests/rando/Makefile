CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror
RANDO := ../../rando
BUILD := build

UNIT_TESTS := test_prng test_items test_bosses test_world test_regions
# test_prng has no fixture mode: it already replays its vectors in `test`.
LOGIC_TESTS := $(filter-out test_prng,$(UNIT_TESTS))

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
RANDO_HDR := $(wildcard $(RANDO)/include/*.h $(RANDO)/src/*.h)
RANDO_OBJ := $(patsubst $(RANDO)/src/%.c,$(BUILD)/obj/%.o,$(RANDO_SRC))
RANDO_LIB := $(BUILD)/librando.a
SUPPORT_OBJ := $(BUILD)/support.o
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
LOGIC_BINS := $(addprefix $(BUILD)/,$(LOGIC_TESTS))

.PHONY: test logic selftest e2e clean

test: $(UNIT_BINS)
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@./selftest.sh discovery
	@echo "test: $(words $(UNIT_TESTS)) unit tests and the discovery self-test ran"

logic: $(LOGIC_BINS)
	@set -e; for t in $(LOGIC_BINS); do ./$$t --fixtures; done
	@echo "logic: $(words $(LOGIC_TESTS)) unit tests replayed the oracle fixtures"

selftest:
	@./selftest.sh

e2e:
	@./run_e2e.sh

$(BUILD)/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

# rm first: `ar r` would keep the object of a deleted source.
$(RANDO_LIB): $(RANDO_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(SUPPORT_OBJ): unit/support.c unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(RANDO_LIB) $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -o $@ $< $(SUPPORT_OBJ) $(RANDO_LIB)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
