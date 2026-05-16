# M0.3 - GameState design notes

Companion notes for `feature/m0-03-game-state`. Locks in the structural
rules that every later Milestone 0 sub-milestone (and Milestone 1) will
have to abide by. The key invariants here are easy to break later, so
they get written down up front.

## 1. The "container is dumb" rule

`GameState` is a **passive data struct** with **zero methods**.

```cpp
struct GameState {
    GameDate    current_date{1930, 1, 1};
    RandomState rng{};

    std::vector<CountryState>    countries;
    std::vector<ProvinceState>   provinces;
    std::vector<FactionState>    factions;
    std::vector<PolicyData>      policies;
    std::vector<EventDefinition> events;
    std::vector<LogEntry>        logs;
};
```

Every transformation that needs to read or mutate game state is a **free
function** (eventually grouped into a "system") that takes a
`GameState&` (or `const GameState&` if read-only).

That maps directly onto RFC-060 §2 ("資料集中、系統分離"):

| RFC system | M0 milestone | API shape |
|------------|-------------|------------|
| TimeSystem | M0.4 | `void advance_one_day(GameState&)` |
| RandomService | M0.5 | reads/writes `state.rng` |
| LoggingSystem | M0.6 | `void log(GameState&, LogEntry)` |
| DataLoader | M0.7 | populates `state.countries` etc. |
| SaveSystem | M0.8 | `void save(const GameState&, ...)` |

If a future PR adds methods to `GameState`, that's a code smell. The
methods should be free functions, and `GameState` should stay a
container.

## 2. Why a free function factory, not a constructor

```cpp
GameState make_game_state(const SimulationConfig& config);
```

A constructor `GameState(const SimulationConfig&)` would also work, but
it would quietly bless `GameState` as a thing that *does work* during
construction. Once that pattern is acceptable, the temptation to make
`GameState::tick()`, `GameState::apply(policy)`, etc. follows.

Keeping the factory external preserves the "dumb container" rule by
construction. The factory itself is intentionally trivial (set date,
set seed, zero the counter). Heavier initialisation flows (load JSON,
seed entity tables, replay save) compose around this factory in later
milestones rather than mutate it.

## 3. Why `rng.counter` resets to 0

Determinism requirement (RFC-000 §5 rule 10): "所有隨機都必須經 seed 控制，
長期目標是可重播."

If the RNG counter survived across `make_game_state` calls, two runs
with the same seed could diverge depending on prior process state.
Resetting it forces every freshly-made `GameState` to start its random
sequence at counter = 0, given the same seed.

The counter advances only through `RandomService` calls in M0.5; tests
that check determinism will compare `(seed, counter)` pairs.

## 4. Entity placeholders

Each entity type in `entities.hpp` carries only what M0.3 must hold:

| Type | Fields | Notes |
|------|--------|-------|
| `CountryState` | `id`, `name` | Real fields (GDP, stability, factions, etc.) land in M1.1. |
| `ProvinceState` | `id`, `owner` | Owner is the country it currently belongs to. Controller diverges from owner during war (much later). |
| `FactionState` | `id`, `country` | Support/influence/radicalism land in M1.2. |
| `PolicyData` | `id`, `name` | "Data" because it's the policy definition; per-country instances are a separate concept that arrives in M1.4. |
| `EventDefinition` | `id`, `name` | The template; an `EventInstance` / `EventLogEntry` will live in `logs`. |
| `LogEntry` | `date`, `message` | Real fields (category, severity, source, metadata) land in M0.6. |
| `RandomState` | `seed`, `counter` | Service that consumes this lands in M0.5. |

These deliberately understaffed structs let us prove the *plumbing*
(containers exist, can hold values, can be mutated, round-trip through
the factory) without committing to field shapes that Milestone 1 will
rewrite.

## 5. Non-goals for M0.3

- No JSON deserialisation. `SimulationConfig` mirrors RFC-070's JSON
  shape so M0.7 can map fields directly, but the loader itself is out
  of scope.
- No save/load. M0.8 will round-trip a `GameState` through JSON;
  M0.3's only obligation is to *be* round-trippable in principle
  (POD-ish, no pointers, no hidden state).
- No copy-policy decisions. The default move/copy semantics from
  `std::vector` are correct for now. We may pin them explicitly when
  a system needs to enforce single-ownership.
- No invariants enforced by the type system beyond `is_valid()` on
  `start_date`. Cross-entity invariants (e.g. "every faction's
  `country` ID exists in `countries`") will be checked by a diagnostics
  pass in M0.10, not by `GameState` itself.
