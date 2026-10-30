---
title: "dungeon_items row: two-world location rules and a stateless playthrough"
description: "dungeon_items-mcs differs from VT only in its playthrough, because the C rules read one world where VT's rules read two: the shadow world for nested reads and the real world for the top-level check."
status: needs-triage
---

## Problem Statement

The randomizer's C port has to reproduce VT's spoiler byte for byte, playthrough included. Two of the
three `dungeon_items` cases already do: since step 3.0, `dungeon_items-mc` and `dungeon_items-full`
are claimed at `all`. `dungeon_items-mcs` is still claimed only at `prepareWorld`. Its trace, attempts
and placements match VT, but its playthrough does not: the first difference is at spoiler line 479,
where the golden has `Eastern Palace - Cannonball Chest` and the C port has
`Eastern Palace - Map Chest`.

The cause is the shape of the location rules (step 2.4 note). In VT, a requirement closure is
`function ($locations, $items)`, and it reads two worlds:

- `$locations` is whatever the caller of `Location::canAccess($items, $locations)` passed in. It is
  read by `$locations->itemInLocations(...)` and `$locations["X"]->hasItem(...)`, passed on by
  `getRegion(X)->canEnter($locations, $items)`, and is the world of `$locations["X"]->canAccess($items)`.
- `$this->locations` and `$this->world` belong to the region's own world, the *self* world. They are
  read by `$this->locations["X"]->hasItem(...)` and by the nested `$this->locations["X"]->canAccess($items)`,
  which has no second argument and so resets `$locations` to the self world.

Outside the playthrough the two worlds are always the same. In `PlaythroughService::getPlayThrough`
they are not: the bubbling pass and the replay call `$shadow_location->canAccess($items, $world->getLocations())`,
so the self world is the shadow and `$locations` is the real world. The C rules take one
`const RandoWorld *`, and the host playthrough hands them the real world, so each nested and
self-world read looks at the wrong item. In `dungeon_items-mcs`, "Ice Palace - Big Key Chest" calls
Spike Room's `canAccess($items)`, whose `itemInLocations(BigKeyD5, ...)` must then read the shadow
world, where the prune has taken items out.

The step also has a second job, given to it by Phase 3's execution order: the host playthrough keeps
all of its working state (the shadow world, spheres, required lists) in file-scope statics, and is
documented as not reentrant.

