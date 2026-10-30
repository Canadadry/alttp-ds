---
title: "Inverted: VT's second world, its regions and its tests"
description: "The C generator returns RANDO_UNSUPPORTED for state inverted before world init, so state-inverted is pending and VT's three Inverted test trees (122 methods) are not ported."
status: needs-triage
---

## Problem Statement

The `state: inverted` row (plan Phase 3, step 12) is the last world the C port does not have. A
player who picks Inverted on the DS would get no seed: `rando_generate` stops with
`RANDO_UNSUPPORTED` right after the crystal draws, before world init. The `state-inverted` case
is the only pending case besides `state-retro`, and the row owes VT's three Inverted test trees
(step 3.0 note): `tests/Inverted`, `tests/InvertedOverworldGlitches`,
`tests/InvertedMajorGlitches`, 22 files each, 43, 38 and 40 methods.

Inverted is not a variant of the rules only. VT builds it as a second World subclass with its own
region classes, and its world structure differs from Standard/Open's:

- **Every one of the 24 game regions has an Inverted class** (`Region\Inverted\…`); Medallions and
  Fountains are Standard's. Each Inverted class extends its Standard parent (Escape extends Open's).
  Twelve classes (Escape, Eastern, Desert, Hera, Castle Tower, PoD, Swamp, Skull Woods,
  Thieves' Town, Ice, Misery Mire, Ganon's Tower) call `parent::initalize()` and then replace a few
  rules; the other twelve (the 11 overworld regions, `DarkWorld\Mire` included, and Turtle Rock)
  rewrite every rule without calling it. 210 rule installs in all (Standard and Open: 286).
- **Locations move between regions, and one is added.** Probe of VT at the pinned commit (scratchpad
  script, not committed; `World::factory('inverted')` against `'open'`): 237 locations, not 236.
  `Ganon` leaves North East Dark World and is appended to North East Light World (which becomes its
  prize region); `Link's House` leaves South Light World and is appended to South Dark World;
  `Ether Tablet` and `Spectacle Rock` leave West Death Mountain and are appended to East Death
  Mountain; a new `Prize\Event` location `Bomb Merchant`, holding `BigRedBomb` from its constructor,
  is appended to South Light World. `World::getLocations()` order changes accordingly, and every
  RNG consumer that walks it (prize placement's `randomCollection(15)` — 65 `fillPrizes` draws for
  `state-inverted` against baseline's 64 — the fill's candidate lists, the playthrough) depends on
  that order.
- **Shops:** the same 41 shops in the same regions — the Inverted West Dark World Death Mountain
  constructor's `shops->removeItem("Dark Death Mountain Fairy")` is a no-op, because
  `Collection::removeItem` compares objects and is given a string. Only the Dark World Lake Hylia
  Shop's default inventory differs: slot 0 is `BluePotion` at 160 (Standard: `RedPotion` at 150);
  the golden's `Shops` section shows it.
- **Two VT quirks the port keeps:** North East Dark World's prize location is still the removed
  `Ganon` object (the dump shows `Ganon` as the prize of both North East regions; nothing in the
  generator reads it); Inverted's Ganon requirement has no `ganonhunt` check.
- **Two config keys only Inverted reads:** a bare `cantTakeDamage` (not `region.cantTakeDamage`,
  one rule in Inverted North West Light World) and `turtlerock.wastekey` (Inverted Turtle Rock).
  No config, preset or VT test sets either: both read as false.

Everything else Inverted-specific in VT's generator is already ported: `canFly`'s Inverted branch
(`ItemCollection::canActivateOcarina`), `getGanonsTowerJunkFillRange`'s Inverted exception, and
the Lake Hylia shop exclusion in `setShops`' retro path (step 3.13). The remaining Inverted
branches (`World.php` 1074 and 1250, `InitialSram`, `EntranceRandomizer`) are ROM-only or out of
scope.

