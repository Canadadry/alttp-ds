---
title: "No Logic: a world whose regions carry no rules"
description: "Under glitches no_logic the C port still applies every region's access and fill rules, which VT never installs, so glitches-no_logic diverges at fill draw 457 and cannot be claimed at all."
status: needs-triage
---

## Problem Statement

The `glitches: no_logic` row (plan Phase 3, step 4) owes no VT test, but its only case,
`glitches-no_logic`, is still claimed only at `prepareWorld`. At `all` it diverges: the golden
trace has `0 457 fill 0 8 -> 8`, the C port draws `0 457 fill 0 6 -> 4`, and the spoiler first
differs at `Palace of Darkness - Boss:1` (golden `KeyD1:1`, C `PieceOfHeart:1`). Someone who picks
No Logic on the DS would get a seed that alttpr.com would never produce for the same seed and
settings, and the row cannot be closed.

### Proven cause

VT builds a No Logic world without running any region's `initalize()`: `World::__construct` calls
`$region->initalize()` only when `logic !== 'NoLogic'`. Every location requirement, location fill
rule, location always-allow rule, region `can_enter`, region `can_complete`, prize-location
requirement and shop requirement is installed by `initalize()`; none is installed by a region
constructor (checked for every Standard and Open region: the constructors only build locations
and shops, set bosses, clear shop inventories and preset the event and Pyramid Fairy items). So
under No Logic every such rule is absent, and only `Region::canFill` (the base class's
dungeon-item confinement, not part of `initalize()`) still applies.

The C port has no such switch: `rando_region_rules(variant, region)` returns the variant's full
rule table whatever the logic, and every rule lookup in the world module goes through it. Under
No Logic the C port therefore keeps rules VT does not have.

Evidence (scratchpad, not committed):

- Instrumented `RandomAssumed::fillItemsInLocations` (VT, by pre-loading a patched copy of the
  class; the oracle's trace stayed byte-identical to the golden) and `fill_items_in_locations`
  (C) to print each map/compass draw's fillable locations. All dungeon map/compass draws match in
  count until `MapH2`, the draw at index 457: VT has 9 candidates, C 7. The two missing ones are
  `Link's Uncle` and `Secret Passage`, whose fill rules (Open's `HyruleCastleEscape::initalize`)
  refuse a map when maps are not wild. VT never installed those rules.
- The earlier spoiler difference has the same cause and draws nothing: keys are placed at the
  first fillable location, and VT's first fillable location for a Palace of Darkness key is the
  boss, whose rules (requirement, fill and always-allow) the C port still enforces; with the
  prototype fix the key lands there too.
- A VT probe on fresh worlds (Open and Standard, NoLogic vs NoGlitches, empty item collection,
  access checks off for fills): under NoLogic `Link's Uncle` and `Secret Passage` accept `MapH2`,
  `Ganon`, `Ganon's Tower - Moldorm Chest` and `Master Sword Pedestal` are accessible, the
  `Capacity Upgrade` shop is accessible; under NoGlitches all of these are false. In both logics
  `Palace of Darkness - Map Chest` refuses `MapP1`, Eastern Palace refuses `KeyH2` and the win
  condition is false.
- Prototype fix (below) in a scratch copy: `glitches-no_logic`'s trace, `attempts` and
  `spoiler.json` are byte-identical to the golden; with its claim raised to `all`, `make e2e`
  reports 42 pass, 0 divergent, 2 pending; `make test` and `make logic` stay green.

## Solution

A No Logic world behaves as VT's uninitialized regions: no access rule, no fill rule, no
always-allow rule, no region entry or completion rule, no shop requirement. Region dungeon-item
confinement, constructor items, bosses, shops and everything the world structure holds stay as
they are. `glitches-no_logic` is then claimed at `all`, and the row is closed with no VT test owed.

## User Stories

1. As a player choosing No Logic, I want the same item placement alttpr.com gives for my seed and
   settings, so that a seed can be shared and compared with VT's spoiler.
2. As a player choosing No Logic, I want every location to be a candidate for any item that may
   leave its dungeon, so that the seed is as unrestricted as VT's No Logic.
3. As a player choosing No Logic, I want maps, compasses, keys and big keys to stay in their own
   dungeon unless the dungeon-items setting makes them wild, so that No Logic does not change the
   dungeon-item rules VT keeps.
4. As a player choosing No Logic with the Standard state, I want the escape key rule to follow
   VT, so that the Sewers key behaves as VT's No Logic does.
5. As a maintainer, I want `glitches-no_logic` claimed at `all`, so that the e2e gate protects the
   whole No Logic generation, not only its pre-fill stages.
6. As a maintainer, I want every other claim unchanged, so that the fix is proven not to touch
   the logics that do install rules.
7. As a maintainer, I want the No Logic switch in one place in the world module, so that a new
   rule lookup cannot bypass it.
8. As a maintainer, I want a hand-written unit test pinning VT's No Logic behaviour on a fresh
   world, so that a regression is caught by `make test` and not only by one e2e case.
9. As a maintainer, I want the unit test's expected values to come from VT, so that the test
   freezes VT's behaviour and not the port's.
10. As a maintainer, I want the world structure (region classes, location order, shops, bosses)
    unchanged under No Logic, so that the world dump fixtures and the spoiler layout stay valid.
11. As a maintainer, I want the win condition, `collectItems`, the playthrough and `isWinnable`
    to see the same rule-less world, so that the spoiler's playthrough matches VT's too.
12. As a maintainer, I want `make test` and `make e2e` to stay as fast as they are, so that the
    loop stays fast (CLAUDE.md).
13. As a maintainer, I want `make measure`'s figures recorded after the change, so that the DS
    budget is tracked for a logic change.
14. As a maintainer, I want the step note to record that the row owes no VT test and is closed,
    so that the plan's row table is complete.
15. As a DS integrator (roadmap item 2), I want No Logic to need no extra state in `RandoWorld`,
    so that the generator's memory footprint does not grow.

## Implementation Decisions

- **Where the switch lives: the world module's rule lookup.** The world module gets one private
  helper that returns the region's rules, or an all-empty rule set (no `can_enter`, no
  `can_complete`, no location rules, no shop rules) when the world's config logic is No Logic.
  Every rule lookup in the world module (location rules, region entry, region completion, shop
  access) uses that helper instead of calling the variant table directly. This is the C reading
  of VT's "skip `initalize()`": the rules are never consulted, as if never installed.
- **The variant table and its public lookup are unchanged.** `rando_region_rules(variant,
  region)` keeps returning the full table: it is also how `rando_region_vt_class` names a region's
  VT class, which No Logic does not change. No new `RandoVariant` value: No Logic is orthogonal to
  Standard/Open (VT skips `initalize()` for either state).
- **Region dungeon-item confinement is untouched.** `rando_world_region_can_fill` is VT's
  `Region::canFill`, a base-class method that does not depend on `initalize()`; its existing No
  Logic exception for the Standard escape key stays.
- **No change to `RandoWorld`, `RandoConfig`, the filler, the playthrough or the gate.** The
  filler's `can_skip_access_checks`, `rando_dungeon_item_confined` and the junk-fill range already
  handle No Logic as VT does; the divergence came only from the rules.
- **Claim:** `glitches-no_logic prepareWorld` becomes `glitches-no_logic all` in the same commit
  as the fix. Every other claim is unchanged. The prototype gives 42 pass, 0 divergent,
  2 pending (`state-inverted`, `state-retro`).
- **Measurements (prototype, host, noisy):** `make measure` before / after: ARM proxy
  code+rodata 80 354 B / 80 640 B; stack peak max 6 328 B / 6 328 B; `glitches-no_logic`
  ~3.9 ms / ~0.9 ms; max case `item_placement-basic` ~19–22 ms both (step 3.6: 19.8 ms, still over
  A-3, unrelated). The implementer records the figures from their own run in the step note.
- **Plan bookkeeping:** the `### Step 3.4` note records the proven cause in one line, the claim
  at `all`, "owes no VT test (no VT directory)", `make test`/`e2e` times and `make measure`'s max
  time. The Step 3.0 row table is not edited (step notes are the history).
- **Hypotheses about step 3.3 (major_glitches), in flight while this PRD is written:**
  - `H-15-1`: step 3.3 does not change the world module's rule lookup or the signature of
    `rando_region_rules`; if it does, the No Logic helper wraps whatever lookup 3.3 leaves, and
    every lookup must still go through it.
  - `H-15-2`: step 3.3's config changes (`rom.freeItemMenu` accepted by the case reader, its
    derivation) do not alter No Logic's derived config; if they add a glitch flag derived from
    No Logic, the claim still holds because the rules that would read it are absent.
  - `H-15-3`: step 3.3 leaves `glitches-no_logic`'s claim at `prepareWorld` and keeps
    `glitches-major_glitches` at `all`; this step's e2e expectation (42 pass, 0 divergent,
    2 pending) is relative to 3.3's final claim count, not an absolute number.

