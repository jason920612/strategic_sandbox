# Project Leviathan

一款以 1930–2000 全球政治沙盒為背景的即時暫停制國家領導人模擬遊戲。玩家不直接微操軍隊，而是透過法律、行政命令、預算、任命、外交與戰略目標影響國家。國內派系、官僚、軍方、情報部門、地方利益集團、技術菁英、媒體、學生、宗教、工會、農民等會依照自身利益自動反應，讓世界局勢自然演化、混沌且不可完全預測。

> Code name: **Project Leviathan**. Final title TBD — see `rfc/RFC-000-overview.md`.

## Status

- Phase: **Milestone 2 — player-operation prototype (closed).**
  Both M1 single-country internal-politics prototype and M2
  player-operation prototype have shipped. See
  `docs/milestone-2-result.md` for the M2 exit report.
- Latest shipped sub-milestone: **M2.22 — M2 exit / integration
  tests.** Closes M2. Ships three end-to-end integration tests
  in `tests/integration/m2_end_to_end_test.cpp` pinning the
  M2 player-operation surface at the seam to M3+:
  (1) command script + replay + verify equivalence — drives
  `apply_command_script` on a source state, replays the resulting
  `applied_commands` log via `replay_with_time` into a target
  state, and asserts `diagnostics::compare_states` reports zero
  mismatches across every gameplay-relevant field;
  (2) order-execution gate atomicity across kinds — a mixed
  script (military AdjustBudget + EnactPolicy + welfare
  AdjustBudget) against a low-bureaucratic-compliance country
  with high military loyalty lands only the military entry,
  rejects the EnactPolicy at the gate, and leaves the welfare
  AdjustBudget unreached, with state / queue / applied_commands
  all consistent with per-command atomicity;
  (3) 5-artefact byte-identical determinism with M2 commands —
  M1.17's determinism contract (save.json + events.jsonl +
  summary.csv + countries.csv + factions.csv) extended through
  a 31-day run that applies commands at day 0 via
  `apply_command_script`. New `docs/milestone-2-result.md`
  exit report lists every sub-milestone, the architectural
  invariants future milestones must preserve, and the deferred
  items (Delayed / Distorted outcomes, scheduler, RNG-based
  resistance, attempted-command log, CLI script flag,
  runner-level rejection surface, expanded authority fields,
  authority drift, faction reactions, etc.) that consciously
  did not ship in M2. **M2 closes here.** Save format stays
  v10; no library behaviour change; no new gameplay; no save
  schema change. **Future player-operation work moves to M3+
  or to separate future milestones.**
- Previously shipped (M2 highlights): **M2.21** command script
  driver helper. **M2.20** command rejection reporting
  (`RejectionRecord` + `try_apply_pending`). **M2.18 / M2.19**
  EnactPolicy + AdjustBudget execution gates. **M2.17**
  OrderExecutionSystem skeleton. **M2.16**
  `GovernmentAuthorityState` (save schema v9 → v10). **M2.14**
  Replay target-date CLI. **M2.9** Replay CLI error-path
  hardening. **M2.13** Verify tolerance CLI. **M2.8 / M2.11 /
  M2.12** `--replay` / `--verify` / `--verify-strict` CLI
  family.
- Next phase: **M3 or targeted post-M2 follow-ups.** Natural
  candidates (per `docs/milestone-2-result.md` §4): promote
  `ExecutionStatus::Delayed` from reserved-name to a real
  outcome with a pending-execution queue; surface rejection at
  the runner / CLI level; add a CLI `--script PATH` entry point
  driving `apply_command_script`; authority drift system;
  faction reactions to player commands; multi-country
  interaction. None committed.
- M0 closed. M1 closed. M2 closed. See
  `docs/milestone-0-result.md`, `docs/milestone-1-result.md`,
  and `docs/milestone-2-result.md` for the exit reports, and
  `rfc/RFC-090-roadmap.md` for the full milestone map.

`GameState` is a passive container. Systems shipped in M0:
`leviathan::systems::time` (date advance + boundary detection);
`leviathan::systems::random` (deterministic splitmix64 RNG, no
`<random>`); `leviathan::systems::logging` (explicit-only logging
with byte-stable JSONL); `leviathan::systems::data_loader` (JSON
config + country parsers via nlohmann/json);
`leviathan::systems::save_system` (JSON round-trip with `save_version`
/ `rng_algorithm_version` gates); `leviathan::systems::runner` (CLI
`leviathan --days N [--config ...] [--seed ...] [--output ...]
[--summary-csv ...]`); `leviathan::systems::diagnostics`
(observation-only `snapshot()` + summary CSV + `sanity_check()`).
Two runs with the same options produce byte-identical save, log,
and summary-CSV files. M0 closes with a full end-to-end integration
test (`tests/integration/m0_end_to_end_test.cpp`) that loads three
country JSON files, ticks 365 days, saves, loads back, and verifies
the round-trip.

