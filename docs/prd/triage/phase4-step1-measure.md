---
title: "Phase 4 step 1: warn-review refactor, then the DS budget figures"
description: "Before roadmap item 2 the generator's fit on the DS must be stated per budget assumption, on a library whose duplicated bunny predicate and unchecked slot layout no longer hide code size or location-id bugs."
status: needs-triage
---

## Problem Statement

The C port reproduces every golden, and Phase 4 must now say whether it fits on the DS before
the NDS integration (roadmap item 2) starts. Two things stand in the way.

First, Phase 3's warn-review left warnings that either cost code size or leave location ids
unguarded. The Moon Pearl / bunny revive / OWYBA predicate is copied into seven region files under
three names and two operand orders. Inverted rules live in two places (their own region files, or
a second table in the classic dungeon file) with no stated reason. Most worrying: a location's
rule is found through its layout slot, and three independent sources must agree for that to be
right — the per-variant layout's per-region location order, the layout's `{region, slot}` entry for
each location, and each region file's slot numbering (a local enum in five Inverted overworld
files plus one Inverted Death Mountain file, an `id - FIRST` offset everywhere else). Nothing checks
that they agree; a mismatch silently gives a location another location's rule, and only an e2e
case that happens to exercise it would notice.

Second, the figures that decide roadmap item 2 are spread across step notes, measured on
different trees, and never stated against each budget assumption with the toolchain that produced
them.

## Solution

One refactor commit under the `all` claims, outputs byte-identical (`test`, `logic`, `e2e`
unchanged): one shared copy of the bunny predicate, one stated convention for where Inverted rules
live, and a `make test` check that the layout and the region files agree on every slot.

Then `make -C tests/rando measure` on the final tree, and a table that gives, for each of A-1 to
A-4, the figure, the toolchain that produced it, the margin and a verdict (within, over, or depends
on the stand-in), recorded in the step note before roadmap item 2 starts.

## User Stories

1. As the port maintainer, I want one definition of the Moon Pearl / bunny revive / OWYBA predicate, so that a VT rule change is made once.
2. As the port maintainer, I want the shared predicate to cost no measurable time on `make logic`, so that the canAccess path stays fast.
3. As the port maintainer, I want the refactor to leave every spoiler and trace byte-identical, so that the goldens prove it changed no behavior.
4. As a DS integrator, I want the shipped library's code+rodata measured before and after the refactor, so that I know the refactor did not eat the A-2 margin.
5. As a contributor adding an Inverted region, I want one stated rule for where its rules go, so that I do not have to guess from the existing files.
6. As a contributor, I want that rule to follow VT's class structure, so that reading VT tells me where the C code is.
7. As a contributor, I want the rule written next to the Inverted rule tables' declarations, so that I see it where I add one.
8. As the port maintainer, I want `make test` to fail when a layout's per-region order and a location's `{region, slot}` disagree, so that a hand-edited layout cannot drift.
9. As the port maintainer, I want `make test` to fail when a region file's slot numbering names a different location than the layout puts in that slot, so that a location never runs another location's rule.
10. As the port maintainer, I want `make test` to fail when a region's rule table has a different length than the layout's region, so that a missing or extra slot is caught.
11. As the port maintainer, I want every location the layout gives a region to appear in that region's order exactly once, so that no location is orphaned.
12. As the port maintainer, I want that check to cover every variant (Standard, Open, Inverted, Retro), so that variants sharing a layout are covered too.
13. As the port maintainer, I want the check to cost no code or rodata in the shipped library, so that A-2 is unaffected.
14. As the port maintainer, I want the check to keep `make test` as fast as before, so that the loop stays fast.
15. As the port maintainer, I want the enum-numbered region files to derive both their slot enum and the ids the test reads from one list, so that the two cannot disagree.
16. As the port maintainer, I want the `id - FIRST` region files to declare their first id and count to the test, so that the offset assumption is checked against the layout.
17. As a reviewer, I want the warnings left for the pull request (Lake Hylia special case, retro constants, state-to-variant mapping, the support header comment) untouched, so that the commit stays one refactor.
18. As the plan owner, I want `make -C tests/rando measure` run on the final tree, so that the recorded figures describe what roadmap item 2 will integrate.
19. As the plan owner, I want A-1 (stack peak ≤ 8 KiB) stated with its figure, toolchain, margin and verdict, so that the DTCM stack decision is grounded.
20. As the plan owner, I want A-2 (code+rodata ≤ 128 KiB, state ≤ 32 KiB, no writable static) stated the same way, per toolchain, so that I know which figure is a stand-in.
21. As the plan owner, I want A-3 (≤ 2 s per generation on the DS, host ×150) stated with the per-case max, the sweep max and the median, so that the known `item_placement-basic` overrun is in the table.
22. As the plan owner, I want A-4 (no separate attempt budget) stated with the attempts observed in the goldens and the sweep, so that retries are visibly folded into A-3.
23. As the plan owner, I want a verdict of "depends on the stand-in" wherever the clang ARMv5TE proxy produced the figure, so that roadmap item 2 rechecks it with devkitARM first.
24. As the plan owner, I want the raw measure output kept with the table, so that every figure can be traced.

## Implementation Decisions

