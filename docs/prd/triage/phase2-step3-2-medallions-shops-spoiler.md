---
title: "Rando setShops (baseline path), setMedallions and the first partial spoiler.json"
description: "After step 2.3.1 the C generator stops after getAdvancementItems and writes no spoiler, so no claim can reach setMedallions and the gate's section/keys checks have nothing to read."
status: needs-triage
---

## Problem Statement

After Phase 2 step 3.1, the C generator replays VT's draws up to `getAdvancementItems` and stops there. The next two stages in VT's order are `setShops` and `setMedallions`, and the C port has neither:

- **`setShops`** draws nothing in every supported matrix case, but it still changes the world. It activates every shop that is not a Take Any. It clears `Capacity Upgrade` under `shops.HardMode` (item pools `hard` and `expert`). It replaces shields with `TwentyRupees2` in five Dark World shops when `item.overflow.count.Shield` is below 2 or below 1 (`expert`, `crowd_control`). The C world has no notion of an active shop, no shop prices and no shop kind, so none of this can be stored, and nothing can write VT's `Shops` spoiler section.
- **`setMedallions`** makes two draws, one per medallion location, in the Medallions region's order (Turtle Rock first, then Misery Mire). Each draw indexes `[Ether, Bombos, Quake]`. Every claim in this step stops before it.
- **No spoiler output.** The convergence gate (step 2.1 note) checks the spoiler entries a stage determines with the `section` and `keys` checks of `stages.txt`, run on the C `spoiler.json`. The C generator writes no `spoiler.json` at all, and `stages.txt` has no `check` line yet. A stage claim at `setMedallions` would compare only the trace, and a correct trace can still go with wrong shops (all of `setShops` draws nothing) or swapped medallions (baseline draws `0, 0`, which gives Ether for both locations, so the order is invisible there).

The traps are in what the trace cannot see. Baseline's two medallion draws are equal. `setShops` makes no draw, so a missed HardMode clear or shield replacement leaves the trace untouched. And the spoiler's text must match VT's normalized pretty-printed JSON exactly, byte for byte, for every entry the gate compares.

## Solution

1. **Shops in the world.** Each shop gets VT's kind (Shop, Take Any, Upgrade) and default prices in the shop table. Each world gets per-shop active flags (all inactive at init, as VT's constructor leaves them) and per-slot prices. World operations mirror VT's `Shop`: `setActive`, `clearInventory`, `addInventory(slot, item, price)`. World init derives `shops.HardMode` from the item pool, as VT's `World` constructor does.
2. **The two stages, in the library.** `setShops`: the draw-free path, ported exactly, and an assert with a message on the retro-only random path (generic keys, rupee bow, take-anys), which Phase 3's `state-retro` step ports. `setMedallions`: VT's loop over the Medallions region, skipping a location that already holds an item.
3. **The generator runs them.** After `getAdvancementItems`, the generator runs stage `setShops`, then stage `setMedallions`, then stops with `RANDO_INCOMPLETE`.
4. **The first C spoiler writer.** A host-only `spoiler_json` unit writes VT's `getSpoiler()` for the world as it stands. This step writes the region sections (every location, with its item or `Nothing`) and `Shops` (the active shops). The output uses the oracle's normalized format: keys sorted bytewise, lists kept in order, PHP's `JSON_PRETTY_PRINT` layout, a final newline. `Bosses`, `Equipped`, `meta` and `playthrough` are left to the steps that determine them. The host driver writes `spoiler.json` whenever the generator returns complete or incomplete.
5. **The gate's checks.** `stages.txt` gains `check setShops section Shops` and `check setMedallions keys *Medallion:1`.
6. **Claims.** Step 3.1's five claims move up to `setMedallions`. Three new claims are added for the three item pools whose `Shops` section differs from baseline's: `item_pool-hard`, `item_pool-expert`, `item_pool-crowd_control`.

`make -C tests/rando e2e` then prints eight `PASS … (setMedallions)` lines and 36 `PENDING`.

## User Stories