**Milestone 1** (single-country internal politics prototype,
RFC-090 §M1) is complete; **Milestone 2** (player-operation
prototype, RFC-090 §M2) is also complete with M2.1 + M2.2 +
M2.3 + M2.4 + M2.5 + M2.6 + M2.7 + M2.8 + M2.9 + M2.10 + M2.11
+ M2.12 + M2.13 + M2.14 + M2.16 + M2.17 + M2.18 + M2.19 +
M2.20 + M2.21 + M2.22 merged. Thirty-eight sub-milestones
shipped:
M1.1 CountryState fields; M1.2 FactionState; M1.3 BudgetState
(seven categories, no sum-to-1 enforcement); M1.4 PolicyData +
PolicyEffect; M1.5 PolicySystem `apply_policy_effects` (first real
gameplay effect, atomic via pre-flight); M1.6 FactionSystem `react`
(linear-toward-equilibrium loyalty / support drift); M1.7
StabilitySystem `tick` (first country-side dynamic, stripped-down
RFC-080 §5); M1.8 EconomySystem `tick` (RFC-080 §3 tax revenue,
expenditure = `gdp × sum_budget × 0.20`, stripped-down RFC-080 §4
GDP growth); M1.9 MonthlyPipeline `tick_country` /
`tick_all_countries` (first composition sub-milestone with canonical
order `faction::react → stability::tick → economy::tick`); M1.10
runner monthly pipeline wiring (every `month_changed` invokes
`monthly::tick_all_countries`; `run_state(state, opts)` exposed for
test injection); M1.11 scenario loader (`--scenario PATH` flag +
`scenario_loader::load_into_state` compose the M0.7 / M1.1 / M1.2
/ M1.4 parsers into a manifest-driven loader; `leviathan --days
365 --scenario data/scenarios/1930_minimal.json` produces a
non-empty world end-to-end without test-only injection); M1.12
economy → stability coupling (new `CountryState::last_gdp_growth_rate`
field, `economy::tick` writes it, `stability::tick` reads it as the
RFC-080 §5 `EconomicGrowth` term with `kEconomicGrowthWeight = 2.0`;
monthly pipeline order unchanged, intentional one-month lag;
save format bumped v5 → v6, first M1 save-schema bump); M1.13
scenario starting policies (manifest gains optional
`starting_policies` array of `{policy, actor}` id_code pairs;
loader applies each via `policy::apply_policy_effects` exactly
once at day 0, with the new fixture
`data/scenarios/1930_with_start_policies.json` enacting
`raise_taxes` + `increase_military_budget` on GER); M1.14
Diagnostics surfaces `last_gdp_growth_rate` — new
`CountrySummaryRow` + `country_snapshot` + per-country CSV writers,
plus opt-in `--countries-csv PATH` runner flag emitting 8 columns
per country per snapshot point (existing `--summary-csv` byte-for-
byte unchanged, M0.10 determinism contract preserved); **M1.15
Policy duration tracking — new `ActivePolicy{policy_id_code,
expires_on}` core type and `CountryState::active_policies` vector;
every successful `policy::apply_policy_effects` records one entry
with `expires_on = current_date + duration_days`; pre-flight
failure appends nothing. Save format bumped v6 → v7 (v6 saves
rejected loudly, v7 country without `active_policies` rejected).
Tracking-only: no expiration sweep, no revert, no scheduler.
`apply_policy_effects` also enforces a runtime cap on
`duration_days` (`kMaxTrackedPolicyDurationDays = 36500`, ~100
years) and rejects negatives, because `GameDate::advance_days` is
a per-day loop; **M1.16 Faction-level diagnostics CSV — new
`FactionSummaryRow` + `faction_snapshot` + per-faction CSV
writers, plus opt-in `--factions-csv PATH` runner flag emitting
9 columns per faction per snapshot point. Existing summary CSV
and per-country CSV both byte-for-byte unchanged (M0.10 + M1.14
contracts preserved). No save-format bump (still v7); **M1.17 M1
exit / integration tests — new
`tests/integration/m1_end_to_end_test.cpp` (1-year scenario run +
10-year soak run + 5-artefact byte-identical determinism), new
`docs/milestone-1-result.md` exit report, drive-by `main()`
milestone-label cleanup. No new system, no new flag, no save
schema change. M1 closes here; **M2.1 Player country selection —
new `GameState::player_country` (`CountryId`, default invalid),
new `--player COUNTRY_IDCODE` runner flag resolved after scenario
load. Save format bumped v7 → v8 (`"player_country"` required at
root; v7 rejected loudly). M1 systems unchanged: no behaviour
branches on `player_country` yet. Opens RFC-090 §M2; **M2.2 Pause
/ resume / step primitives — new `runner::TickController` runtime
struct (lives outside `GameState`) plus three free functions:
`begin_tick` / `step_one_day` / `end_tick`. `run_state` is
rewritten as a thin composition over them; M1.17's 5-artefact
byte-identical determinism contract is preserved (two new
equivalence tests pin it). Misuse rejected. No save format change
(still v8); no new CLI flag; no new logs. Drive-by: 2 regression
tests pinning that bad `--player` writes no on-disk artefacts;
**M2.3 Player command queue — new `core::PlayerCommand` (kind +
`policy_id_code`; M2.3 ships `EnactPolicy`) and new
`systems::commands::{CommandQueue, apply_pending}` module. Queue
is driver-owned (not in `GameState` or `TickController`).
`apply_pending` requires `state.player_country` to index into
`state.countries`, drains in insertion order, dispatches each
`EnactPolicy` through `policy::apply_policy_effects` (reusing
M1.5 atomicity + M1.15 active_policies tracking + duration cap).
Non-atomic across the list; first failure stops with failed cmd
at head. No save format change (still v8); no new flag / log / M1
system change; **M2.4 Player command log — new
`core::AppliedPlayerCommand{applied_on, command}` type and new
`GameState::applied_commands` vector. `commands::apply_pending`
appends one log entry per successful per-command dispatch (after
the M1.5 / M1.15 mutations land), so per-command atomicity covers
the log too; failed commands stay in the queue and do NOT log.
`applied_on` captures `state.current_date` at apply time. **Save
format bumped v8 → v9** with `"applied_commands"` as a required
root-level array; v8 saves rejected loudly; malformed entries
all rejected with `applied_commands[N]` in the error. Foundation
for future deterministic replay (RFC-050 §8). No M1 system
behaviour change; **M2.5 AdjustBudget player command — extends
`PlayerCommandKind` + `PlayerCommand` (new `budget_category` +
`budget_delta` fields). `apply_pending` gains a new switch arm
that validates the 7-category whitelist + finite delta, then
applies `budget.<category> += delta` and clamps to `[0, 1]`.
Per-command atomicity (M2.3) and log-on-success (M2.4) shared
unchanged. Save kind mapping grows to handle `"AdjustBudget"`
with per-kind JSON shape. No save format bump (still v9). Drive-
by: `player_command_kind_to_string` fallback now returns
`"UnknownPlayerCommandKind"` sentinel rather than a real kind
string, addressing the PR #32 reviewer nit; **M2.6 Replay applied
command log prototype — new `systems::commands::replay(state,
log)` free function. For each log entry forces
`state.current_date = applied_on`, builds a 1-element
`CommandQueue`, calls `apply_pending` (reusing M2.3 dispatch +
M2.4 log append + M1.5/M1.15 effect machinery unchanged).
Preconditions: `player_country` valid + `applied_commands`
empty. Atomicity across the log mirrors M2.3 mid-list-failure:
failed entry reported with `replay[N]: ...` in the error,
prior entries stay applied + logged, subsequent entries skipped.
Prototype limits pinned by tests: no time-system advancement
between commands, `current_date` ends at last entry, scenario
must be pre-loaded. No save format change; **M2.7 Replay with
time-system advancement — new
`systems::commands::replay_with_time(state, opts, ctrl, log)`
free function lifting M2.6's "no time advance" limit. For each
log entry, advances via M2.2 `step_one_day` until
`current_date == applied_on` (so M1.10 monthly pipeline runs on
boundaries naturally), then dispatches via 1-element queue +
`apply_pending`. Preconditions add `ctrl.started && !ctrl.ended`
and monotonic non-decreasing dates (addresses PR #34 nit).
Killer equivalence test pins that replay reproduces the original
simulation's state byte-for-byte (current_date, days_stepped,
monthly_ticks, log entries, command effects, and monthly-
pipeline-mutated fields). No save format change; **M2.8 Replay
CLI harness — `--replay PATH` runner flag wires M2.7 into the
CLI. `run()` branches: with `--replay` set (requires
`--scenario`), loads the save at PATH, optionally inherits
`player_country` from the loaded save when `--player` is unset,
runs `begin_tick → replay_with_time → end_tick`, and populates
a new `RunOutcome::replay_commands_replayed` field. `main()`
prints two extra summary lines. The CLI does NOT auto-compare —
user diffs source vs target save files. No save format change;
**M2.14 Replay target-date CLI — new `--target-date YYYY-MM-DD`
runner flag (requires `--replay`). Combines two effects: log
truncation (entries with `applied_on > target_date` are skipped
before `replay_with_time` runs) + post-replay time-system
extension (`step_one_day` loop until `current_date == target_date`,
so the M1.10 monthly pipeline fires on every month boundary
crossed). Parsed via `core::GameDate::parse` (rejects malformed
dates at parse time). Scenario-start precondition checked in
`run()` before any tick — pre-`end_tick` failure under the M2.9
contract, so bad target_date writes no artefacts. `main()` prints
`Target date: <value>` in the replay block when set.
`replay_with_time` and `step_one_day` semantics are unchanged;
M2.14 is glue. No save format change;
**M2.22 M2 exit / integration tests — closes M2. New
`tests/integration/m2_end_to_end_test.cpp` ships 3 end-to-end
tests pinning the player-operation surface: command script +
replay + `compare_states` equivalence, order-execution gate
atomicity across `EnactPolicy` and `AdjustBudget`, and 5-artefact
byte-identical determinism with M2 commands applied at day 0
through `apply_command_script`. New
`docs/milestone-2-result.md` exit report lists M2.1–M2.21
ledger, the architectural invariants every M3+ milestone must
preserve, and the explicitly-deferred items (Delayed /
Distorted outcomes, scheduler, RNG-based resistance, attempted-
command log, CLI script flag, runner-level rejection surface,
expanded authority fields, authority drift, faction reactions,
multi-country interaction, weighted formulas, …). README +
docs README + rfc README + memory all marked M2 closed. No new
gameplay, no save format change (still v10), no runner / CLI /
replay primitive / DataLoader / M1 system change, no new
`PlayerCommandKind`, no new CSV column. **M2 closes here.**
**M2.21 Command script driver helper — adds the library-only
free function
`commands::apply_command_script(state, vector<PlayerCommand>)`
on top of M2.20's `try_apply_pending`. Takes a one-shot script,
builds a local `CommandQueue`, dispatches through
`try_apply_pending`. Outcome reuses M2.20's
`ApplyWithReportOutcome` (no parallel struct). Routing inherited:
full drain → success + nullopt rejection; gate rejection →
success + populated record; non-execution failure (precondition
/ NaN delta / unknown policy / unknown category) → failure. Input
script vector not mutated. Library-only — no runner / RunOutcome
/ `main()` / CLI / replay / save schema change. Saves three
boilerplate lines at every REPL / scripted-test / agent-driver
call site;
**M2.20 Command rejection reporting — makes M2.18 / M2.19
order-execution rejections observable as structured data without
changing `apply_pending` semantics. New POD
`commands::RejectionRecord { kind, policy_id_code,
budget_category, compliance, threshold, resistance }`. New
wrapper `commands::ApplyWithReportOutcome { apply, rejection }`.
New free function `commands::try_apply_pending(state, queue)`
drains the queue exactly like `apply_pending` (same precondition,
same atomicity) but surfaces an order-execution rejection as
`Result::success` carrying the populated record. Non-execution
errors (precondition / NaN delta / unknown policy / unknown
category) still return `Result::failure`. Internal refactor
extracts `dispatch_one` in `commands.cpp`'s anonymous namespace
shared by both functions; `apply_pending`'s legacy rejection
error string is byte-identical via a `format_rejection_message`
helper. M2.18 / M2.19 tests pass unchanged. Drive-by:
refreshed stale `order_execution.cpp` comment that still said
"only EnactPolicy is evaluated in this PR" before M2.19 added
the AdjustBudget arm. No save format change (still v10); no
`apply_pending` signature change; no persistent attempted-
command log; no `state.logs` entry; no `RunOutcome` rejection
surface (M2.21 candidate); no DataLoader / replay primitive /
runner / CLI / M1 system change;
**M2.19 AdjustBudget execution gate — extends M2.18's
command-rejection shape to `AdjustBudget` with a category-aware
single-input gate. New constant
`kAdjustBudgetComplianceThreshold = 0.3` (matches `EnactPolicy`).
The `AdjustBudget` arm in `evaluate()` selects an authority input
by `command.budget_category`: `"military"` ⇒ `military_loyalty`,
every other category ⇒ `bureaucratic_compliance`. Then
`resistance = 1.0 - selected` and `status = (selected >= 0.3) ?
Accepted : Rejected`. `commands::apply_pending` gains a
pre-flight gate block structurally identical to M2.18's; rejected
`AdjustBudget` commands short-circuit with an error naming
`order_execution`, `rejected`, `AdjustBudget`, the offending
`budget_category`, selected compliance, and threshold. M2.3 / M2.4
mid-list-failure atomicity preserved. **No save format change**
(still v10); no `Delayed` / `Distorted` outcomes; no per-category
routing beyond the `military` branch; no weighted formula; no
scheduler; no RNG; no `state.logs` entry; no `RunOutcome`
rejection counter (M2.20 candidate); no DataLoader / policy /
replay primitive / runner / M1 system change;
**M2.18 EnactPolicy execution gate — first M2 sub-milestone where
a player command can be **rejected**. `order_execution` grows the
constant `kEnactPolicyComplianceThreshold = 0.3`, a `Rejected`
variant on `ExecutionStatus`, and a `resistance` field on
`OrderExecutionOutcome` (`1.0 - bureaucratic_compliance` for
`EnactPolicy`; `0.0` for kinds without a gate). `evaluate` now
branches on `command.kind`: `EnactPolicy` is `Accepted` when
`bureaucratic_compliance >= 0.3` and `Rejected` otherwise;
`AdjustBudget` stays unconditionally `Accepted`.
`commands::apply_pending` calls `evaluate` BEFORE the M2.3 policy
lookup for `EnactPolicy` commands; on `Rejected` it returns
`Result::failure` whose error names `order_execution`, `rejected`,
and the policy id_code. The rejected command stays at the head of
the queue and is NOT appended to `state.applied_commands`
(mirrors M2.3 / M2.4 mid-list-failure atomicity). Threshold 0.3
chosen so canonical default-0.5 scenarios accept unchanged. No
save format change (still v10); no `AdjustBudget` gate; no
`Delayed` / `Distorted` outcomes; no scheduler; no RNG; no
`state.logs` entry on rejection; no `RunOutcome` field counting
rejected commands; no DataLoader / policy effect / replay /
runner / M1 system change;
**M2.17 OrderExecutionSystem skeleton — first M2 system that reads
the M2.16 `government_authority` block. New module
`leviathan::systems::order_execution` with `OrderExecutionInputs`
(4-field snapshot of the actor country's authority ratios,
defaults 0.5), `ExecutionStatus` enum (only `Accepted` shipped;
`Rejected` / `Delayed` / `Distorted` reserved by name), and
`OrderExecutionOutcome { status, inputs }`. New free function
`evaluate(state, command) → Result<OrderExecutionOutcome>` mirrors
the M2.3 `apply_pending` preconditions (valid `player_country`
indexing into countries), snapshots authority into the outcome,
and always returns `Accepted`. **No caller wires the function in
yet** — `commands::apply_pending` is byte-identical with M2.5 /
M2.16. **No `resistance` field** in the outcome (deferred to the
same PR that introduces the formula). Pure read: no state
mutation, no logs, no RNG. CMake wires the new
`order_execution.cpp` into `leviathan_systems` and the new
`order_execution_test.cpp` into `leviathan_tests`. No save format
change;
**M2.16 GovernmentAuthorityState — first M2 gameplay-state
extension. New `core::GovernmentAuthorityState` POD with four
`[0, 1]` ratio fields defaulting to `0.5`
(`bureaucratic_compliance`, `military_loyalty`,
`intelligence_capability`, `media_control`) added to
`CountryState` as the `government_authority` field. **Save format
bumped v9 → v10**: block REQUIRED at save layer (`require_ratio`
per sub-field); DataLoader keeps it OPTIONAL in country JSON
(missing → defaults; present → validated). `diagnostics::compare_states`
extended to walk the four sub-fields under the
`countries[N].government_authority.*` JSON path. Drive-by: every
`"save_version": 9` JSON literal in tests bumped to `10`; every
existing v10 hand-built country JSON gained the new block.
**Data-only** — zero M1 systems read or write the new fields; M1
monthly pipeline and M2 command path are byte-identical. Deferred
from RFC-020 §3 with explicit documentation: `local_control`,
`legal_mandate`, `leader_prestige`, `party_organization`. **No
new gameplay logic, no new policy effect target, no new
`PlayerCommandKind`, no new CSV column, no scenario fixture
changes, no `state.logs` entry.**
**M2.9 Replay CLI error-path hardening — doc + tests PR with no
library behaviour change. Cements the **pre-`end_tick`** contract:
a `--replay` failure that occurs before `end_tick` is reached
(`save_system::load` failure, missing `--scenario`, `begin_tick`
rejection, `replay_with_time` failure on out-of-order log /
unknown policy id_code / malformed budget command / monthly
pipeline failure) writes ZERO output artefacts because `end_tick`
is the only function that touches disk. Failures INSIDE `end_tick`
itself are explicitly NOT covered — its five writes (save → log
→ summary CSV → countries CSV → factions CSV) are sequential and
non-transactional, so a mid-`end_tick` I/O failure can leave a
partial set on disk; atomic temp-file + rename is a deliberate
non-goal of this PR. `runner::run`'s doc comment gains an
explicit "M2.9 contract" block with that scope split; 3
regression tests pin missing source / out-of-order log / unknown
policy id_code with all five artefact paths wired and checked
absent. No save format change;
**M2.10 State comparison API — new
`systems::diagnostics::compare_states(a, b, opts)` free function
returning a list of `StateMismatch` entries (field_path +
detail). Walks gameplay-relevant fields field-by-field in
canonical order with floating-point tolerance (default 1e-9).
Library-only for now; consumers include replay-equivalence
tests and a future `--verify` CLI flag. Deliberately skips rng,
logs, policies, provinces, events, simulation_config — each
documented with rationale. No save format change; **M2.11
Replay verify CLI — new `--verify` boolean runner flag wires
M2.10 `compare_states` into the M2.8 `--replay` flow. After
`end_tick`, the runner compares the replayed state against the
loaded source save and populates
`RunOutcome::verify_mismatches`. `main()` prints
`Verify mismatches: N` plus one bullet per mismatch.
Informational only (exit code stays 0; strict mode deferred).
Reuses the already-loaded source save. No save format change;
**M2.12 Replay strict mode — new `--verify-strict` boolean
runner flag (requires `--verify`) makes `main()` exit
`EXIT_FAILURE` when M2.11 reports any mismatches. Full mismatch
list still printed first so CI logs capture every divergence.
`run()` semantics unchanged — strict is a `main()`-level exit
policy (library / CLI separation stays clean). Flag-chain
`--verify-strict → --verify → --replay` each with loud rejection
on missing dependency. No save format change; **M2.13 Verify
tolerance CLI — new `--verify-tolerance FLOAT` runner flag
(requires `--verify`) overrides M2.10's default 1e-9 tolerance
when calling `compare_states`. Parses via a new
`parse_nonneg_double` helper that rejects empty / garbage /
non-finite / negative inputs. Plumbed into `run()`'s replay
branch via `diagnostics::CompareOptions`. `main()` prints the
active tolerance when set. Completes the M2 replay-CLI family
(--replay / --verify / --verify-strict / --verify-tolerance).
No save format change.**

