# Good and Bad Tests

Tests live in `tests/rando/unit/test_<module>.c`: `static void` test functions using `assert`, called from `main`. Names below are illustrative.

## Good Tests

**Integration-style**: test through the module's public header, not its internals.

```c
/* GOOD: observable behavior, through the public interface */
static void test_shuffle_is_a_permutation(void) {
    int items[5] = {1, 2, 3, 4, 5};
    prng_t prng = prng_make(1);
    fy_shuffle(&prng, items, 5);
    int sum = 0;
    for (int i = 0; i < 5; i++) sum += items[i];
    assert(sum == 15);
}
```

Characteristics:

- Tests behavior callers care about
- Includes only the public header
- Survives internal refactors
- Name describes WHAT, not HOW
- One logical assertion per test

For a behavior only a whole generation reaches, build the smallest world state that reaches it — a few items placed, one region — and assert on what the public API returns, never by calling the inner function directly.

## Bad Tests

**Implementation-detail tests**: coupled to internal structure.

```c
/* BAD: reaches into a static helper by including the .c */
#include "filler.c"
static void test_fill_uses_location_cache(void) {
    location_cache_fill(&cache, world, 3);
    assert(cache.count == 3);
}
```

Red flags:

- Including a `.c` file, or a test-only header exposing statics
- Asserting on call counts or call order
- Test breaks when refactoring without behavior change
- Test name describes HOW, not WHAT
- Verifying through a side channel instead of the interface
- Running a whole seed to reach a state a few calls could build

## Assertions and Tests: Complementary Layers

Tests verify behavior from the outside. Assertions enforce contracts from the inside. Both are needed.

When production code asserts, a failing test tells you not just _that_ something broke but _where_ the contract was violated:

```c
/* Without assertion: a bad id reads past the table, fails much later */
const char *item_name(int item) {
    return item_names[item];
}

/* With assertion: fails at the violated contract */
const char *item_name(int item) {
    assert(item >= 0 && item < ITEM_COUNT && "item id out of range");
    return item_names[item];
}
```

Rule of thumb: if a test setup must respect a constraint to call a function, that constraint is asserted inside the function.

```c
/* BAD: verifies by poking the world's storage */
static void test_place_item_fills_location(void) {
    world_place(&world, LOCATION_LINKS_HOUSE, ITEM_BOW);
    assert(world.slots[LOCATION_LINKS_HOUSE] == ITEM_BOW);
}

/* GOOD: verifies through the interface */
static void test_placed_item_is_found_at_its_location(void) {
    world_place(&world, LOCATION_LINKS_HOUSE, ITEM_BOW);
    assert(world_item_at(&world, LOCATION_LINKS_HOUSE) == ITEM_BOW);
}
```