- **Shared predicate.** One out-of-line function in the world module, declared in the internal
  rules header, named for what it tests (Moon Pearl, or bunny revive when the config allows it, or
  OWYBA when the config allows it). It replaces the seven file-local copies, including the
  Inverted escape's `inverted_link_state`, and the inline copies of the same three-operand
  disjunction in other rules. Every operand is a pure predicate on items and config, so the
  differing operand orders of the copies are equivalent; one order is kept. A `static inline`
  version was tried first: it grew the clang ARM code by 148 B and host -O2 by 1.3 KB, while the
  out-of-line one saves 3.6 KB on ARM with `make logic`, `test` and `e2e` medians unchanged (the
  operands already call out-of-line item helpers). The dungeon "not bunny" predicates that add
  clip conditions (Ganon's Tower's) are different rules and are not merged.
- **Inverted rule placement: keep the split, state it as the convention.** VT's Inverted classes
  all extend a classic class, but in two ways: those whose `initalize` calls `parent::initalize()`
  re-set a few rules and inherit the rest (the twelve dungeons and the escape); those that do not
  rewrite every rule and often change the location list (the Inverted overworld regions, Mire,
  Turtle Rock). The C files already follow exactly that line: the first kind is a second table in
  the classic file, reusing its file-static rules; the second kind has its own Inverted file.
  Moving the first kind out would turn shared statics into external functions, losing inlining and
  adding call overhead on the canAccess path. So nothing moves; the convention is written once,
  briefly, as a trap comment above the Inverted rule-table declarations in the internal rules
  header.
- **Slot agreement check.** The rule tables are indexed by the layout slot, so the test needs to
  know which location each region file means by each slot. A test-only slot description is added
  to the region rules struct, present only under `RANDO_FAULTS` (the test build's flag; the
  shipped and measured library has no such field and no extra rodata):
  - `id - FIRST` files declare their first location id and slot count;
  - enum files turn their slot enum into one X-macro list that expands both into the enum and,
    under `RANDO_FAULTS`, into the location-id array the test reads (the enum names already match
    the `RANDO_LOC_` names).
  A helper macro in the internal rules header appends the slot description to each table's
  positional initializer and expands to nothing outside `RANDO_FAULTS`.
- **Test interface.** Under `RANDO_FAULTS`, the public world header gains two read-only queries:
  the slot count a variant's region rule table declares (0 for a region with no location table),
  and the location id a region rule table means by a slot; plus a layout query for a location's
  slot. Existing public layout queries give the per-region order and a location's region.
- **Canonical source.** The layout stays the source of truth for VT's order; region files must
  agree with it, not the reverse.
- **Ship build unchanged by the check.** The ship build compiles without `RANDO_FAULTS`, so only the
  predicate change can move `make measure`'s code+rodata; before/after figures are recorded.
- **Measurement (chunk 2).** `make -C tests/rando measure` on the final tree, unchanged. The table
  per assumption: A-1 from `stack.peak.budget-basis` (host peak scaled by the clang ARM frame
  ratio); A-2 from `lib.code+rodata` per toolchain (host -O2, host -Os, clang ARMv5TE) and
  `sizeof.RandoGen`, writable and bss; A-3 from the per-case max and sweep max ×150; A-4 from the
  golden and sweep attempt counts. Toolchain column: "clang ARMv5TE stand-in" or "host". Verdict:
  within, over, or depends on the stand-in (an ARM figure that devkitARM may move across the
  budget, or a host-time projection). The step note itself is written by the plan owner.

## Implementation Chunks

1. **Phase 3 warn-review refactor** (one commit): shared predicate; the Inverted placement
   convention comment; the `RANDO_FAULTS`-only slot descriptions and queries; the layout
   agreement test in the world unit test. Proof: `test`, `logic`, `selftest`, `e2e` (44 pass,
   0 divergent, 0 pending), `make measure` code+rodata before/after, `make test` and `e2e` A/B
   timed against a worktree of the previous commit.
2. **Measure**: `make -C tests/rando measure` on the final tree and the A-1..A-4 table, handed to
   the plan owner for the step note.

## Testing Decisions

- A good test here checks the external contract: for every variant and region, the layout's
  order, its `{region, slot}` entries and the rule table's declared slots name the same locations
  in the same order. It does not test how a rule is written.
- The agreement test lives in the world unit test, which already checks layout queries; it runs in
  `make test` (no fixture mode needed). It loops over 4 variants × 27 regions × at most 28 slots:
  negligible time.
- Red first: the test is written against the new queries before the slot descriptions exist, then
  proven able to fail by a deliberately wrong slot (reverted).
- The predicate merge has no new unit test: it changes no behavior, and the existing location
  fixtures (`make logic`, every Inverted, OWG and MG row) plus the 44 goldens are the proof.
- Prior art: the layout checks already in the world unit test; the `RANDO_FAULTS` hooks in the
  public API.

## Out of Scope

- `prepare.c`'s Lake Hylia special case and retro constants, `generate.c`'s state-to-variant
  mapping, the support header comment: left open for the pull request.
- Phase 2's host-only warnings (`rando_gen.c`, `spoiler_json.c`, watchdog, `measure.sh` constants).
- Shop tables' `FIRST_SHOP` offsets (no warning names them).
- Making `item_placement-basic` faster: the measurement reports the overrun; roadmap item 2
  rechecks it.
- Writing the step note (the plan owner writes it from the table).

## Further Notes

- devkitARM is absent in this environment; every ARM figure comes from the clang ARMv5TE proxy,
  whose asserts are compiled out, so its code size is a lower bound.