Finally, the row owes two VT tests (step 3.0's owed table):

- `WorldTest::testSetVanillaDungeonItems`, all 8 rows, including the HybridMajorGlitches and NoLogic
  rows that step 3.0 assigned here.
- The top-level `RegionTest::testRegionLockedItems`.

This is Phase 3 step 6. It comes before every other logic step because it settles the rule shape
that the glitch and inverted rules will be written in.

## Solution

Every access rule gets the second world as an explicit parameter. It mirrors VT's closure: `world`
is VT's `$this->world`/`$this->locations`, and `locations` is VT's `$locations`. Each VT read is
mapped one to one onto its source. The library's single-world entry points keep their meaning
(self = locations). A new public entry point, VT's `canAccess($items, $locations)`, lets the host
playthrough check a shadow location against the real world. The playthrough uses it where VT does,
and keeps its working state in a local struct instead of statics. `dungeon_items-mcs` is then raised
to `all`. Both owed VT tests are ported as oracle fixture rows replayed by `make logic`.

A throwaway prototype of exactly this design was built twice, in scratch copies: first on the
pre-3.0 HEAD, then again on the committed step 3.0 code. Neither was committed. On the step 3.0 code:

- The region rule files are byte-identical to before step 3.0, so the prototype's region edits applied
  as they were. The world module and playthrough edits applied unchanged on the refactored files.
- With the 42 supported cases claimed at `all`, 41 are byte-identical, `dungeon_items-mcs` among them.
  The 42nd, `glitches-no_logic`, diverges at fill draw 457, as it does on HEAD; that is its own row.
- `make test`, `make logic` and the committed-claims e2e are all green (42 pass, 0 divergent,
  2 pending).

## User Stories

1. As the randomizer maintainer, I want `dungeon_items-mcs` claimed at `all`, so that the whole `dungeon_items` row is green.
2. As the randomizer maintainer, I want the playthrough of every supported case to stay byte-identical to VT after the rule change, so that this step breaks no earlier claim.
3. As a porter of VT rules, I want a C rule's parameters to match a VT closure's (`$this->world` → `world`, `$locations` → `locations`, `$items` → `items`), so that each line of a C rule can be checked against VT by eye.
4. As a porter of glitch rules (Phase 3, `glitches` steps), I want the rule shape fixed before I start, so that I don't rewrite every rule twice.
5. As a porter of inverted rules (Phase 3, `state-inverted`), I want region `can_enter` rules to receive `locations` too, so that an inverted region reading `$locations` in its entry closure needs no new rule shape.
6. As a porter, I want every `$locations->itemInLocations(...)` and `$locations["X"]->hasItem(...)` to read the `locations` world, so that key logic gets VT's answer in the playthrough.
7. As a porter, I want every `$this->locations["X"]->hasItem(...)` (the boss compass/map checks) to read the self world, so that the wild compass and map checks match VT.
8. As a porter, I want a nested `$this->locations["X"]->canAccess($items)` to run with self = locations = the self world, so that the Ice Palace Spike Room, every `can_complete` → Boss call and the Ganon's Tower Moldorm Chest call reset the locations world as VT does.
9. As a porter, I want a nested `$locations["X"]->canAccess($items)` (Purple Chest → Blacksmith, Swamp Palace → Old Man) to run with self = locations = the locations world, so that those calls match VT, even where no case can tell the difference yet.
10. As a porter, I want `getRegion(X)->canEnter($locations, $items)` to pass both worlds on unchanged, so that the locations world reaches the rules that read it.
11. As a porter, I want fill and always-allow rules left with their single world, so that I don't thread a parameter VT never varies (VT passes the region's own world to fill callbacks).
12. As a library user, I want `rando_world_can_access`, `rando_world_region_can_enter`, `rando_world_region_can_complete`, `rando_world_can_access_shop`, `rando_world_collect_until_stable` and the win condition to keep their one-world signatures and meaning, so that the fill, `isWinnable`, `prepareWorld` and the tests don't change.
13. As the host playthrough, I want a public entry point taking a location's own world and a separate locations world, so that I can reproduce VT's `canAccess($items, $world->getLocations())` on shadow locations.
14. As the host playthrough, I want the sphere building to keep checking shadow locations against the shadow world, so that `getLocationSpheres` (which calls `canAccess($items)`) is unchanged.
15. As the host playthrough, I want the bubbling pass and the replay to check shadow locations against the real world, so that they match VT's two `canAccess($items, $world->getLocations())` calls.
16. As the host playthrough, I want shop slots to keep using the real world for both reads, so that the copy quirk VT has (a shadow shop slot belongs to a real-world region) stays reproduced.
17. As a host tool author, I want the playthrough to keep no file-scope state, so that two calls can never leak state into each other and the "not reentrant" contract goes away.
18. As a DS integrator (roadmap item 2), I want the extra rule parameter to cost little, so that the ARM9 budget still holds. Against step 3.0, the prototype measured +0.4 KB ARM code, +1.1 KB host code and +48 B of stack peak.
19. As the randomizer maintainer, I want hand-written unit tests for each kind of read source, so that a rule reading the wrong world fails `make test` even where no golden can tell the difference.
20. As the randomizer maintainer, I want `make -C tests/rando test` and `e2e` no slower than step 3.0's figures, so that the fast loop stays fast.
21. As the randomizer maintainer, I want VT's top-level `RegionTest::testRegionLockedItems` replayed against the C port, so that base-region `canFill` with toggled wild maps and compasses is checked against VT.
22. As the randomizer maintainer, I want VT's `WorldTest::testSetVanillaDungeonItems` replayed against the C port, all 8 rows (NoGlitches, MajorGlitches, HybridMajorGlitches, NoLogic, each with dungeon items wild or not), so that the four `rom.vanilla*` derivations the C config already computes are checked against VT.
23. As the randomizer maintainer, I want the new oracle fixture rows generated by VT itself, with every existing golden and fixture byte-identical, so that the oracle stays the source of truth.
24. As a reviewer, I want the step note to record the e2e time, the `make test` time and `make measure`'s figures against step 3.0's, so that I can see the cost of the change.
25. As a reviewer, I want the playthrough's `TODO` about the shadow world removed together with its cause, so that no stale note is left behind.
26. As a future reader, I want the read-source rule (which VT form reads which world) written down once, next to the rule type, so that new rules follow it.

