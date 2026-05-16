# M0.4 - TimeSystem design notes

Companion notes for `feature/m0-04-time-system`. Locks in the
free-function-systems pattern that every later milestone will follow,
and answers the obvious "where do callbacks go" question with the
loop-on-`advance_one_day` pattern.

## 1. Free functions, not methods

```cpp
namespace leviathan::systems::time {
    TickResult advance_one_day(GameState&);
    void       advance_days(GameState&, int days);
}
```

`GameState` still has zero methods. Every system in the project will
follow the same shape:

| System | Header | Entry point |
|--------|--------|-------------|
| TimeSystem (M0.4) | `systems/time_system.hpp` | `advance_one_day(state)`, `advance_days(state, n)` |
| RandomService (M0.5) | `systems/random_service.hpp` | `draw_int(state, ...)`, `draw_double(state, ...)` |
| LoggingSystem (M0.6) | `systems/logging_system.hpp` | `log(state, entry)`, `export(state, ...)` |
| DataLoader (M0.7) | `systems/data_loader.hpp` | `load_simulation_config(path)`, `load_country(...)` |
| SaveSystem (M0.8) | `systems/save_system.hpp` | `save(state, path)`, `load(path)` |

If a future PR introduces something that doesn't fit this shape, that's
a signal that the new system wants state of its own — and the right move
is to add a field to `GameState`, not to wrap a class around it.

## 2. New library: `leviathan_systems`

This milestone introduces `src/leviathan/systems/CMakeLists.txt`, which
builds `leviathan_systems` (alias `leviathan::systems`). Dependency
direction is strictly one-way:

```
leviathan_main  ──┐
                  ├──> leviathan::systems ──> leviathan::core
leviathan_tests ──┘
```

`leviathan_core` does not link `leviathan_systems`. This keeps the
data layer reusable in tools (M0.10 diagnostics, M0.8 save round-trip
fuzzers, etc.) without dragging in every system.

## 3. Boundary detection: `TickResult`

```cpp
struct TickResult {
    bool month_changed = false;
    bool year_changed  = false;
};
```

`advance_one_day` returns a `TickResult` so callers can react to
calendar boundaries (e.g. "run monthly GDP calc on month_changed",
"emit annual stats on year_changed") without re-checking the date
themselves.

`advance_days` deliberately returns `void`. A caller that needs
per-day boundaries should loop on `advance_one_day` themselves:

```cpp
for (int i = 0; i < n; ++i) {
    const TickResult r = advance_one_day(state);
    if (r.month_changed) /* monthly system pipeline */;
    if (r.year_changed)  /* yearly system pipeline */;
}
```

This is the "system pipeline minimal structure" the spec mentions.
No callback infrastructure, no event bus, no polymorphism - just a
loop. Future milestones that need a richer pipeline (M0.6 logging,
M1+ economy) compose around this primitive rather than replace it.

## 4. What TimeSystem does NOT do

- **No economy tick.** GDP and tax calculations land in M1.13/M1.14
  and will be invoked by the M1 monthly pipeline.
- **No faction calculations.** M0.4 leaves `state.factions` untouched.
- **No event evaluation.** M0.5 + RFC-050 event system stays out of
  scope.
- **No log emission.** TimeSystem does not write to `state.logs`.
  M0.6 will, and may choose to log day-by-day or only on boundary
  crossings.
- **No RNG draws.** `state.rng.counter` is untouched. The dedicated
  determinism test in `time_system_test.cpp` proves this.

## 5. Negative deltas: still unsupported

`advance_days(state, -1)` asserts. Simulation time only moves forward.
If a future feature genuinely needs "rewind by N days" (replay
inspection, save scrubbing), it should be a separate operation with
explicit semantics, not an overload of the tick function.

## 6. Determinism

TimeSystem is deterministic *by construction*: it makes no random
draws and reads no external state. Same starting `current_date` plus
same number of `advance_one_day` calls = same final date, every time.
The `(seed, counter)` pair stays exactly where it was - confirmed by
the dedicated regression test.
