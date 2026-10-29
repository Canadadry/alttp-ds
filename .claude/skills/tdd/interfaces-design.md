# Interface Design for Testability

Good interfaces make testing natural. Names are illustrative.

1. **Accept dependencies, don't create them**

   ```c
   /* Testable */
   int rando_generate(const rando_config_t *config, uint32_t seed, rando_result_t *out);

   /* Hard to test */
   int rando_generate(void);   /* reads the save slot, writes into the game's globals */
   ```

2. **Return results, don't produce side effects**

   ```c
   /* Testable */
   bool region_can_enter(const rando_world_t *world, const item_set_t *items, int region);

   /* Hard to test */
   void region_update_reachable(int region);  /* writes into a global reachability table */
   ```

3. **Small surface area**
   - Fewer functions in the header = fewer tests needed
   - Fewer parameters = simpler test setup

4. **Make illegal states unrepresentable**

   A field meaningful only in some cases says the type is wrong. Link by id, never by pointer (see `warn-review`'s pointer rules):

   ```c
   /* Allows an impossible state: a pointer into a world that was reset */
   typedef struct { const location_t *location; } placement_t;

   /* A checked id, valid across resets */
   typedef struct { location_id_t location; item_id_t item; } placement_t;
   ```

5. **Keep functions pure; push state to the top**

   Core logic is pure: same input, same output, no I/O, no global. A thin top layer holds the state and calls into it.

   ```c
   /* Pure: trivial to test */
   bool world_is_winnable(const rando_world_t *world);

   /* Stateful: orchestrates, stays at the top */
   int rando_generate(const rando_config_t *config, uint32_t seed, rando_result_t *out) {
       /* reseed, build the world, fill, check winnable, retry */
   }
   ```

6. **Assert preconditions at the boundary**

   Every public function asserts what it requires, so invalid input fails at the call site, not a thousand draws later.

   ```c
   void world_place(rando_world_t *world, location_id_t location, item_id_t item) {
       assert(location < LOCATION_COUNT && "location id out of range");
       assert(world_item_at(world, location) == ITEM_NONE && "location already filled");
       /* ... */
   }
   ```

   Assertions are not input validation for player data — they are contracts between modules. Key invariants inside a module ("every item of the pool is placed exactly once") belong as assertions too, not just as tests.