The C port cannot express this today: `RandoLocationId` is one enum in Standard's
`getLocations()` order, each `RandoRegionInfo` owns a contiguous id range (`first_location`,
`nlocations`) and one prize location, `RandoLocationInfo` stores one region per location, region
rule tables are indexed by `location - first_location`, and the library and the host walk
locations as `0 … RANDO_LOCATION_COUNT`.

## Solution

The C port gets VT's Inverted world: a per-variant world layout (which locations a world has, in
which order, in which region), the Inverted region classes as rule tables (sharing their Standard
parent's rules where VT inherits them), world init and generation for the inverted variant. VT's
three Inverted test trees are extracted into `make logic` fixtures and replayed; `state-inverted`
is claimed at `all`. Standard, Open and every other case behave byte for byte as before.

## User Stories

1. As a player choosing Inverted, I want a seed generated on the DS, so that the Inverted mode is
   playable without a PC tool.
2. As a player choosing Inverted, I want the same item placement alttpr.com gives for my seed and
   settings, so that I can compare my spoiler with VT's.
3. As a player choosing Inverted, I want Link's House treated as a Dark World location, so that its
   item is placed with Dark World logic, as VT does.
4. As a player choosing Inverted, I want Ganon fought from the Light World, so that the win
   condition follows VT's Inverted rules.
5. As a player choosing Inverted, I want the Bomb Merchant to hold the Big Red Bomb, so that the
   Pyramid Fairy logic sees the bomb where VT puts it.
6. As a player choosing Inverted, I want the Dark World Lake Hylia Shop to sell a Blue Potion at
   160 rupees, so that the shop matches VT's.
7. As a player choosing Inverted with a glitched logic, I want the Inverted glitch rules VT has, so
   that a future glitched Inverted seed is VT's (the three test trees pin those rules now).
8. As a player choosing Standard or Open, I want my seeds unchanged, so that adding a world breaks
   nothing I already have.
9. As a maintainer, I want `state-inverted` claimed at `all`, so that the e2e gate protects the
   whole Inverted generation, playthrough included.
10. As a maintainer, I want every other claim unchanged, so that the second world is proven not to
    leak into the first.
11. As a maintainer, I want VT's Inverted world structure (regions, VT classes, prize locations,
    locations in `getLocations()` order, their regions, classes and constructor items) in the world
    dump, so that the C layout is checked record by record against VT.
12. As a maintainer, I want every row of `tests/Inverted`, `tests/InvertedOverworldGlitches` and
    `tests/InvertedMajorGlitches` replayed by `make logic`, so that every Inverted rule is pinned by
    VT's own tests.
13. As a maintainer, I want each extracted row replayed by VT from its record alone before it is
    written, so that a fixture row cannot disagree with VT.
14. As a maintainer, I want the extractor to report any row where VT disagrees with its own test, so
    that a VT test bug is visible and not silently ported.
15. As a maintainer, I want `testDungeonItemWrite` skipped as ROM-only, as in the other trees, so
    that no ROM code enters the fixtures.
16. As a maintainer, I want `canFly`'s Inverted branch checked against VT's predicate values, so
    that the one Inverted predicate is pinned by a fixture, not only by e2e.