## Implementation Decisions

### Rule shape: a third parameter, VT's `$locations`

- An access rule is `bool rule(const RandoWorld *world, const RandoWorld *locations, const RandoItems *items)`.
  This covers location requirements, region `can_enter` and `can_complete`, and shop requirements:
  every rule of the `RandoRule` type. Local helpers called from rules with `(world, items)` get the
  same extra parameter. `has_lamp`, `boss_can_beat`, `boss_has_wild_compass_and_map`,
  `wild_or_not_dungeon_item` (now a wrapper over `rando_dungeon_item_confined`) and the other
  config/self-only helpers in the rules header do not.
- Fill rules (`RandoFillRule`) and always-allow rules (`RandoAlwaysAllowRule`) keep their single world.
  VT calls a fill callback with `$this->region->getWorld()->getLocations()`, so self = locations
  there by construction, and always-allow callbacks get no locations.
- The read-source rule, written once as a comment next to the rule type (it states a contract the
  type does not):
  - `$locations->itemInLocations`, `$locations["X"]->hasItem` and `$locations["X"]->getItem` read `locations`.
  - `$this->locations["X"]->hasItem` reads `world`.
  - `$this->locations["X"]->canAccess($items)` is the single-world access on `world`.
  - `$locations["X"]->canAccess($items)` is the single-world access on `locations`.
  - `getRegion(X)->canEnter($locations, $items)` passes `(world, locations)`.
  - Config, bosses and `$this->world->collectItems()` read `world`.
- The parameter is named `locations` to mirror VT. Ganon's Tower's `big_key_in` helper has an array
  parameter named `locations`; it is renamed (`chests`). A region file's rule table, also called
  `locations`, is declared after the rules and so is never in scope inside them. If one ever is, the
  table is renamed, never the parameter.
- Rules that don't read `locations` (90 at HEAD) silence it with `(void)locations;`, as the code
  already does for `world` and `items`. `-Wunused-parameter` stays on, and no warning flag changes.

### Alternatives rejected (decided, with evidence from the prototype)

- **One world, the real one (HEAD):** `dungeon_items-mcs` diverges.
- **One world, the shadow, in the bubbling pass and replay:** 39 of the 42 supported cases diverge.
  The locations world really must be the real one.
- **A `{self, locations}` context struct passed by pointer:** semantically identical. Rejected because
  it rewrites every body line touching `world` (`ctx->self->config`) and breaks the one-to-one reading
  against VT closures. The third parameter leaves the bodies alone except for the ~30 lines that read
  `locations`.
- **A locations-view pointer stored in `RandoWorld`:** cannot work. A nested
  `$this->locations[X]->canAccess` resets locations to self on the *same* self world, so the view would
  have to be on and off at once. It would also put a pointer inside a struct that `World::copy` copies
  by value.
- **Threading `locations` only into the rules that can reach a locations read:** rejected. Region
  `can_enter` rules read `$locations` too (the Misery Mire and Turtle Rock medallions, Swamp Palace's
  Old Man and `$mire` under glitch flags), so the boundary covers almost every entry rule. Glitch and
  inverted rules would keep moving it, and two rule types would have to coexist.

### Proof of equivalence over the 42 supported cases

- **By construction:** every VT read form is mapped onto its VT source, using the table above. Outside
  the playthrough, every call passes self = locations, so the library's behaviour (fill, `isWinnable`,
  `prepareWorld`, `rando_world_collect_until_stable`, every `make logic` row) is unchanged by
  definition. Inside the playthrough, the pairs are VT's: (shadow, shadow) for spheres and the win
  condition, (shadow, real) for bubbling and replay, and (real, real) for shop slots.
- **By the goldens, rechecked on the step 3.0 code:** with the 42 supported cases claimed at `all`,
  41 spoilers are byte-identical. `glitches-no_logic` keeps its fill divergence at draw 457, which comes
  before any playthrough.