## Repository layout

```text
.
├── CMakeLists.txt        Top-level build
├── README.md             This file
├── rfc/                  Design RFCs — read these before changing scope
├── include/leviathan/    Public headers (core/ + systems/)
├── src/                  Simulation core + executable entry point
├── tests/                Unit / integration tests (doctest, per-module)
├── data/                 Game data (JSON): config/simulation.json,
│                         countries/{germany,france,japan}.json (M0.7-M1.3),
│                         factions/ger_*.json (M1.2),
│                         policies/*.json (M1.4),
│                         scenarios/1930_minimal.json (M1.11),
│                         scenarios/1930_with_start_policies.json (M1.13)
├── tools/                Dev / debug tools, currently empty
└── docs/                 Per-milestone design notes (m0-N-*.md) +
                          pr-drafts/ (PR write-ups)
```

## Requirements

- A C++17 compiler:
  - MSVC 19.20+ (Visual Studio 2019 or 2022), or
  - GCC 9+, or
  - Clang 10+
- CMake **3.16 or newer**
- A build tool that CMake can drive (Ninja, Make, MSBuild, Xcode, ...)
- Network access on the **first** configure: the build fetches
  [nlohmann/json](https://github.com/nlohmann/json) v3.11.3 (used by
  the data loader), and the test suite fetches
  [doctest](https://github.com/doctest/doctest) v2.4.11. Both go through
  CMake `FetchContent` (shallow clones, pinned tags). Subsequent
  configures reuse the cached clones in `build/_deps/`.

## Build

From the repo root:

```bash
# Configure (single-config generators)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build --config Debug
```

On Windows with Visual Studio, the equivalent is:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The main executable is produced at `build/bin/leviathan` (`leviathan.exe`
on Windows).

## Run a simulation (M0.9 headless runner)

```bash
# Show usage
./build/bin/Debug/leviathan --help

# 10-day smoke run (everything else falls back to defaults)
./build/bin/Debug/leviathan --days 10

# M1.11 - load the canonical 1930 world so the monthly pipeline
# actually has countries to tick:
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --seed 12345

# Pinned configuration matching the M0.9 spec example
./build/bin/Debug/leviathan \
    --config data/config/simulation.json \
    --days 365 \
    --seed 12345 \
    --output out/ \
    --save out/save.json \
    --log out/events.jsonl \
    --summary-csv out/summary.csv

# M1.14 - per-country diagnostic CSV: one row per country per snapshot
# point with gdp / stability / last_gdp_growth_rate etc., inspectable
# without round-tripping the save.
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --countries-csv out/countries.csv

# M1.16 - per-faction diagnostic CSV: one row per faction per snapshot
# point with support / influence / radicalism / loyalty / resources.
# Can be combined with --countries-csv and --summary-csv.
./build/bin/Debug/leviathan \
    --days 365 \
    --scenario data/scenarios/1930_minimal.json \
    --countries-csv out/countries.csv \
    --factions-csv  out/factions.csv

# M1.17 - 10-year soak (RFC-090 §1.17 acceptance criterion).
# Loads the canonical day-0-policies scenario and ticks 3652 days
# (1930-01-01 -> 1940-01-01). 120 monthly pipeline runs.
./build/bin/Debug/leviathan \
    --days     3652 \
    --seed     12345 \
    --scenario data/scenarios/1930_with_start_policies.json

# M2.1 - select a player country. Resolution runs after --scenario
# loads, so --player only makes sense when a scenario is loaded.
# The id_code must match a country in the manifest; bad id_codes
# fail loudly before any tick happens.
./build/bin/Debug/leviathan \
    --days     365 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --player   GER

# M2.8 - replay a previously-saved command log onto a fresh state.
# --replay requires --scenario (for the fresh baseline). When
# --player is unset, player_country is auto-inherited from the
# loaded save. The runner writes the replayed state to save.json
# under --output; diff against the source to verify equivalence.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --output   replay_out/

# M2.11 - same as above, but auto-compare the replayed state
# against the source via diagnostics::compare_states. Mismatches
# are printed to stdout; exit code stays 0 regardless.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --verify \
    --output   replay_out/

# M2.12 - same as above but make the process exit EXIT_FAILURE
# on any mismatch. The full mismatch list still prints first so
# CI logs see the divergence; useful as a build/replay regression
# gate.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/golden.json \
    --verify --verify-strict \
    --output   replay_out/

# M2.13 - loosen the verify tolerance for cumulative drift in
# long simulations. Default is 1e-9; pass any finite non-negative
# double via --verify-tolerance to override.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/golden.json \
    --verify --verify-strict --verify-tolerance 1e-6 \
    --output   replay_out/

# M2.14 - replay only up to a chosen date and save there. Entries
# with applied_on > target_date are skipped; after the truncated
# replay, the time system is advanced day-by-day until current_date
# equals target_date so the resulting save reflects exactly that
# day. Requires --replay.
./build/bin/Debug/leviathan \
    --days     0 \
    --scenario data/scenarios/1930_with_start_policies.json \
    --replay   path/to/source.json \
    --target-date 1930-06-15 \
    --output   replay_out/
```

Required flag: `--days`. Everything else has a default
(`--config data/config/simulation.json`, `--output out/`, `--seed`
falls back to the value in the config file, `--save` and `--log` live
under `--output`).

Outputs (paths can be overridden):

| File | Format | Source |
|------|--------|--------|
| `out/save.json`     | Pretty-printed save (M0.8)            | Round-trippable with `save_system::load` |
| `out/events.jsonl`  | One JSON object per line              | M0.6 logging exporter |
| `out/summary.csv`   | Per-snapshot CSV (M0.10, optional)    | `--summary-csv PATH` ; columns: `date,country_count,log_count,seed` |
| stdout              | Plain-text run summary                | start / end dates, log count, sanity-issue count, output paths |

**Determinism**: the same `--config` + `--days` + `--seed` produces
byte-identical `save.json`, `events.jsonl`, **and `summary.csv`**.
Pinned by tests in `tests/systems/runner_test.cpp`
(`byte-identical`, `summary-csv same-seed`).

The runner ticks an empty world (no countries, factions, policies)
in M0.9. Loading countries from a directory will land in a later PR.

## Test

```bash
ctest --test-dir build --output-on-failure
```

For multi-config generators (Visual Studio, Xcode):

```bash
ctest --test-dir build -C Debug --output-on-failure
```

As of M2.22 there are **627 doctest cases**. M0 contributed 179;
M1.1 added 9; M1.2 added 17; M1.3 added 9; M1.4 added 17; M1.5
added 24; M1.6 added 17; M1.7 added 16; M1.8 added 19; M1.9 added
11; M1.10 added 9; M1.11 added 25; M1.12 added 15; M1.13 added 15;
M1.14 added 17; M1.15 added 15; M1.16 added 18; M1.17 added 3
end-to-end integration tests; M2.1 added 17; M2.2 added 10; M2.3
added 8; M2.4 added 13; M2.5 added 11; M2.6 added 9; M2.7 added
10; M2.8 added 7; M2.10 added 12; M2.11 added 5; M2.12 added 5;
M2.13 added 8; M2.9 added 3; M2.14 added 9; M2.16 added 13;
M2.17 added 10; M2.18 added 10; M2.19 added 11; M2.20 added 10;
M2.21 added 8; M2.22 adds 3 end-to-end integration tests in
`m2_end_to_end_test.cpp` closing M2:
(1) command script + replay reproduces source via
`compare_states` (zero mismatches across every gameplay-
relevant field after replay_with_time on the source's
applied_commands);
(2) gate atomicity across `EnactPolicy` and `AdjustBudget`
(low bureaucratic_compliance + high military_loyalty: military
adjustment lands, EnactPolicy rejected, trailing welfare
AdjustBudget unreached; state / queue / applied_commands all
consistent with per-command atomicity);
(3) 5-artefact byte-identical determinism with M2 commands
(same script + same setup produces matching save.json /
events.jsonl / summary.csv / countries.csv / factions.csv
across two independent temp dirs, extending M1.17's
determinism contract through the command path). Previously
M2.21 adds 8 covering the new
`commands::apply_command_script` helper: empty script success;
full success applies both `EnactPolicy("raise_taxes")` and
`AdjustBudget("military", +0.02)` in order and logs both;
EnactPolicy rejected at compliance 0.1 surfaces structured
`RejectionRecord{kind, policy_id_code, compliance=0.1,
threshold=0.3}`; AdjustBudget(military) rejected at
military_loyalty 0.05 records `military_loyalty` as compliance
(M2.19 selected-input contract); mid-script rejection preserves
prior `AdjustBudget(military)` apply+log while trailing
`AdjustBudget(welfare)` does NOT run (helper does not surface
remaining commands); unknown policy id_code still returns
`Result::failure`; invalid `player_country` returns failure
naming `player_country`; input vector survives the call
unmutated element-by-element. Previously M2.20 added 10
covering the new `try_apply_pending` structured-rejection
surface: full drain returns success + nullopt rejection; EnactPolicy
rejection populates `RejectionRecord{kind, policy_id_code,
compliance=0.1, threshold=0.3, resistance=0.9}` with full
atomicity asserted (tax burden / queue head / applied_commands
all unchanged); AdjustBudget(military) rejection records
`military_loyalty` as `compliance` (not bureaucratic);
AdjustBudget(welfare) rejection records `bureaucratic_compliance`
even with high military_loyalty (selected-input contract);
unknown policy / unknown budget_category / non-finite delta all
still return `Result::failure`; invalid `player_country` returns
failure that names `try_apply_pending`; mid-list rejection
preserves a prior successful `AdjustBudget(military)` and leaves
the rejected `EnactPolicy` at the queue head with the trailing
command still behind it; `apply_pending` rejection still returns
`Result::failure` (backward-compat regression). Previously M2.19
adds 11 covering the new
category-aware AdjustBudget gate: 7 in `order_execution_test.cpp`
(military category at threshold 0.3 accepts with resistance 0.7;
0.299 rejects; military category ignores high bureaucratic
compliance when military_loyalty is low; non-military categories
— iterates over administration/education/welfare/intelligence/
infrastructure/industry — reject when bureaucratic_compliance <
0.3 regardless of military_loyalty; non-military accepts at high
bureaucratic_compliance with low military_loyalty; default 0.5
authority accepts both military and non-military categories;
rejected path is non-mutating) and 4 in `commands_test.cpp`
(AdjustBudget(military) rejected when military_loyalty < 0.3 with
full error contents; AdjustBudget(welfare) rejected when
bureaucratic_compliance < 0.3 even with high military_loyalty;
AdjustBudget(military) still accepted at military_loyalty 0.8
when bureaucratic_compliance 0.05; mid-list rejection with prior
AdjustBudget(military) applied and trailing EnactPolicy still
queued). Drive-by: M2.18's "EnactPolicy and AdjustBudget identical
inputs" assertion updated (adjust.resistance now reflects
military_loyalty, not 0.0); M2.18's "AdjustBudget bypasses" /
"unaffected" tests refreshed to reflect the M2.19 routing.
Previously M2.18 added 10 covering the new EnactPolicy gate:
6 in `order_execution_test.cpp` (compliance at threshold 0.3
accepts with resistance 0.7; compliance 0.299 rejects;
`resistance == 1.0 - compliance` across spot-check inputs;
default 0.5 compliance still accepts; `AdjustBudget` bypasses the
gate at compliance 0.01; rejected path leaves state byte-
identical) and 4 in `commands_test.cpp` (default 0.5 EnactPolicy
still drains the queue and logs; compliance < 0.3 rejected with
`order_execution` + `rejected` + policy id_code in the error,
state unchanged, queue head intact, applied_commands untouched;
mid-list rejection stops with prior AdjustBudget already applied
and logged while trailing EnactPolicy stays queued;
`AdjustBudget` unaffected by 0.05 compliance). Drive-by updates:
"default OrderExecutionOutcome" now also pins `resistance == 0.0`
and the "EnactPolicy and AdjustBudget" kind-comparison test
splits into "same inputs, different resistance". Previously
M2.17 added 10 covering the new `order_execution::evaluate`
skeleton (3 precondition cases — no player_country, out of range,
empty countries; 3 success-path cases — Accepted returned, inputs
mirror the actor's authority block one-for-one, function reads
the *selected* country rather than `countries[0]`; 3
non-mutation / determinism / kind-independence cases — state is
byte-identical after the call, `EnactPolicy` and `AdjustBudget`
produce identical outcomes since the skeleton ignores
`command.kind`, repeated calls return identical outcomes; 1
default-construction baseline pinning all four `inputs` ratios at
0.5 and `status == Accepted`). Previously M2.16 added 13 across
the v10 save-format bump and the new
`GovernmentAuthorityState` plumbing: 1 game_state baseline (default
0.5 across all four sub-fields), 5 data_loader (missing block →
defaults; present → loaded; wrong-type rejected; missing sub-field
rejected; out-of-range rejected), 6 save_system (serialize emits
the block; round-trip preserves arbitrary values; v9 save rejected
loudly; v10 country missing block rejected; v10 block missing
sub-key rejected; v10 out-of-range value rejected), and 1
diagnostics compare_states test pinning the per-sub-field paths
under `countries[0].government_authority.*`. Previously M2.14
added 9 covering `--target-date`:
5 parse cases (plumbed; default nullopt; missing value rejected;
malformed date `"1930-13-01"` rejected with flag name + value in
error; without `--replay` rejected with both flag names in error)
and 4 run cases (target past log advances time system and saves
at target; target equal to last entry replays full log with no
extra step; target earlier than a log entry truncates the log to
entries with `applied_on <= target_date`; target before scenario
start rejected with `--target-date` + the bad date +
"scenario start" in error, and `check_no_artifacts` confirms the
M2.9 pre-`end_tick` no-artefact contract). The dated-log helper
`build_source_with_dated_log` hand-splices `AppliedPlayerCommand`
entries at chosen monotonic dates so truncation can be exercised
without going through `apply_pending`. Previously M2.9 added 3
covering the no-artefact contract under `--replay`: missing
source file fails with "--replay" in error and all five artefact
paths absent; out-of-order log fails with "out-of-order" in error
and all five paths absent; unknown policy id_code fails with
`"no_such_policy_id_code"` in error and all five paths absent.
Each test wires save / events / summary CSV / countries CSV /
factions CSV inside a `TempDir` and uses a shared
`wire_all_artifacts` + `check_no_artifacts` helper. Previously
M2.13 added 8 covering `--verify-tolerance`: parse plumbed with
value preserved; default nullopt; missing value rejected;
non-numeric (`"abc"`) rejected with "floating-point" in error;
negative rejected with `">= 0"` in error; without `--verify`
rejected with both flag names; end-to-end: loose tolerance
(`1e-2`) absorbs a `1e-3` `gdp` tweak with no mismatch on that
path; tight tolerance (`1e-6`) catches the same tweak.
Previously M2.12 added 5 covering `--verify-strict`: parse plumbed (with `--verify`),
defaults false, `--verify-strict` without `--verify` rejected
with both flag names; run with strict + matching source succeeds
empty mismatches; run with strict + tweaked source still
succeeds at `run()` level (exit code is `main()`'s concern) but
reports mismatches. Previously M2.11 added 5 covering the
`--verify` CLI flag: parse_args plumbed (with `--replay`),
defaults false, `--verify` without `--replay` rejected with both
flag names in the error; full-replay with matching source
reports zero mismatches; full-replay with manually tweaked
source (`countries[0].legal_tax_burden = 0.99`) detects the
divergence at the documented path. Previously M2.10 added 12
covering the new `compare_states` API: two empty match; identical seeded match;
`current_date` diff with both date strings in detail;
`player_country` diff; country count → `countries.size()`
mismatch; gdp diff on country[0] → `countries[0].gdp` path;
tolerance — within (silent) and outside (reported);
`active_policies` size diff caught with the array path;
`applied_commands` size diff caught; multiple mismatches
collected in canonical order; custom `CompareOptions` tolerance
respected. Previously M2.8 added 7 covering the `--replay` CLI: parse_args plumbed,
missing value rejected, default unset; run --replay without
--scenario rejected; run --replay with single EnactPolicy
reproduces source's `legal_tax_burden` + log; --player auto-
inherits from loaded save when unset; --replay of an empty-log
save replays zero commands. Previously M2.7 added 10 covering
"M2.7: replay_with_time": empty log no-op,
command at start_date (0 advance), command 5 days later (5
advance), command past month boundary (45 days + 1 monthly_tick),
multiple commands at different dates, out-of-order rejected,
ctrl not started rejected, no player_country rejected, non-empty
`applied_commands` rejected, **full equivalence vs original
simulation** (drive source via step + apply, replay onto fresh
target, verify every observable matches including gdp /
stability / last_gdp_growth_rate). Previously M2.6 added 9 in
`tests/systems/commands_test.cpp`: empty log replays cleanly;
single EnactPolicy / single AdjustBudget / mixed-kind log all
replay state + re-log correctly; replayed log mirrors source
byte-equivalent across two dates; `current_date` forced to last
entry's `applied_on` (prototype limit pinned); unknown
policy_id_code mid-list stops with `replay[N]` in the error;
no `player_country` rejects before any replay; non-empty
`applied_commands` rejects the precondition. Previously M2.5
added 11 covering the AdjustBudget kind: 8 commands_test cases (delta mutates target field; negative
delta shrinks; overshoot clamps to 1.0; undershoot clamps to 0.0;
unknown `budget_category` rejected with no mutation; non-finite
`budget_delta` rejected; AdjustBudget log entry carries correct
category + delta; mixed-kind queue applies both in insertion order
with both kinds logged) and 3 save_system_test cases (AdjustBudget
log entry round-trips category + delta; v9 entry missing
`budget_category` rejected; v9 entry missing `budget_delta`
rejected). Previously M2.4 added 13 covering player command log: 5
commands_test cases (successful enact appends one entry; multiple
successes append in insertion order; failed command does NOT log
(per-command atomicity); `applied_on` captures `state.current_date`
at apply time, distinct dates pinned across two `apply_pending`
calls; precondition failure leaves log untouched); 8 save_system
cases (rejects an old v8 save loudly; serialize emits
`"applied_commands": []`; populated round-trip preserves dates +
policy_id_code; v9 missing `applied_commands` rejected; entry
with malformed `applied_on` `"1930-02-30"` rejected; entry with
unknown `kind` `"SomethingBogus"` rejected; entry missing
`policy_id_code` rejected; entry missing `command` sub-object
rejected). Plus an extension of the `game_state_test` baseline
check to assert `applied_commands.empty()`. Save schema is now
v9. Previously M2.3 added 8 in
`tests/systems/commands_test.cpp`: empty queue is a no-op success;
single `EnactPolicy` drains the queue and applies the policy;
successful enact chains into `active_policies` (M1.15 integration:
`expires_on = 1930-03-02` for canonical `raise_taxes` 60-day enact
from `1930-01-01`); multiple commands apply in insertion order;
no `player_country` selected rejected with queue untouched;
`player_country` out-of-range rejected; unknown `policy_id_code`
mid-queue stops at failed cmd (first cmd applied, failed stays at
head, trailing cmd still queued); policy with bad effect target
stops via M1.5 pre-flight rejection. Previously M2.2 added 10
covering pause / resume / step primitives: 6 misuse + counter
cases (`begin_tick` double-begin rejected; `step_one_day` before
begin / after end rejected; `end_tick` before begin / double end
rejected; controller counters `start_date / days_stepped /
monthly_ticks / started / ended` reflect actual lifecycle); 2
equivalence cases (`begin/step×N/end` byte-identical to
`run_state(days=N)` across save + events + all 3 CSVs;
`begin/step×15/step×16/end` byte-identical to
`run_state(days=31)`); 2 drive-by regression cases from the PR
#29 nit (bad `--player` empty-world / unknown id_code both leave
no `save.json` / `events.jsonl` on disk). Previously M2.1 added
17 covering player country selection: 9 save_system cases (rejects an old v7 save loudly,
serialize emits `"player_country": -1`, save+load round-trips
both `-1` and a valid index, v8 missing `player_country` rejected,
non-integer rejected, `< -1` rejected, out-of-range rejected,
above `INT_MAX` rejected); 8 runner cases (`--player` plumbed /
value-missing / default-unset; on empty world rejected with
id_code in message; unknown id_code rejected; `--player GER` →
`player_country: 0`; `--player FRA` → `player_country: 1`; no
`--player` → `player_country: -1`). M1.17 added the 3
end-to-end integration tests in `tests/integration/m1_end_to_end_test.cpp`:
(1) 1-year run that loads the canonical day-0-policies scenario via
the runner, asserts the `active_policies` round-trip (`expires_on
= 1930-03-02` for `raise_taxes`, `1930-01-31` for
`increase_military_budget`), checks `monthly_ticks == 12` and CSV
row counts (1+12+1 snapshot points × N entities); (2) 10-year soak
run pinning RFC-090 §1.17 (3652 days, 120 monthly pipelines, zero
sanity issues, every country's gdp / stability / legitimacy /
last_gdp_growth_rate finite and clamped); (3) 5-artefact
byte-identical determinism check (save / events / summary CSV /
countries CSV / factions CSV) over the 1-year scenario. M1.16
previously added 18 covering the **per-faction CSV**: 9 diagnostics cases (faction_snapshot reads
every field, invalid id rejected with bad index in message,
default-id rejected, empty state rejects any index, header
byte-exact, row well-formed with 8 commas / 9 columns, negative
resources survives format, byte-identical for same row twice,
snapshot doesn't mutate state); 9 runner cases (`--factions-csv`
plumbed/value-missing/default-unset, no-flag no file, empty state
header-only, canonical scenario emits `9 rows = 3 factions × 3
snapshots` for a 31-day run, byte-identical determinism, summary
CSV unchanged when `--factions-csv` is added (M0.10 contract
regression), countries CSV unchanged when `--factions-csv` is
added (M1.14 contract regression)). Save schema remains v7 (no
new persistent state). Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "faction_snapshot|factions-csv"`
runs just the M1.16 cases.

Previously: M1.15 added 15 covering the **policy duration
tracking** save state plus the new runtime cap on `duration_days`:
9 policy_system cases (successful enact appends one `ActivePolicy`,
pre-flight failure appends nothing, `duration_days == 0` same-day
expiry, multiple enacts preserve insertion order, faction-zero-
match enactment still records, only the actor's list grows,
negative `duration_days` rejected, `duration_days >
kMaxTrackedPolicyDurationDays` rejected, the cap boundary value
itself is accepted); 6 save_system cases (rejects an old v6 save
loudly, serialize emits `"active_policies": []`, save+load
round-trips populated entries with their `expires_on` dates, v7
country missing `active_policies` rejected, entry missing
`policy_id_code` rejected, entry with malformed `expires_on`
rejected); plus an extension to the M1.13 day-0-enactment scenario
test that pins `expires_on = 1930-03-02` for the canonical
`raise_taxes` (60-day) enactment.
Save schema is now v7. Each `TEST_CASE` is registered with CTest
individually, so e.g. `ctest -R "M1.15"` runs just the M1.15 cases.

## Build options

| Option                   | Default                       | Purpose                              |
|--------------------------|-------------------------------|--------------------------------------|
| `LEVIATHAN_BUILD_TESTS`  | `ON` when top-level, else `OFF` | Build and register the test suite. |

Override with `-DLEVIATHAN_BUILD_TESTS=OFF` if you only want the binary.

## Contributing / roadmap

Read the RFCs in order before changing scope:

1. `rfc/README.md`
2. `rfc/RFC-000-overview.md`
3. `rfc/RFC-001-development-contract.md`
4. `rfc/RFC-010-prototype-v0_1.md`
5. `rfc/RFC-060-technical-architecture.md`
6. `rfc/RFC-070-data-formats.md`
7. `rfc/RFC-080-research-formulas.md`
8. `rfc/RFC-090-roadmap.md`

Each Milestone 0 sub-milestone (M0.1 – M0.11) ships as its own PR to
`main`. Do not bundle multiple sub-milestones in one PR.

Per-milestone design notes (locked-in schemas, error formats,
architectural rules) live under `docs/m0-N-*.md`; PR write-ups live
under `docs/pr-drafts/`.