17. As a maintainer, I want an Inverted `collectItems` scenario in the fixtures, so that the
    per-variant location walk (Bomb Merchant collected, Link's House in the Dark World) is pinned
    outside e2e.
18. As a maintainer, I want one place that says which locations a world has and in which order, so
    that no loop can walk a location the world does not have (the Bomb Merchant in a Standard world
    would otherwise become an empty fill candidate and shift every Standard golden).
19. As a maintainer, I want a location's region to depend on the world's variant, so that rules,
    region entry, region fill confinement, the spoiler section and the playthrough region all
    follow VT's Inverted moves.
20. As a maintainer, I want the layout refactor committed alone with every test, fixture and golden
    unchanged, so that a regression from the refactor is told apart from an Inverted bug.
21. As a maintainer, I want an Inverted region class that inherits its parent's rules to reuse the
    C Standard rules, so that a rule VT shares is written once.
22. As a maintainer, I want an Inverted class that rewrites its rules to get its own region file, so
    that the Standard files keep their own logic readable.
23. As a maintainer, I want an unported Inverted region to fail loudly rather than fall back to
    Standard's rules, so that a half-ported world never answers silently with the wrong world's
    logic.
24. As a maintainer, I want the region port split into chunks whose replayed rows only reach ported
    regions, so that each chunk is a true red-green cycle.
25. As a maintainer, I want `state-inverted` claimed at `setMedallions` as soon as the world exists,
    so that the layout, the shop override and world init are proven against VT's trace before any
    rule is ported.
26. As a maintainer, I want `make test` to stay as fast as it is, with only hand-written cases
    added, so that the loop stays fast (CLAUDE.md).
27. As a maintainer, I want `make e2e` under 0.5 s with the new claim, so that the gate's budget
    holds.
28. As a maintainer, I want `make measure`'s code size recorded against A-2's 128 KiB, so that the
    second region set is weighed against the DS budget before retro and weapons add theirs.
29. As a maintainer, I want the step note to record the rows replayed per tree and the methods
    ported, so that the step 3.0 owed table is closed with numbers.
30. As a DS integrator (roadmap item 2), I want the world layout as const data with no writable
    static, so that A-2's "no writable static in the library" still holds.
31. As a DS integrator, I want `RandoWorld` to grow by only the one new location, so that the
    state budget (A-2: 32 KiB) is unaffected.
32. As a future implementer of `state: retro` (step 3.13), I want the variant layout to be the only
    per-world structure, so that Retro (an Open subclass in VT) can reuse the classic layout.
33. As a future implementer, I want No Logic to apply to Inverted worlds through the same rule
    lookup step 3.4 adds, so that No Logic Inverted needs no extra work.

## Implementation Decisions

### World layout (the core decision)

- **Location ids stay global; membership, order and region become per-variant.** One new id,
  `RANDO_LOC_BOMB_MERCHANT`, is appended after `RANDO_LOC_PYRAMID_BOTTLE`, so every existing id
  and Standard's order are unchanged and `RANDO_LOCATION_COUNT` becomes 237 (still under the 256
  the `uint8_t` id assertions require). The enum's comment says "Standard's `getLocations()` order,
  then the locations only Inverted has". Rejected: a second enum, or ids meaning different
  locations per variant — every rule names locations by constant.