- **Which distinctions the goldens pin:** each read site was flipped on its own in the first prototype.
  The region files are unchanged since, so the result still holds.
  - Flipping one of these makes cases diverge: Palace of Darkness Big Key Chest key (6 cases), Harmless
    Hellway key (32), Ganon's Tower big-key rooms (32), Ganon's Tower Map Chest (1), and the Ice Palace
    nested Spike Room call (1, `dungeon_items-mcs`).
  - Flipping one of these changes no case: Thieves' Town Big Chest; Misery Mire's big-key chests; both
    medallions; Turtle Rock's roller/compass big key and Big Key Chest; Ganon's Tower Firesnake Room;
    Hera's and Swamp's glitch `$mire`/Old Man reads; Purple Chest → Blacksmith; the boss compass/map
    self reads; `can_complete` → Boss; Ganon's Tower → Moldorm Chest.
  - The unit tests below cover representative unpinned kinds where a difference is observable.
- The medallion reads can't be told apart by any test: a medallion location is never collectable, so
  the playthrough never changes it. They still read `locations`, as VT does.

### World module interface

- New public: `rando_world_can_access_with(const RandoWorld *world, const RandoWorld *locations, RandoLocationId, const RandoItems *)`,
  VT's `Location::canAccess($items, $locations)`. The location belongs to `world`: its region's
  `can_enter` and its requirement both run with `(world, locations)`.
- `rando_world_can_access(world, location, items)` is exactly `rando_world_can_access_with(world, world, ...)`.
- A library-internal `rando_world_region_can_enter_with(world, locations, region, items)` is declared
  in the rules header and backs the rules' `can_enter` helper. `rando_world_region_can_enter`,
  `rando_world_region_can_complete` and `rando_world_can_access_shop` keep their one-world signatures
  and call the rules with `(world, world)`.

### Host playthrough

- The internal access helper takes the location's world and the locations world:
  - Spheres: (shadow, shadow), unchanged.
  - Bubbling and replay: (shadow, real) for world locations.
  - Shop slots: the real world for both, unchanged.
  - The `TODO` about the shadow world is deleted.
- The prune's `rando_dungeon_item_confined` call (from step 3.0) stays as it is.
- All file-scope state moves into one local struct: the shadow world, the shadow shop items, the
  sphere arrays, the required lists and the replay order. `rando_playthrough` declares it on its own
  stack (about 10 KB, host-only) and hands it to the helpers by pointer. The header's "not reentrant"
  sentence is deleted, and the contract becomes "leaves world unchanged" alone. The junk list stays a
  `static const` table.
- The playthrough is host-only: it is not linked into the ARM9 build and is not measured by
  `make measure`.

### Claims and gate

- `dungeon_items-mcs` is raised from `prepareWorld` to `all`; the claims file format is unchanged.
  `dungeon_items-mc` and `-full` stay at `all`. No other claim changes; `glitches-no_logic` stays at
  `prepareWorld` for its own row (3.4).
