CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror
RANDO := ../../rando
BUILD := build

UNIT_TESTS := test_prng test_items test_bosses test_world test_regions test_options test_case_reader test_pool test_prepare test_generate test_spoiler_json
# No fixture mode: test_prng already replays its vectors in `test`, the others have no fixture.
LOGIC_TESTS := $(filter-out test_prng test_options test_case_reader test_pool test_prepare test_spoiler_json,$(UNIT_TESTS))

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
RANDO_HDR := $(wildcard $(RANDO)/include/*.h $(RANDO)/src/*.h)
RANDO_OBJ := $(patsubst $(RANDO)/src/%.c,$(BUILD)/obj/%.o,$(RANDO_SRC))
RANDO_LIB := $(BUILD)/librando.a
HOST_SRC := $(wildcard $(RANDO)/host/*.c)
HOST_HDR := $(wildcard $(RANDO)/host/*.h)
HOST_OBJ := $(patsubst $(RANDO)/host/%.c,$(BUILD)/host/%.o,$(HOST_SRC))
HOST_LIB := $(BUILD)/librando_host.a
INCLUDES := -I$(RANDO)/include -I$(RANDO)/host
SUPPORT_OBJ := $(BUILD)/support.o
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
GEN := $(BUILD)/rando_gen
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

e2e: $(GEN)
	@./run_e2e.sh

$(BUILD)/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

# rm first: `ar r` would keep the object of a deleted source.
$(RANDO_LIB): $(RANDO_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(BUILD)/host/%.o: $(RANDO)/host/%.c $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(HOST_LIB): $(HOST_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(SUPPORT_OBJ): unit/support.c unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(SUPPORT_OBJ) $(HOST_LIB) $(RANDO_LIB)

$(GEN): $(RANDO)/host/main/rando_gen.c $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(HOST_LIB) $(RANDO_LIB)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