- **A layout per world shape, as const data:** the *classic* layout (Standard and Open; Retro will
  reuse it) and the *inverted* layout. A layout holds: the world's locations in VT's
  `getLocations()` order; per region, its locations in VT order (as a span of that order) and its
  prize location; per location id, its region (`RANDO_REGION_NONE` when the world does not have
  it) and its slot (its index in its region's list). Region and slot lookups are O(1) const table
  reads: they sit on the `canAccess` hot path. No writable static (A-2).
- **What moves out of the static tables:** `RandoRegionInfo` loses `first_location`,
  `nlocations` and `prize_location`; `RandoLocationInfo` loses `region`. Removing the fields (not
  keeping them as "the classic values") makes the compiler find every caller that must become
  variant-aware. `RandoLocationInfo` keeps its name, class and constructor item (`Bomb Merchant`'s
  `BigRedBomb`, used only by worlds that have it). Shops keep `first_shop`/`nshops` and their
  region: shops are identical in both layouts.
- **Public interface (rando_world.h)**, all asserting their arguments are in range:
  - the number of locations of a variant's world, and its i-th location in VT order;
  - a location's region in a variant (`RANDO_REGION_NONE` if absent);
  - a region's location count, i-th location, and prize location in a variant.
  `rando_location_find` stays global (a name maps to one id whatever the variant). A world query
  on a location its world does not have asserts.
- **Rule tables are indexed by slot**, the location's index in its region's list for the world's
  variant (for the classic layout, exactly today's `location - first_location`, so no Standard
  table changes). The slot lookup is private to the world module and the region files (the
  internal rules header).
- **Every walk over locations goes through the layout**, in VT order: world init's constructor
  items, the collectable cache, `collectItems`' search, the collected count and reset, prize
  placement's candidate list, the medallion and fountain regions, the fill's empty-location lists
  and Ganon's Tower junk fill, the host spoiler's region sections, and the host playthrough's
  sphere walk and entry regions. No library or host loop runs `0 … RANDO_LOCATION_COUNT` over
  locations any more (arrays stay sized by it).
- **3.4's rule lookup is kept as the single entry point** (see `H-16-1`): the variant dispatch and
  the No Logic switch stay in the one private helper; the slot lookup is applied to what it returns.

### Inverted world

- `RandoVariant` gains `RANDO_VARIANT_INVERTED`. World init derives `mode_state` from it
  (`RANDO_STATE_INVERTED`), as it does for Standard and Open (VT's `World::factory` sets
  `mode.state` to the type).
- World init applies the inverted layout's constructor items (Ganon's `DefeatGanon`, the Bomb
  Merchant's `BigRedBomb`) and one per-variant shop default override: the Dark World Lake Hylia
  Shop's slot 0 is `BluePotion` at 160 under Inverted, written as a small const override table read
  by world init, not as a second shop table.
- The bare `cantTakeDamage` and `turtlerock.wastekey` are not added to `RandoConfig`: the two Inverted
  rules that read them take VT's default (false) as a constant, each with a one-line comment
  naming the VT key (a trap: a reader would otherwise "fix" it to `region_cant_take_damage`). The
  extractor lists both keys in its config keys, so a fixture row that ever overrides one fails the C
  fixture reader on an unknown key.
- The generator maps `state: inverted` to the inverted variant; `retro` stays
  `RANDO_UNSUPPORTED` (step 3.13).

### Regions: how the code is shared with Standard/Open

- **Rule: an Inverted class that calls `parent::initalize()` lives in its parent's C file**, as a
  second `RandoRegionRules` table in that file. Its location rules array copies the parent's
  entries and replaces only the requirements VT re-sets; a `setRequirements` replaces the
  requirement only, so the parent's fill and always-allow rules stay on that entry, exactly as in
  VT. Its `can_enter`/`can_complete` are the parent's unless VT re-assigns them; where VT re-sets the
  prize's requirement to a new `can_complete` (Ganon's Tower, Turtle Rock), the prize entry uses
  the new one. This reuses the parent's file-local rule functions without exporting them. Applies to
  Escape (in the Open escape file), Eastern, Desert, Hera, Castle Tower, PoD, Swamp, Skull Woods,
  Thieves' Town, Ice, Misery Mire and Ganon's Tower.
- **An Inverted class that does not call it gets its own file**: the 11 overworld regions (Mire
  included) and Turtle Rock. VT duplicates their logic; so does the port. No overworld rule
  function is shared across files.
- New region files are flat beside the existing ones, named with an `inverted_` prefix: the test
  Makefile's library wildcard covers exactly one directory level below the library sources (a
  nested directory would silently drop out of the host build).
- VT class names follow VT (`Region\Inverted\LightWorld\NorthEast`, …), so the world dump checks
  them; Medallions and Fountains keep `Region\Standard\…` in the inverted table.
- **Transliteration, not simplification**: every glitch-flag branch is ported as VT writes it
  (rows of the OverworldGlitches and MajorGlitches trees pin them); helper closures (`$main`,
  `$side`, `$thieves`, `$mire`, `$hera`) and Turtle Rock's `canReach*`/`enter*` methods become
  file-local functions. Nested reads follow step 3.6's two-world contract: `$locations[...]`
  reads the rule's `locations` world, `$this->locations[...]` and nested `canAccess` the rule's own
  world.
- **An unported Inverted slot is NULL and `rando_region_rules` asserts on it** with a message
  naming the region. No fallback to Standard. The assert goes when the last region chunk fills the
  table.

### Oracle and fixtures