## Implementation Chunks

One TDD cycle; no split:

1. Red: the hand-written No Logic unit test in `test_world`. Green: the world module's helper and
   its four call sites. Then raise the claim to `all`, run `make test`, `make logic`, `make e2e`,
   `make selftest`, `make measure`; one commit.

## Testing Decisions

- A good test drives the public world API on a fresh world and checks VT-observable behaviour
  (can a location take an item, can it be accessed, can a shop be reached, is the game won), never
  which rule table is consulted.
- **`test_world`, one new hand-written case** (in `make test`, cheap): build a No Logic world for
  Open and for Standard and check, with an empty item collection, the values VT's probe gave:
  `Link's Uncle` and `Secret Passage` accept `MapH2` (access checks off); `Ganon`,
  `Ganon's Tower - Moldorm Chest` and `Master Sword Pedestal` are accessible; the `Capacity
  Upgrade` shop is accessible; `Palace of Darkness - Map Chest` still refuses `MapP1`; Eastern
  Palace still refuses `KeyH2`; the win condition is false. The same Uncle/pedestal checks under
  the default logic are false, which pins that the switch keys on No Logic only. The values came
  from a one-off VT probe (not committed), as step 2.3.5's prize-pack values did.
- The test fails today (Uncle refuses `MapH2`), so it is a true red.
- **e2e:** `glitches-no_logic` at `all` is the end-to-end proof (trace, `attempts`, spoiler
  including the playthrough). No golden is regenerated.
- **Unchanged guards:** `make logic` (NoGlitches and OverworldGlitches rows, world dump) must stay
  green: those fixtures never use No Logic, so they prove the other logics keep their rules.
- Prior art: `check_region_can_fill`, `check_can_fill_and_fill` and the derivation checks in
  `test_world`.

## Out of Scope

- Any other row (inverted, retro, weapons), and step 3.3's major_glitches fixtures.
- Porting No Logic location tests: VT has no `tests/NoLogic` directory.
- Inverted under No Logic (`state-inverted` is still unsupported; when it lands, its regions must
  use the same lookup).
- The `item_placement-basic` time over A-3, and the other open warn-review items.
- Regenerating any golden or changing the oracle.

## Further Notes

- The divergence was invisible to every other case because only No Logic skips `initalize()`;
  hybrid and major glitches install rules (with glitch flags), so their cases were already green.
- Scratchpad artefacts (not committed): the prototype tree, the VT fill instrumentation wrapper
  and the VT probe script.