- After this step: 41 claims at `all`, plus `glitches-no_logic` at `prepareWorld`. e2e stays one
  parallel run per claim (step 3.0's gate), with no new generator run and no new check in `make test`
  beyond the hand-written unit cases.

### Owed VT tests (step 3.0's list wins)

Both are ported as rows of the existing NoGlitches rows fixture, written by VT through the oracle's
logic extractor and replayed by the world test's `--fixtures` mode (`make logic`), never by
`make test`. The extractor learns to run named methods from two top-level test files besides
`tests/NoGlitches`. Those rows use a file column with no directory (`RegionTest.php`,
`WorldTest.php`), and the fixture's header gains one line saying such a file is under `tests/`. Every
existing row stays byte-identical.

- **`RegionTest::testRegionLockedItems`** (provider `dungeonItemsPool`, 91 rows): the existing
  `region_can_fill` query, with region column `Region` (VT's base class) and the row's
  `testSetConfig` override recorded as for the NoGlitches rows. The C replay maps `Region` onto a C
  region whose region-item list is empty, and asserts that it is empty.
- **`WorldTest::testSetVanillaDungeonItems`** (provider `vanillaDungeonItemsPool`, all 8 rows): a new
  query, `vanilla_dungeon_items`.
  - VT builds the world through `World::factory('standard', ...)` from the test config plus the row's
    `logic` and the four `region.wild*` keys, and records its four `rom.vanilla*` values.
  - The C replay builds the same constructor config, runs world init (so `derive_config` and its
    `apply_logic_glitches` run), and compares `rom_vanilla_keys`, `rom_vanilla_big_keys`,
    `rom_vanilla_compasses` and `rom_vanilla_maps`.
  - The C config already derives these four flags, so no library change is expected. If a row
    disagrees, the derivation is fixed to match VT. VT uses `config(key, default)` where C uses `||`;
    the two differ only when a flag is preset, which no row does.
- Regenerating the fixture is its own commit (an oracle change). That commit's check is that every
  golden case and every other fixture comes out byte-identical. `gen_golden.sh` is run for the
  fixture and never to turn a case green.

## Implementation Chunks

1. **Two-world rule shape (library).**
   - Change the rule type and the rules header's `can_enter` helper.
   - Add the world module's two-world entry points.
   - Apply the mechanical rewrite across the 25 region rule files: 183 rule signatures plus the
     helpers taking `(world, items, ...)`; every `name(world, items` helper call; every
     `can_enter(world, ...)`.
   - Hand-switch the locations-read sites:
     - Palace of Darkness: 2 sites.
     - Ice Palace Spike Room: 3 reads.
     - Thieves' Town Big Chest.
     - Misery Mire: 2 big-key reads and 3 medallion reads.
     - Tower of Hera `$mire`: 2 reads.
     - Turtle Rock: 3 medallion reads, `big_key_in` and the Big Key Chest (2 reads).
     - Ganon's Tower: `big_key_in`, Firesnake Room, Map Chest (2 reads).
     - Purple Chest → Blacksmith and Swamp Palace → Old Man as `locations` accesses.
   - Add `(void)locations;` where the compiler asks for it.
   - Add the read-source unit tests.
   - Gate: `make test`, `make logic` and `make e2e` all unchanged (42 pass, 0 divergent, 2 pending).
2. **Playthrough: VT's world pairs and no statics.**
   - The bubbling pass and the replay use `rando_world_can_access_with(shadow, real, ...)`.
   - The playthrough's state moves to a local struct; the `TODO` and the "not reentrant" contract go.
   - Raise `dungeon_items-mcs` to `all`.
   - Gate: e2e still 42 pass with the raised claim, and an all-`all` survey of the 42 supported cases
     leaves only `glitches-no_logic` divergent.
3. **Owed VT tests.**
   - Extend the oracle extractor to the two top-level methods (one new query) and regenerate the
     fixture in its own commit (goldens and other fixtures byte-identical).
   - Then the replay commit, adding the `vanilla_dungeon_items` query and the base-`Region` mapping to
     the fixture replay.
   - Independent of chunks 1–2. It goes last so the step note can list what was ported.
   - Then run `make measure` and write the `### Step 3.6` note (see Further Notes).

## Testing Decisions

- **A good test here:**
  - It goes through the public world interface (`rando_world_can_access_with`, `rando_world_can_access`)
    and region rules.
  - It sets items on two separate worlds and asserts which world's item decided the answer.
  - It never inspects how a rule is written.
- **Hand-written cases in the regions test (`make test`, prior art: the existing key-count and
  big-key cases there).** Each one builds a self world and a locations world that differ in one
  location's item, then checks both orders.
  1. Ice Palace Spike Room, top level: the big key's position is read from `locations`.
  2. Ice Palace Big Key Chest: the nested Spike Room call reads the big key from `world`. This is the
     `dungeon_items-mcs` case.
  3. Thieves' Town Big Chest: the KeyD4 position is read from `locations`. No golden pins this one.
  4. A boss with wild maps and compasses: the compass/map on the boss location is read from `world`.
     No golden pins this one.
  5. `rando_world_can_access(w, ...)` equals `rando_world_can_access_with(w, w, ...)` on a mixed sample
     of the above.
- **No new playthrough unit test.**
  - The (shadow, real) pairing is pinned by the `dungeon_items-mcs` golden at `all`.
  - Building a hand-written playthrough where it matters would mean freezing a long spoiler, and the
    golden already pins it.
  - The existing playthrough cases must stay green unchanged. If one froze the old single-world
    behaviour it is updated, since the goldens are the truth.
- **No test for removing the statics:** it is a structural change, guarded by the existing playthrough
  cases and e2e.
- **The owed VT rows (91 + 8) are replayed by `make logic`**, following step 2.2's fixture pattern
  (prior art: the world test's `region_can_fill` and config-key replay).
- **Timing against step 3.0's figures** (`make test` ~85 ms, e2e ~0.16 s):
  - `make test` must stay at that level. The prototype measured 65–81 ms, against 65–75 ms for the
    step 3.0 code in the same session.
  - e2e must stay under 0.5 s. The prototype measured 0.15–0.17 s with the committed claims.

## Out of Scope

- `glitches-no_logic`'s fill divergence (draw 457, map/compass fillable count): row 3.4.
- `state-inverted` and `state-retro` (unsupported worlds), and the inverted and glitch rules
  themselves. This step only fixes the shape they will be written in.
- The `testRegionLockedItems` rows of the Inverted and glitch test trees: owed by rows 3.1, 3.3 and
  3.12.
- The other host-only warn-review items left for the pull request: the `rando_gen.c` statics and
  duplicated file handling, the `spoiler_json.c` scratch buffers, the watchdog defined three times, and
  the `measure.sh` constants.
- `item_placement-basic`'s A-3 overrun (step 2.5 note): unchanged by this step.

## Further Notes

### Hypotheses about step 3.0's outcome, with verdicts

Step 3.0 is now committed (its step note and owed table, the refactored `rando/` and `tests/rando/`).
Each hypothesis was checked against it.

- **H-12-1 — holds.** Step 3.0 changed neither the rule type, nor the region rule files (byte-identical
  to before 3.0), nor the playthrough's world pairs. The map/compass-not-wild test is now
  `rando_dungeon_item_confined(const RandoConfig *, RandoItemClass)`. It is used by the world module,
  the rules header's `wild_or_not_dungeon_item`, the filler and the playthrough's prune. Chunk 2 keeps
  that call as it is.
- **H-12-2 — holds.** `dungeon_items-mc` and `-full` are at `all`, `-mcs` is at `prepareWorld`, and the
  claims file format is unchanged. Overall: 40 claims at `all`, plus `-mcs` and `glitches-no_logic` at
  `prepareWorld`.
- **H-12-3 — holds.** The parallel gate runs 42 claims in ~0.16 s. This step adds no generator run.
- **H-12-4 — fails.** Step 3.0's owed list for row 3.6 is `WorldTest::testSetVanillaDungeonItems` (all
  8 rows, the HybridMajorGlitches and NoLogic ones included) *and* the top-level
  `RegionTest::testRegionLockedItems`. The list wins, so this PRD ports both (see "Owed VT tests").
  This also corrects the first draft's claim that the C config has no `rom.vanilla*` fields: it
  derives all four.
- **H-12-5 — holds.** The playthrough's static state is still there; chunk 2 removes it.
- **H-12-6 — resolved.** Comparisons use step 3.0's figures: ARM code+rodata 79.9 KB, stack peak
  6 280 B, maximum host time per case ~20 ms (`item_placement-basic`), `make test` ~85 ms, e2e ~0.16 s.
- **H-12-7 — holds.** Nothing this PRD names was renamed. Step 3.0 added
  `rando_world_collect_until_stable`, `rando_random_collection` and pool helpers; this step doesn't
  touch them.
- **H-12-8 — holds.** Step 3.0 added and changed no rule, so the list of locations-read sites in
  chunk 1 is complete.

### Step note (`### Step 3.6`) must record

- The rule shape and the read-source rule, one line each.
- The claims: `dungeon_items-mcs` at `all`, and the row's three cases green.
- The VT tests ported, with their row counts (91 + 8).
- e2e time and `make test` time, against step 3.0's.
- `make measure`'s maximum host time per case, library code+rodata (host and ARM proxy) and stack
  peak, against step 3.0's. The prototype's figures (step 3.0 code → prototype, same session) are only
  an expectation and are re-measured:

  | Figure | Step 3.0 code | Prototype |
  |---|---|---|
  | Host code+rodata | 95 359 B | 96 439 B |
  | ARM proxy code+rodata | 79 938 B | 80 354 B |
  | Stack peak | 6 280 B | 6 328 B |
  | Maximum host time per case | 20.2 ms | 19.5 ms (`item_placement-basic`, noise level) |
  | Sweep maximum | 9.0 ms | 6.9 ms (noise level) |

- That `dungeon_items` is ticked.