- The extractor adds `inverted` to its world-dump variants (region and location records, same
  format), adds the three trees to its row-tree table with their own fixture files (one per tree,
  named after it), adds `testComplete` to its method kinds (Castle Tower, region-scoped, `complete`
  query, collected items), keeps `testDungeonItemWrite` skipped, adds an `inverted` predicate
  vector config (the test config on an Inverted world) so `canFly(w)` is checked on its Inverted
  branch, and adds one Inverted `collectItems` scenario whose steps collect the Bomb Merchant and
  Link's House. It fails, writing nothing, on an unknown method or a row whose record does not
  replay, as today.
- The row record format is unchanged: the variant field reads `inverted` (it comes from
  `mode.state`), config overrides and the logic-before-world-init rule are as in the other trees.
- `gen_golden.sh` writes the new files; every golden and every existing fixture line is
  byte-identical, except the records the change adds to the world dump, the predicate vectors and
  the NoGlitches row file (its collect section).
- Expected row counts: provider rows are roughly 3 600 / 1 450 / 2 250 per tree (a grep, not the
  extractor's count); the step note records the extractor's figures. The three files add roughly
  2 MB of committed text, like the existing trees.

### Gate, claims, measurements

- After the world chunk, `state-inverted` is claimed at `setMedallions` (getAdvancementItems,
  setShops' `Shops` section with the Blue Potion, both medallions — no stage before `fillPrizes`
  evaluates a rule). At the end it is raised to `all`. Expected final report (relative to `H-16-2`):
  43 pass, 0 divergent, 1 pending (`state-retro`).
- `make e2e` stays under 0.5 s (one more parallel claim, ~4 ms of generator). `make test` gains
  only hand-written cases. `make logic` grows from ~0.23 s by about the rows added (~0.5 s
  expected); it is not part of `make test`.
- **Size:** the Standard/Open regions are ~37.5 KB of ARM proxy code (clang, ARMv5TE -O2); the
  Inverted classes install 210 rules against 286, so +25–32 KB is expected, putting the library
  at ~105–113 KB against A-2's 128 KiB. The step note records `make measure`'s ARM code+rodata,
  stack peak (fill arrays grow by one entry), `RandoWorld`/`RandoGen` sizes (+3 bytes of location
  arrays, alignment aside) and the max case time, against step 3.4's figures. Over 128 KiB is not a
  claim blocker: it is recorded in the step note as an open A-2 warning for Phase 4.
- **Plan bookkeeping:** a `### Step 3.12` note records: the layout decision in one line, the
  claim, the extractor's row counts per tree and methods ported (42, 38, 39: `testDungeonItemWrite`
  is ROM-only in two trees), "VT agrees with every test" or the disagreements, the VT quirks kept
  (shop removal no-op, orphan Ganon prize, no ganonhunt check, the two constant-false keys), `make
  test`/`logic`/`e2e` times and `make measure`'s figures. The 3.0 table is not edited.

### Hypotheses about step 3.4 (no_logic), in flight while this PRD is written

- `H-16-1`: step 3.4 lands as its PRD says — one private helper in the world module returns the
  region's rules, or an empty set under No Logic, and every rule lookup (location rules, region
  entry and completion, shop access) uses it. This step's slot lookup is applied to that helper's
  result and the variant dispatch stays inside it. If 3.4 landed another shape, the layout chunk
  routes every lookup through whatever single entry point 3.4 left, and adds one if there is none.
- `H-16-2`: step 3.4 claims `glitches-no_logic` at `all` and leaves `state-inverted` and
  `state-retro` pending: 42 pass, 2 pending before this step. This step's e2e expectations are
  relative to 3.4's final claim count, not absolute.
- `H-16-3`: step 3.4 changes neither the static world tables nor the extractor; its new
  hand-written No Logic case uses only public world queries (no `first_location` or location
  region field), so the layout refactor does not touch it. If it does read a removed field, the
  layout chunk moves it to the layout accessors like any other caller.
- `H-16-4`: step 3.4's note records `make measure` figures (prototype: ARM 80 640 B, stack peak
  6 328 B, max case ~19–22 ms); this step compares against them, or against step 3.6's (80.4 KB,
  6 328 B, 19.8 ms) if 3.4 recorded none.

## Implementation Chunks