1. As the e2e gate, I want the C generator to write `<out_dir>/spoiler.json` when it stops after its last ported stage, so that the `section` and `keys` checks of a stage claim have a C file to compare.
2. As the e2e gate, I want the C `spoiler.json` to use the golden's normalized format: keys sorted bytewise, PHP pretty-print layout (four-space indent, `": "` separator, one member per line, no trailing comma on a last member), a final newline. Then `section_block` and `entries` read both files the same way.
3. As the e2e gate, I want `stages.txt` to declare `check setShops section Shops`, so that every claim at `setShops` or later compares the whole `Shops` block byte for byte.
4. As the e2e gate, I want `stages.txt` to declare `check setMedallions keys *Medallion:1`, so that every claim at `setMedallions` or later compares exactly the two medallion entries of `Special`, and not the fountain bottles, which `setFountains` decides later.
5. As the e2e gate, I want the C trace to carry two `setMedallions` draws after the four `getAdvancementItems` draws, with the stage labelled exactly as in the stage table, so that a `setMedallions` claim's prefix comparison covers six lines (seven for the `crystals_*-random` cases).
6. As the e2e gate, I want a stage that makes no draw (`setShops` here) to still run under its own stage label, so that an unexpected draw inside it would carry its true stage name in the trace.
7. As a C port developer, I want each shop to carry VT's kind (Shop, Take Any, Upgrade), so that `setShops` activates exactly the shops VT activates and the spoiler writes `"type": "Shop"` or `"Take Any"` as VT does.
8. As a C port developer, I want each shop slot to carry a price, initialized to VT's region defaults, so that the spoiler's `price` values come from the world, not from a table the stages cannot change.
9. As a C port developer, I want all shops inactive after world init, as VT's `Shop` constructor leaves them, so that a world `setShops` has not touched writes no `Shops` entry.
10. As a C port developer, I want world operations mirroring VT's `setActive`, `clearInventory` and `addInventory(slot, item, price)`, so that `setShops` reads like VT's code and Phase 3's retro shops reuse them.
11. As a C port developer, I want an empty slot to stay `RANDO_ITEM_NONE` (VT's missing inventory key), so that `collectItems`, the spoiler and the shop rules all agree on what a shop holds.
12. As a C port developer, I want `shops.HardMode` as a config field, derived by world init as true for item pools `hard` and `expert` (VT's `World` constructor), so that `setShops` reads one field and every derivation keeps its single home.
13. As a C port developer, I want `setShops` to activate every non–Take Any shop, `Capacity Upgrade` included, before anything else, so that a HardMode `Capacity Upgrade` stays listed in `Shops` with no item, as VT lists it.
14. As a C port developer, I want `setShops` to clear `Capacity Upgrade` under HardMode, so that `item_pool-hard` and `-expert` match their goldens and their `collectItems` no longer finds the two capacity upgrades.
15. As a C port developer, I want `setShops` to put the shield replacement in `Dark World Forest Shop` slot 0 at 500 when the shield overflow count is below 2, and in slot 1 of the Potion, Lumberjack Hut, Outcasts and Dark World Lake Hylia shops at 50 when it is below 1, so that `item_pool-expert` and `-crowd_control` match their goldens.
16. As a C port developer, I want the shield replacement read from an `item.overflow.replacement.Shield` config field that defaults to VT's `TwentyRupees2`, so that `setShops` reads the key as VT does, following `RandoConfig`'s one-field-per-key rule.
17. As a C port developer, I want `setShops` to assert with a message when generic keys, rupee bow or take-anys are on, because that path draws and only the retro world (not ported) turns it on, so that an unported path cannot pass silently.
18. As a C port developer, I want `setMedallions` to walk the Medallions region in VT's order (Turtle Rock, then Misery Mire) and draw `rando_random_int(0, 2)` into `[Ether, Bombos, Quake]`, so that the `crystals_*-random` cases (draws `0, 2`) give Turtle Rock Ether and Misery Mire Quake.
19. As a C port developer, I want `setMedallions` to skip a medallion location that already holds an item, without drawing, as VT does, so that a future config that pre-places a medallion keeps the draw count right.
20. As a C port developer, I want the medallion placed with the world's `set_item`, asserting it succeeds, so that the location-class guard of step 2.2 is exercised and a wrong item id fails loudly.
21. As a C port developer, I want neither stage to call `collectItems`, because VT caches the collectable shop locations on its first call, so that the cache is built after `setShops` has changed the shops, as VT builds it.
22. As a C port developer, I want the generator to stop with `RANDO_INCOMPLETE` after `setMedallions`, so that step 2.3.3 only appends `placeBosses` and `fillPrizes`.
23. As a host-tool developer, I want one writer call that takes a world and an open stream and returns false on an I/O error, so that the driver and the unit tests share it and the library prints nothing.
24. As a host-tool developer, I want the writer to name locations and items as VT's `getName()` does (raw name plus `:1`), and location items through VT's `getTarget()` (an alias writes its target), so that the entries match the golden's.
25. As a host-tool developer, I want shop items written with `getName()` and not `getTarget()`, as VT's shop loop does, so that a shop holding an alias in Phase 3 writes the alias's own name.
26. As a host-tool developer, I want the region sections to merge every region that shares a spoiler name (`Special` holds Medallions and Fountains; `Light World` holds several regions), skip `Prize\Event` and `Trade` locations, and write `Nothing` for an empty location, as VT's loop does, so that the writer has one rule (VT's `getSpoiler` on the current state) that later steps never have to bend.
27. As a host-tool developer, I want `Shops` written as a list in `getShops()` order, active shops only, each object with `item_<slot>` (only the slots that hold an item), `location` and `type`, and each item object with `item` and `price`, so that the block equals the golden's byte for byte.
28. As a host-tool developer, I want JSON strings escaped as PHP does with unescaped slashes and unicode (`"` and `\` escaped, control characters asserted absent), so that a future name with a quote cannot break the file silently.
29. As a host-tool developer, I want the driver to write `spoiler.json` on a complete or incomplete generation and never on an unsupported one, a give-up or an input error, so that a missing file always means the generator never reached a world worth showing.
30. As a host-tool developer, I want a failed spoiler write to exit with the input/output error code, so that a full disk is not reported as a divergence.
31. As a maintainer, I want the five step 3.1 claims raised to `setMedallions`, since the generator runs to its stop point anyway, so that a regression in either new stage on those configs fails the gate at no extra cost.
32. As a maintainer, I want claims only for the cases whose code path or checked output differs from baseline's in these stages, so that the e2e loop stays fast (CLAUDE.md). Among the 42 supported cases, the golden `Shops` sections show exactly four distinct outcomes (baseline, `hard`, `expert`, `crowd_control`), and the medallion entries show two (`0, 0` and the `crystals_*-random` `0, 2`).
33. As a maintainer, I want a deliberately broken stage to show up as `DIVERGENT` with the gate's check message, so that the new checks are proven to bite.
34. As a developer, I want `make -C tests/rando test` to run only hand-written cases of the new and changed modules and stay near its current time, so that the fast loop stays fast.
35. As a developer reading a divergence, I want the gate to say `setShops: section Shops:` or `setMedallions: keys *Medallion:1:` followed by the first differing line, so that I know which stage's output drifted without opening either file.
36. As the author of step 2.3.3, I want the writer already producing the region sections, prize locations included, so that checking the dungeon prizes is a `check` line plus the `Bosses` section.
37. As the author of step 2.3.4, I want the fountain bottles to land in the `Special` section the writer already produces, so that checking them is one `keys` line.
38. As the author of step 2.4, I want the writer's top-level layout already in place, so that adding `Equipped`, `meta` and `playthrough` only adds sections.
39. As a Phase 3 author (`state-retro`), I want the retro-only part of `setShops` to fail on an assert that names it, and the shop operations it needs to exist already, so that porting it is adding draws only.
40. As a DS integrator, I want shops' active flags and prices in `RandoWorld`, fixed-size and id-based, so that the DS build can later write shop data without a host-only structure.

## Implementation Decisions

All decisions below are mine; no `guideline` agent was consulted. Each rests on the plan's Rules and step notes, CLAUDE.md, the committed harness and library, step 3.1's PRD, and VT's source at the pinned commit. Every assumption about step 3.1's outcome was a hypothesis, labelled `H-5-n`; each has since been checked against the committed step 3.1 (see Hypotheses).

**Facts read from VT, the goldens and committed code (not hypotheses)**
- `prepareWorld` runs `setShops`, then `setMedallions`, after the four pool getters and a draw-free prologue (the goal's Triforce, the glitched-logic Boots swap, the `RescueZelda` pre-collect). `getSpoiler` (with `entrances` `none`) loops over `getRegions()`. For each location, it skips `Prize\Event` and `Trade` and writes `getTarget()->getName()` or `Nothing`. It then writes the active shops in `getShops()` order: `location`, `type` (`Take Any` for `Shop\TakeAny`, else `Shop`), and `item_<slot>` with `item` = `getName()` and `price`. `getName()` appends `:<world id>`, which is always `:1` here.
- VT has 41 Standard shops: 9 `Shop`, 1 `Shop\Upgrade` (`Capacity Upgrade`), 31 `Shop\TakeAny`. The C shop table (step 2.2) holds them in `getShops()` order with their default items, but without kinds or prices.
- The Medallions region (`Special`) holds `Turtle Rock Medallion` then `Misery Mire Medallion`. Fountains is also named `Special`. VT's `Location\Medallion::setItem` throws for a non-medallion, and the C `rando_world_set_item` already refuses one.
- The `shops.HardMode` key is set by item pools `hard`, `expert` (and `superexpert`, which is not in the matrix). `item.overflow.count.Shield` is hard 2, expert 1, crowd_control 0, default 3, and `RandoConfig` already has the field (default 3). `rom_generic_keys` and `rom_rupee_bow` already exist in `RandoConfig`; `region.takeAnys` has no field.
- Goldens: every supported case's attempt 0 has two `setMedallions` draws, `0, 0` except `crystals_ganon-random` and `crystals_tower-random`, which draw `0, 2` (Turtle Rock Ether, Misery Mire Quake). Only `state-retro` (unsupported) gives Bombos. No supported case draws in `setShops`. Their golden `Shops` sections equal baseline's except `item_pool-hard` (an item-less `Capacity Upgrade`), `item_pool-expert` (that, plus `TwentyRupees2` at 500 in Forest slot 0) and `item_pool-crowd_control` (the five replacements, `Capacity Upgrade` kept). The trace prefixes of the three item-pool cases through `setMedallions` equal baseline's.
- The gate: a `section` check compares the lines from `    "<Name>": ` to the first line that closes at four spaces, with the trailing comma dropped. `entries` reads only the 8-space `"key": value` lines under each top-level section (so the `Shops` list is invisible to `keys` and needs `section`). Checks owed by every stage up to the claimed one are run, in table order.

**World and config (library, modified)**
- `RandoShopInfo` gains `kind` (an enum: shop, take-any, upgrade) and `default_prices[RANDO_SHOP_SLOTS]`, filled for the ten shops with a default inventory from VT's region `initalize` code (RedPotion 150, Heart 10, TenBombs 50, BlueShield 50, RedShield 500, Bee 10, TenArrows 30, both capacity upgrades 100).
- `RandoWorld` gains `shop_active[RANDO_SHOP_COUNT]` (bool) and `shop_prices[RANDO_SHOP_COUNT][RANDO_SHOP_SLOTS]` (`uint16_t`, prices reach 500). `rando_world_init` copies the default prices and leaves every shop inactive. About 290 bytes are added to the step 2.2 size, which goes in the step note.
- New world functions: `rando_world_shop_active`, `rando_world_shop_price` (asserts the slot holds an item), `rando_world_shop_set_active`, `rando_world_shop_clear` (every slot to `RANDO_ITEM_NONE`), `rando_world_shop_add` (slot, item, price; replaces what the slot held, as VT's keyed `addInventory` does). The capacity upgrades' `max` and the replacement fields are not stored: nothing reads them before the ROM writer, which is out of the plan's scope.
- `RandoConfig` gains `shops_hard_mode` (default false). World init's derivation function sets it for item pools `hard` and `expert`, in the same `item.pool` branch where step 3.1 derives the shield overflow count (H-5-4). Step 3.1's `TODO` for `shops.HardMode` is removed.
- `RandoConfig` gains `item_overflow_replacement_shield` (a `RandoItemId`; `rando_config_init` sets it to `RANDO_ITEM_TwentyRupees2`, VT's default). No matrix config sets the key, but `setShops` reads it through `config()`, and `RandoConfig` documents one field per key the ported code reads. Step 3.1 made this possible: `rando_config.h` now includes `rando_item_ids.h`, as `item_count` already does.

**Prepare module (new, library)**
- A new library module for `Randomizer`'s world-setup stages, with a header. This step adds two functions, and later sub-steps add `placeBosses`, `fillPrizes` and `setFountains` beside them.
  - `rando_set_shops(RandoWorld *)`: activate every shop whose kind is not take-any. If `shops_hard_mode`, clear `Capacity Upgrade`. Shield replacements below 2 and below 1, slots and prices as VT's. Then, if `rom_generic_keys` or `rom_rupee_bow` is set, assert with a message that the retro shops are not ported (`region.takeAnys` has no field, and only the retro world sets it). No PRNG parameter: this path cannot draw, and the signature proves it. Phase 3 adds the parameter when it ports the random path.
  - `rando_set_medallions(RandoWorld *, RandoPrng *)`: for each location of the Medallions region in table order, skip it if it holds an item; otherwise draw `rando_random_int(prng, 0, 2)` and set `[Ether, Bombos, Quake][r]`, asserting `rando_world_set_item` succeeds.
- Comments kept (CLAUDE.md, traps only): Turtle Rock before Misery Mire (baseline's equal draws hide the order); HardMode leaves `Capacity Upgrade` active and listed; neither stage may call `collectItems` (VT's collectable cache).

**Generator (library, modified)**
- One attempt now runs, after the `getAdvancementItems` stage: stage `setShops` (`rando_set_shops`), stage `setMedallions` (`rando_set_medallions` with the generator's PRNG), then `RANDO_INCOMPLETE` (H-5-1). The current stage is set before each call, even for `setShops`, which makes no draw.
- The draw-free prologue of `prepareWorld` between the getters and `setShops` (Triforce, glitched Boots swap, `RescueZelda`) is not ported here: neither stage reads what it sets. It is owed by step 2.3.3 at the latest, because `fillPrizes`' `collectItems` reads the pre-collected items (H-5-9).

**Spoiler writer (new, host only)**
- It lives in `rando/host/` beside step 3.1's case reader (`rando_case.h`) and trace and attempts writers (`rando_out.h`), with its own header. The Makefile's host wildcard compiles it into `librando_host.a`, which is linked into every unit test and into `build/rando_gen` (H-5-2). The plan's layout names it `spoiler_json.c (host only)`; it never goes under `rando/src/`.
- Interface: `bool rando_spoiler_write_json(FILE *out, const RandoWorld *world)`. It writes a whole file and returns false on a stream error. The caller owns and closes the stream. It prints nothing else.
- Content, one rule: VT's `getSpoiler()` on the world as it stands, limited for now to:
  - **region sections:** for each region in world order, its spoiler name. For each location of the region, unless its class is a `Prize\Event` or `Trade` (VT's `instanceof`, via `rando_location_is_a`): key `<location name>:1`, value `<target name>:1` (the item's `alias_target` when it is an alias, else the item itself) or `Nothing`. Regions sharing a name merge into one object. The generic-keys `Key` substitution is a `TODO` for `state-retro`.
  - **`Shops`:** a list of the active shops in shop-id order. Each object holds `item_<slot>` for each slot with an item (an object of `item` = `<raw name>:1` and `price`), then `location` and `type`.
  - **Not yet:** `Bosses` (step 2.3.3), `Equipped`, `meta`, `playthrough` (step 2.4), each a `TODO` naming its step.
- Format: the oracle's `normalize()` then `json_encode(JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE)` plus `"\n"`. Object keys are sorted with `strcmp` (PHP's `ksort` `SORT_STRING`) at every level, lists keep their order, indentation is four spaces per level, an empty object or list prints `[]` (PHP encodes an empty array that way), integers are plain decimal. Strings escape `"` and `\`; a control character asserts.
- Implementation freedom: host code may use the C library (`qsort`, stdio). The library's no-`malloc` rule binds `rando/` sources, not host tools; a fixed buffer sized from the location and shop counts is still preferred.

**Host driver (modified)**
- In `rando/host/main/rando_gen.c`, after `rando_generate` returns `RANDO_COMPLETE` or `RANDO_INCOMPLETE`, the driver writes `<out_dir>/spoiler.json` from `gen.world` (H-5-1), then exits as before: `EXIT_COMPLETE` with `attempts`, or `EXIT_INCOMPLETE` (H-5-10). If the write fails, it prints the reason and exits `EXIT_INPUT`, which already covers 3.1's output errors (an unwritable trace or `attempts`). Unsupported, gave-up and input-error runs write no spoiler. On an unsupported run, `gen.world` was never initialized for that attempt, which is one more reason to write nothing.
- `run_e2e.sh` is unchanged: it already reads `spoiler.json` for due checks.

**Gate files**
- `stages.txt` gains two lines, below the stage list: `check setShops section Shops` and `check setMedallions keys *Medallion:1`.
- `claims.txt`: `baseline`, `crystals_ganon-random`, `crystals_tower-random`, `item_functionality-hard`, `item_functionality-expert`, `item_pool-hard`, `item_pool-expert`, `item_pool-crowd_control`, each at `setMedallions` (H-5-3).
- Claim rule, extending step 3.1's: one claim per distinct code path or distinct checked output through the claimed stages. An existing claim moves up to the newest stage whenever its case can reach it, because the generator runs to its stop point whatever the claim says. The ratchet costs only the checks' few `awk` runs. The 34 other supported cases reach `setMedallions` with baseline's `setShops` path, baseline's `Shops` block and baseline's medallion draws, so claiming them adds time and no coverage.
- Speed: eight claims instead of five. The commit records `test` and `e2e` times before and after; budget: e2e well under 0.5 s (H-5-6).

## Implementation Chunks

Each chunk leaves `make -C tests/rando test`, `logic`, `selftest` and `e2e` green.

1. **Shops in the world, HardMode in the config.** Shop kinds and default prices in the table; `shop_active`, `shop_prices` and the five shop functions; `shops_hard_mode` and its derivation. `test_world` gains the hand-written cases below; `make logic` unchanged and green.
2. **Prepare module and generator stages.** `rando_set_shops`, `rando_set_medallions`, their header; the generator's two new stages and the moved stop point. `test_prepare` (new, registered in `UNIT_TESTS` and in `logic`'s filter, since it has no fixture mode). `test_generate`'s stop-point expectations move from four to six draws: the `trace.lines == 4` checks and `gen.draw == 4` (H-5-8). Its `--fixtures` pool replay keeps expecting `RANDO_INCOMPLETE`, unchanged. `test_prepare` and `test_spoiler_json` join `LOGIC_TESTS`' filter-out list beside `test_options`, `test_case_reader` and `test_pool`.
3. **Spoiler writer.** `rando_spoiler_write_json` in the host archive; `test_spoiler_json` (new, registered like `test_prepare`).
4. **Driver, gate and claims.** The driver writes `spoiler.json`; the two `check` lines; the eight claims. Verify `e2e`: eight `PASS … (setMedallions)`, 36 `PENDING`, nothing `DIVERGENT`. Then two deliberate breaks, each reverted: swap the medallion locations' order, which must make both `crystals_*-random` cases `DIVERGENT` with `setMedallions: keys *Medallion:1:`; skip the HardMode clear, which must make `item_pool-hard` and `-expert` `DIVERGENT` with `setShops: section Shops:`. Record `test` and `e2e` times.

## Testing Decisions

- **A good test checks external behavior:** what a shop holds, at what price, whether it is active, which medallion a location holds after the stage, which draws the trace hook receives, and the text the writer produces. Never struct layout, capacities or the writer's internal buffers.
- **VT is the oracle.** Hand-written expectations restate VT's source or a golden's lines and cite them. A hand-written case that contradicts a golden is the test's bug (CLAUDE.md).
- **`test_world` (extended):** after init, every shop is inactive; `Dark World Forest Shop` slot 0 is `RedShield` at 500 and `Capacity Upgrade` slot 1 is `ArrowUpgrade5` at 100; shop kinds give 9 shops, 1 upgrade, 31 take-anys; clear empties every slot; add replaces a slot's item and price; `shops_hard_mode` is true for item pools `hard` and `expert` and false for `normal` and `crowd_control`.
- **`test_prepare` (new):**
  - `rando_set_shops` on a baseline world: exactly the 10 non-take-any shops active, inventories unchanged.
  - On a `hard` world: `Capacity Upgrade` active and empty, no shield replaced.
  - On an `expert` world: that, plus Forest slot 0 `TwentyRupees2` at 500.
  - With `item_overflow_replacement_shield` set to another item on a `crowd_control` config: that item in the five slots (the field is read, not hard-coded).
  - On a `crowd_control` world: the five replacements at their prices, `Capacity Upgrade` untouched.
  - `rando_set_medallions`:
    - with a PRNG seeded as the `crystals_ganon-random` attempt 0 and advanced past its first five draws, Turtle Rock gets Ether and Misery Mire gets Quake;
    - the index map (0 Ether, 1 Bombos, 2 Quake), checked against a twin PRNG with the same seed;
    - a pre-set Misery Mire medallion is skipped with exactly one draw.

  The retro assert is not tested (an assert cannot be caught in a plain test). Its message is the documentation.
- **`test_generate` (updated):**
  - baseline seed 1 gives `RANDO_INCOMPLETE` and exactly the golden's first six lines through the hook, the last two labelled `setMedallions`;
  - `crystals_ganon-random` gives the golden's first seven;
  - after the baseline run, the generator state's world holds Ether at both medallion locations and has the ten shops active.

  Step 3.1's four-draw expectation is updated, not kept: it froze the old stop point (CLAUDE.md).
- **`test_spoiler_json` (new):**
  - The writer, run on the world of a baseline generation stopped after `setMedallions`, produces a `Shops` block whose lines equal the baseline golden's (read from `cases/baseline/spoiler.json`, the source of truth).
  - Same for the `item_pool-hard` golden's block, which covers an item-less shop object.
  - Its `Special` section holds the two medallion lines of the golden and `Nothing` for both fountains.
  - Top-level keys come out in `strcmp` order (`Shops` before `Skull Woods` before `Special`); a `Prize\Event` location (`Agahnim`) is absent; the file ends with `}` and one newline.

  Output goes through `tmpfile()`, so the test stays C99.
- **Driver and gate:** covered by the e2e claims and chunk 4's two deliberate breaks. `selftest` is unchanged: the gate's `section` and `keys` machinery already has its fixtures (step 2.1).
- **Speed:** `test` gains two hand-written tests (milliseconds, a golden read each). Nothing moves into `logic`, since neither new test has a fixture mode. `e2e` runs eight claims.
- **Prior art:** `test_world` for shop assertions; step 3.1's `test_generate` for driving a generation through the hook; `test_prng` for a plain `main` with a first-failure exit; the test support's `test_fail`.

## Hypotheses

Step 3.1 is now committed (`### Step 2.3.1` note; `rando/include/rando.h`, `rando/src/generate.c`, `rando/host/`, `tests/rando/Makefile`, `claims.txt`, `test_generate`). Each hypothesis below was checked against that code. The sections above already apply the verdicts.

- **H-5-1 — Generator shape: holds.** `RandoGen` embeds `options`, `seed`, `attempt`, `stage`, `draw`, the trace hook, `prng`, `pools` and `world`, and is caller-owned, so `gen.world` holds the last attempt's world after `rando_generate` returns. `RandoStage` in `rando.h` lists the ten stages in `stages.txt` order, including `RANDO_STAGE_SET_SHOPS` and `RANDO_STAGE_SET_MEDALLIONS`, with `rando_stage_name`. `run_attempt` sets `gen->stage` around each stage, resets it to `RANDO_STAGE_NONE` after, and returns `RANDO_INCOMPLETE` after `getAdvancementItems`. The two new stages go just before that return, in the same style. Detail applied: on `RANDO_UNSUPPORTED`, `gen.world` is not initialized for the attempt, so the driver writes no spoiler then.
- **H-5-2 — Host-only sources: holds.** Host units are in `rando/host/` (`case_reader.c`, `rando_out.c`, headers `rando_case.h`, `rando_out.h`). The Makefile's `HOST_SRC` wildcard (`rando/host/*.c`) builds them into `librando_host.a`, which is linked into every unit test and into `build/rando_gen`. The driver is `rando/host/main/rando_gen.c`. Applied: `spoiler_json.c` and its header go in `rando/host/`, so no Makefile rule changes for the writer.
- **H-5-3 — Claims after 3.1: holds.** `claims.txt` holds exactly the five claims at `getAdvancementItems`. e2e prints 5 pass, 39 pending.
- **H-5-4 — Item-pool config: holds.** `RandoConfig` has `item_pool` (`RANDO_ITEM_POOL_NORMAL`, `HARD`, `EXPERT`, `CROWD_CONTROL`). World init's `item.pool` switch sets `item_overflow_count_shield` to 2, 1 and 0, and carries `/* TODO: item.pool's overflow counts other than Shield, and shops.HardMode. */`. Applied: `shops_hard_mode` is set in that switch, and the TODO drops `shops.HardMode`. Side finding, not a hypothesis: `rando_config.h` now includes `rando_item_ids.h` (for `item_count`). The earlier decision "no field for `item.overflow.replacement.Shield`, because `RandoConfig` cannot hold an item id" no longer stands, so the field is added (story 16, the World and config decisions, a `test_prepare` case).
- **H-5-5 — No early `collectItems`: holds.** In the library, `rando_world_collect_items` is called only from the escape regions' Sanctuary rules (`open_escape.c`, `standard_escape.c`), which access checks evaluate. World init, `rando_pool_build` and `generate.c` evaluate none, so the collectable cache is first built after `setShops`.
- **H-5-6 — Speed after 3.1: holds.** Measured on the committed tree: `make -C tests/rando test` about 45 ms, `e2e` about 0.1 s for five claims. Eight claims should stay near 0.15 s.
- **H-5-7 — Stage table untouched: holds.** `stages.txt` has the same ten `stage` lines and no `check` line. `test_generate`'s stage-table check skips every line whose kind is not `stage`, so the two new `check` lines do not affect it.
- **H-5-8 — `test_generate` pins the stop point: holds.** It asserts `RANDO_INCOMPLETE` for baseline, `trace.lines == 4` (baseline, `state` `standard`, and the reseeding cases) and `gen.draw == 4`. Applied: these become 6 (7 with a `random` crystal count), and the update is noted in chunk 2.
- **H-5-9 — `prepareWorld`'s prologue not ported: holds.** No C code places the Triforce, swaps Boots or pre-collects `RescueZelda`. `generate.c` goes straight from world init to the pool stage.
- **H-5-10 — Driver exit codes: holds.** `rando_gen.c` defines `EXIT_COMPLETE` 0, `EXIT_INPUT` 1 (input and output errors alike), `EXIT_USAGE` 2, `EXIT_INCOMPLETE` 3, `EXIT_UNSUPPORTED` 4 and `EXIT_GAVE_UP` 5. It writes `trace.txt`, and `attempts` only on complete, never a `spoiler.json`.

## Out of Scope

- `setShops`' random path (take-anys, generic keys, rupee bow) and the retro and inverted worlds (Phase 3).
- `placeBosses`, `fillPrizes`, `setFountains`, `shufflePrizePacks`, `prepareWorld`'s own work, its draw-free prologue, the fill and `isWinnable()` (steps 2.3.3 to 2.4).
- The spoiler's `Bosses`, `Equipped`, `meta` and `playthrough` sections, the generic-keys `Key` substitution, and `pool.txt`.
- Claiming any case other than the eight, or at any stage past `setMedallions`.
- Changing `run_e2e.sh`, `selftest.sh`, a golden, `vt.patch` or the stage table's stage lines.
- Shop data the spoiler does not show (`max`, replacement items, shopkeeper, ROM bytes).

## Further Notes

- **Why `section Shops` and `keys *Medallion:1`, not `section Special`.** `Shops` is a list, which `keys` cannot see, and `setShops` fully determines it in every supported case. `Special` also holds the two fountain bottles, which stay `Nothing` in C until step 2.3.4, so only the medallion keys are due now. Step 2.3.4 can add a `*Bottle:1` keys check, or turn the pair into `section Special`.
- **Why write every region section now.** It is VT's own loop on the current state, and nothing checks it yet. But step 2.3.3's prizes (`… - Prize:1`) and step 2.3.4's bottles live in those sections, so later steps only add `check` lines. An unfilled location writes `Nothing`, which no due check reads.
- **Step note material** for `### Step 2.3.2`: the claims and the ratchet rule, the two `check` lines, `test` and `e2e` times, the `RandoWorld` size change, the two deliberate breaks and their messages, where each hypothesis landed, and that `prepareWorld`'s prologue is owed by 2.3.3.
