# When to Use a Test Double

Replace only **system boundaries**:

- The DS hardware: libnds and the ARM9 host are never linked into a unit test; `rando/` builds without them
- Files: a golden is read by `run_e2e.sh`, never by a unit test

Don't replace:

- Your own modules
- Internal collaborators
- Anything in `rando/` a test can call for real
- Randomness: the PRNG takes its seed as a parameter, so a run is reproducible as is

## Designing for Test Doubles

**1. Pass the dependency in**

The module receives its PRNG and its world as parameters instead of reaching for a global:

```c
/* Easy to test: the caller chooses the seed */
void fill_items(rando_world_t *world, prng_t *prng);

/* Hard to test: hidden dependency on a global generator */
void fill_items(rando_world_t *world);
```

**2. One narrow function per boundary operation**

Each operation is its own function, not one `do(op, ...)` entry point with a switch:

- No conditional logic in the test setup
- Easy to see which operations a test exercises