In dependency order; each ends with `make test`, `make logic`, `make e2e` green and one commit.

1. **Oracle (test commit).** Extractor: `inverted` world dump records, the three row trees and
   their fixture files, `testComplete`, the two config keys, the `inverted` predicate vector config,
   the Inverted collect scenario; `gen_golden.sh` run; goldens and existing fixture lines
   byte-identical (a `--fresh --out` run compared). C side, only to stay green: the world-dump,
   predicate and collect replays skip `inverted` records by name (one named skip each, removed by
   chunk 3); the new row files are not read yet. Red/green: none (fixtures only); the extractor's
   self-replay is the check.
2. **Layout refactor (refactor commit, outputs byte-identical).** The classic layout, the public
   layout accessors, the slot lookup, the removed fields, every location walk through the layout
   (library and host), the world-dump check per variant (counts from the layout, spans per region
   per layout). Standard/Open unchanged: `test`, `logic`, `e2e`, `selftest` identical, `make
   measure` within noise. Test first: a hand-written `test_world` case pinning the classic layout
   through the new accessors (count 236, Link's House in South Light World, Ganon the prize of
   North East Dark World, Ether Tablet in West Death Mountain).
3. **Inverted world.** `RANDO_VARIANT_INVERTED`, `RANDO_LOC_BOMB_MERCHANT`, the inverted layout,
   the shop override, `mode_state`, the inverted variant table with every game-region slot NULL
   except Medallions and Fountains (asserting lookup), the generator's state mapping. Removes the
   chunk-1 skips: the world dump's inverted records, the `canFly` vectors and the collect scenario
   replay. `test_generate`: the getAdvancementItems lists of `state-inverted` replayed (its skip
   removed for those lists only), `check_states` generates Inverted up to `setMedallions` and
   matches the golden prefix. Claim `state-inverted setMedallions`. Test first: hand-written
   `test_world` cases on a fresh Inverted world (237 locations; Link's House in South Dark World;
   Ganon in, and prize of, North East Light World; Bomb Merchant holds `BigRedBomb` and a Standard
   world has no Bomb Merchant; Ether Tablet and Spectacle Rock in East Death Mountain; the Lake
   Hylia shop's Blue Potion at 160; `mode_state` inverted).
4. **Overworld and dungeon entries** (as step 2.2's first region chunk). Every Inverted overworld
   region (11 files) with its shop requirements, the Inverted escape, every Inverted dungeon's
   `can_enter`, and the five dungeons whose only override is `can_enter` whole (Castle Tower, PoD,
   Thieves' Town, Ice, Misery Mire): their tables point at the parent's location rules. Fill the
   remaining dungeon slots with their parent-rules tables plus the Inverted `can_enter` (Eastern,
   Desert, Hera, Swamp, Skull Woods, Turtle Rock, Ganon's Tower), so no slot is NULL and every
   region can be entered. `test_regions` reads the three Inverted row files through a
   `(tree, file)` enabled list: every overworld file, `HyruleCastleEscapeTest`,
   `HyruleCastleTowerTest`, `PalaceOfDarknessTest`, `ThievesTownTest`, `IcePalaceTest`,
   `MiseryMireTest`, in all three trees, plus the entry methods (`testEntry`,
   `testCanEnterWithEverything`) everywhere. The rows of an enabled file only reach regions whose
   entry rules are ported: the overworld reaches Desert, Thieves' Town and Ganon's Tower through
   their entries only (Desert Ledge, Ganon under glitch flags). Test first: those rows are red.
5. **Light World dungeons, Swamp and Skull Woods.** Eastern, Desert (with `$main`/`$side`/
   `$thieves`), Hera (`$main`/`$mire`), Swamp (`$mire`/`$hera`), Skull Woods; their files enabled
   in all three trees.
6. **Turtle Rock.** Its own file (no parent `initalize`): `canReach*`, `enter*`, the
   wasted-key choice (constant false), `can_complete` as the prize's requirement; its file enabled.
7. **Ganon's Tower, and every row replayed.** The 34 overrides, `can_complete` re-set on the prize;
   the enabled list and the NULL-slot assert removed; every row of the three trees replayed.
8. **Generator claim.** `state-inverted` raised to `all`; the prepared-pools skip for
   `state-inverted` removed; `make test`, `logic`, `e2e`, `selftest`, `measure`; the step note. A
   divergence found here is fixed in this chunk (trace first divergence, then the rule or walk that
   explains it), never by regenerating a golden.

Size estimates (lines of C, tests included): 1 ≈ 120 PHP + 30 C (+~2 MB fixtures); 2 ≈ 400–550
touched; 3 ≈ 450 (the inverted layout tables are most of it); 4 ≈ 1 300 (like step 2.2's 1 674);
5 ≈ 450; 6 ≈ 250; 7 ≈ 400; 8 ≈ 30 plus the note.

## Testing Decisions

- A good test drives the public world API (or the generator) and checks VT-observable behaviour —
  which locations a world has, where, what a location accepts or can be reached with, what the
  generator draws and places — never which table or slot holds a rule.
- **VT's tests are the fixtures**: the three Inverted trees replayed row by row by `test_regions
  --fixtures` (`make logic`), with VT's answer as the expectation; the world dump's inverted records
  by `test_world --fixtures`; the `inverted` vectors by `test_items`/`test_bosses --fixtures`; the
  Inverted collect scenario by `test_world --fixtures`. They are not in `make test`.
- **Hand-written cases (in `make test`, cheap):** `test_world` pins the classic layout (chunk 2)
  and the inverted layout, constructor items, shop override and `mode_state` (chunk 3), so that a
  broken layout is caught by `make test` and not only by `make logic`; `test_regions` gets one
  Inverted entry check per region chunk mirroring the step 2.2 cases (e.g. Inverted
  `TowerOfHeraTest::testCanEnterWithEverything`); `test_generate`'s `check_states` generates
  Inverted to `setMedallions` (chunk 3) and stops treating it as unsupported.
- **e2e:** `state-inverted` at `setMedallions` (chunk 3), then at `all` (chunk 8): trace, attempts
  and spoiler including the playthrough. Every other claim must stay green after every chunk — the
  layout chunk is proven by that alone. No golden is regenerated.
- **Order-sensitive behaviour is proven by e2e only:** the Inverted `getLocations()` order changes
  draws (65 `fillPrizes` draws); no hand-written test pins draw counts.
- Prior art: step 2.2's region chunks and their `enabled_files`/`enabled_methods` filter in
  `test_regions`; steps 3.1 and 3.3's oracle-then-replay pairs; `test_generate`'s named
  `skipped_cases`; `test_world`'s world-dump record checks and `check_region_spans`.

## Out of Scope

- `state: retro` (step 3.13), even though Retro will reuse the classic layout.
- Inverted combined with other non-default options (interactions are the plan's second pass). No
  Logic Inverted gets VT's rule-less world through 3.4's helper for free, but no case tests it.
- Entrance shuffle and the Inverted ER patches (`World.php` 1074), ROM writes
  (`setGameState`, `InitialSram`, `setLockAgahnimDoorInEscape`), texts and hints.
- `testDungeonItemWrite` (ROM-only) in `tests/Inverted` and `tests/InvertedMajorGlitches`.
- Reducing code size if A-2 is exceeded: recorded, not fixed here.
- The `item_placement-basic` time over A-3 and the other open warn-review items.

## Further Notes

- Scratchpad artefacts (not committed): the VT structure probe comparing `open` and `inverted`
  worlds, and an ARM proxy size probe of the current region objects.
- VT's Inverted Desert test files build a second, Standard `world_free_items` world in `setUp`;
  it is used only by `testDungeonItemWrite`, which is skipped, so the extractor's captures (taken
  from `$this->world`) are unaffected.
- The dump's two `Ganon` prize records (North East Light and Dark World) are VT's state, not an
  extractor bug: Inverted North East Dark World removes `Ganon` from its locations but keeps the
  object as its prize location.
