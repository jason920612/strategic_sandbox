# Milestone 2 — Player-operation prototype: exit report

**Status: closed.**

M2 set out to make the player a first-class actor in the
simulation: a country selection, a way to issue commands, a log
of what the player did, a deterministic replay path for those
commands, a set of authority dimensions that decide whether
commands actually land, and a structured surface for the
inevitable rejections. Twenty-one sub-milestones across two
distinct phases delivered that.

## 1. What M2 ships

### Phase A — Replay / verify / save (M2.1 – M2.14, plus the
backfilled M2.9)

| Sub-milestone | Title | Highlights |
|---------------|-------|------------|
| **M2.1**  | Player country selection | `GameState::player_country` (`CountryId`, default `invalid()`). New `--player COUNTRY_IDCODE` runner flag resolved after scenario load. **Save format v7 → v8.** No M1 system reads the field. |
| **M2.2**  | Pause / resume / step primitives | `runner::TickController` runtime struct (not saved). `begin_tick` / `step_one_day` / `end_tick` extracted from `run_state`. M1.17's 5-artefact byte-identical determinism contract preserved. |
| **M2.3**  | Player command queue | `core::PlayerCommand{kind, policy_id_code}` + `systems::commands::{CommandQueue, apply_pending}`. Driver-owned queue (not in GameState). Non-atomic across the list (first failure stops). |
| **M2.4**  | Player command log | `core::AppliedPlayerCommand{applied_on, command}` + `GameState::applied_commands`. `apply_pending` appends one entry per successful per-command dispatch. **Save format v8 → v9.** |
| **M2.5**  | AdjustBudget player command | `PlayerCommandKind::AdjustBudget` variant + `budget_category` / `budget_delta` payload fields. Per-kind JSON shape in save. No save format bump. |
| **M2.6**  | Replay applied command log prototype | `systems::commands::replay(state, log)` free function. Per-entry: force `current_date = applied_on`, 1-element queue, `apply_pending`. No time advancement; scenario pre-load required. |
| **M2.7**  | Replay with time-system advancement | `commands::replay_with_time(state, opts, ctrl, log)`. Interleaves `step_one_day` with command dispatch. Monotonic non-decreasing dates required. Killer equivalence test pins behaviour preservation. |
| **M2.8**  | Replay CLI harness | `--replay PATH` runner flag wires M2.7 into `run()`. Auto-inherits `player_country` from the loaded save when `--player` is unset. CLI does NOT auto-compare. |
| **M2.9**  | Replay CLI error-path hardening (backfilled) | Doc + 3 regression tests cementing the **pre-`end_tick`** no-artefact contract for `--replay`-mode failures. Failures inside `end_tick` itself are explicitly NOT covered (its five writes are sequential and non-transactional). |
| **M2.10** | State comparison API | `systems::diagnostics::compare_states(a, b, opts)` + `StateMismatch` + `CompareOptions`. Walks gameplay-relevant fields with floating-point tolerance. Skips rng, logs, policies, provinces, events. |
| **M2.11** | Replay verify CLI | `--verify` flag wires `compare_states` into `--replay`. Populates `RunOutcome::verify_mismatches`. Informational only (exit code stays 0). |
| **M2.12** | Replay strict mode | `--verify-strict` makes `main()` exit `EXIT_FAILURE` on any mismatch. `run()` unchanged — strict mode is a `main()`-level policy. |
| **M2.13** | Verify tolerance CLI | `--verify-tolerance FLOAT` overrides M2.10's default `1e-9`. New `parse_nonneg_double` exception-free helper. |
| **M2.14** | Replay target-date CLI | `--target-date YYYY-MM-DD` scopes the replay flow: log truncation + post-replay `step_one_day` extension to the target day. |

### Phase B — Government authority + execution gates (M2.16 –
M2.21)

| Sub-milestone | Title | Highlights |
|---------------|-------|------------|
| **M2.16** | GovernmentAuthorityState | New POD with four `[0, 1]` ratio fields (`bureaucratic_compliance`, `military_loyalty`, `intelligence_capability`, `media_control`) added to `CountryState`. **Save format v9 → v10.** DataLoader treats the block as optional in country JSON. **Data-only**: no system reads or writes the fields. |
| **M2.17** | OrderExecutionSystem skeleton | `systems::order_execution` module with `OrderExecutionInputs`, `ExecutionStatus` (only `Accepted` variant), `OrderExecutionOutcome`, and `evaluate(state, command)`. Pure read. No caller wires it in yet. |
| **M2.18** | EnactPolicy execution gate | First command-rejecting sub-milestone. `kEnactPolicyComplianceThreshold = 0.3`. `ExecutionStatus::Rejected` variant. `resistance` field. `commands::apply_pending` calls `evaluate` BEFORE the M2.3 policy lookup. M2.3 / M2.4 mid-list-failure atomicity preserved. |
| **M2.19** | AdjustBudget execution gate | Category-aware single-input formula: `"military"` ⇒ `military_loyalty`, every other category ⇒ `bureaucratic_compliance`. `kAdjustBudgetComplianceThreshold = 0.3`. Same atomicity rules. |
| **M2.20** | Command rejection reporting | `commands::RejectionRecord` + `ApplyWithReportOutcome` + `try_apply_pending(state, queue)`. Order-execution rejection surfaces as `Result::success` with structured record; non-execution errors still `Result::failure`. `apply_pending` byte-identical surface preserved via internal `dispatch_one` + `format_rejection_message`. |
| **M2.21** | Command script driver helper | `commands::apply_command_script(state, vector<PlayerCommand>)` wraps `try_apply_pending` for one-shot scripts. Library-only; no runner / RunOutcome / CLI / replay change. |

### Phase C — Exit (M2.22)

| Sub-milestone | Title | Highlights |
|---------------|-------|------------|
| **M2.22** | M2 exit / integration tests | This sub-milestone. Three end-to-end tests (`tests/integration/m2_end_to_end_test.cpp`): command script + replay + `compare_states` equivalence; gate atomicity across `EnactPolicy` and `AdjustBudget`; 5-artefact byte-identical determinism with M2 commands applied. New `docs/milestone-2-result.md` exit report (this file). M2 closes. |

## 2. Architectural invariants every M3+ milestone must preserve

These are the rules M2 added on top of the M1 invariants (see
`milestone-1-result.md`). Future milestones must not silently
break them.

- **GameState stays a passive container.** No methods, no
  setters, no methods on the new `GovernmentAuthorityState` or
  `OrderExecutionOutcome` types.
- **Systems are free functions.** `order_execution::evaluate`
  is a pure read of `state` + `command`. A future PR that
  needs RNG or mutation must rename, not overload.
- **Player commands flow through `commands::apply_pending` (or
  its `try_apply_pending` / `apply_command_script`
  alternatives).** Direct mutation of `state.applied_commands`
  by a caller is unsupported — it would break replay equivalence
  and the M2.3 atomicity rule.
- **Per-command atomicity covers state, queue, and log.** A
  rejected or otherwise-failed command does not mutate any of
  the three. M2.18 / M2.19 / M2.20 / M2.21 all inherit this from
  M2.3.
- **`apply_pending`'s rejection-as-`Result::failure` surface is
  load-bearing.** `replay_with_time` (M2.7) and every existing
  runner / replay caller depends on it. New helpers should sit
  beside `apply_pending`, not replace its semantics.
- **Save format v10 is the new floor.** Any subsequent persistent
  state addition bumps it; M0.8's strict-equality + version-
  history rule applies as it always did.
- **Determinism contract still holds for 5 artefacts.** Even
  with command scripts applied, same inputs ⇒ byte-identical
  save.json / events.jsonl / summary.csv / countries.csv /
  factions.csv. Pinned by `m2_end_to_end_test`'s third case.
- **Replay reproduces source state under default authority.**
  Every save written before M2.18 had its commands applied
  under default-0.5 authority; the M2.18 / M2.19 gates accept
  at default 0.5, so re-running through `apply_pending` in
  `replay_with_time` re-Accepts in the same order.
- **`order_execution::evaluate` is the single source of truth
  for command gating.** `commands::apply_pending` and
  `commands::try_apply_pending` both call it; no caller should
  re-implement gate logic.
- **Threshold constants live in `order_execution.hpp`.** Future
  formula changes update the constants there; no scenario hook,
  no runtime tuning, no per-policy override (yet).
- **Government authority fields stay `[0, 1]` ratios.** The
  M2.16 block follows the same range discipline as M1.1 numeric
  fields. Out-of-range values are rejected at every load surface
  that actually validates them (DataLoader if the block is
  present; SaveSystem unconditionally).

## 3. Deferred items (NOT shipped in M2)

These were considered, surfaced in design notes, and
deliberately scoped out of M2. They are candidates for M3+ or
for targeted follow-ups when a real consumer appears.

### Player / command surface
- **CLI script flag / command script file input.** No
  `--script PATH` flag. `apply_command_script` is library-only.
- **Runner-level rejection surface.** No `RunOutcome::last_rejection`
  or rejection counter; `main()` doesn't print a "commands
  rejected" line. The structured record lives in
  `commands::ApplyWithReportOutcome` only.
- **Persistent attempted-command log.** Only successful
  commands land in `state.applied_commands`. Rejected attempts
  are not persisted in any form.
- **More `PlayerCommandKind` variants.** Two kinds shipped
  (`EnactPolicy`, `AdjustBudget`). The RFC-020 §2 catalogue
  lists more (e.g. `ChangeTaxBurden`, `ToggleMartialLaw`,
  `LaunchPropagandaCampaign`); each lands when a real gameplay
  consumer needs it.

### Execution / gate
- **`Delayed` and `Distorted` outcomes.** `ExecutionStatus` is
  shipped with only `Accepted` and `Rejected`. The other two
  RFC-020 §2 categories stay reserved by name in the enum
  comment.
- **Scheduler / pending-execution queue.** Rejected commands
  do not auto-retry; delayed commands are not multi-day-spread.
- **RNG / probabilistic execution resistance.** All M2 gates
  are deterministic threshold checks. No `Temperature`, no
  softmax, no random rejection.
- **Events reacting to rejected commands.** No "the
  bureaucracy publicly defied your command" event.
- **Weighted multi-input formulas.** Each (kind, category) pair
  reads exactly one authority input. Composite formulas wait
  for actual gameplay evidence about which weights matter.
- **Per-category routing beyond `military` ⇒ `military_loyalty`.**
  `intelligence` ⇒ `intelligence_capability`, `welfare` ⇒ a
  future `welfare_capacity`, etc. all wait for their own PR.

### Government authority
- **Expanded authority fields.** Only 4 of the RFC-020 §3
  surface shipped. Specifically deferred: a distinct
  `local_control` (separate from existing `central_control`),
  `legal_mandate`, `leader_prestige`, `party_organization`.
- **Authority drift over time.** No system writes the four
  fields. They stay at whatever scenario / save authored.

### Replay / verify
- **Relative tolerance in `CompareOptions`.** M2.10 / M2.13
  support absolute tolerance only. Relative tolerance for
  large-magnitude fields (e.g. cumulative GDP after long
  simulations) is a future PR.
- **Atomic `end_tick` writes.** M2.9 explicitly carved out
  `end_tick`-internal failures from the no-artefact contract
  because its five writes (save → log → CSV ×3) are sequential.
  Temp-file + rename is a deferrable follow-up.

### Faction / interest-group reactions
- **Faction reactions to player orders.** No faction state
  reads or writes any authority field. `react` still only
  drifts toward `country.stability` / `country.legitimacy`.
- **Interest-group bonus / penalty on rejection.** Not modelled.

### UI / driver
- **REPL / interactive command interface.** No script.
- **Headless agent driver.** The library helpers exist
  (`apply_command_script` etc.); the agent loop does not.

## 4. Recommendations for M3+

The natural follow-ons, with no commitment that they are M3:

1. **`Delayed` outcomes.** Promote `ExecutionStatus::Delayed`
   from "reserved by name" to a real value. Introduce a
   pending-execution queue keyed by an `applied_on` future
   date. Requires save schema work (the queue is persistent) so
   it's a v10 → v11 bump.
2. **Runner-level rejection surface.** With the
   `RejectionRecord` already defined, plumb it into
   `RunOutcome` + a stdout line. Save-neutral.
3. **CLI script entry point.** `--script PATH` reading a JSON
   command list, dispatching through `apply_command_script`,
   surfacing the structured rejection in `RunOutcome` / stdout.
   Composes with `--player` and `--scenario`.
4. **Authority drift system.** Monthly authority updates based
   on `react` outputs / stability / corruption. First real
   consumer of the M2.16 fields.
5. **Faction reaction to rejected commands.** Per-faction
   support / loyalty changes when their domain's command is
   accepted vs rejected. Needs M3-scale design — out of
   M2-style stripped-formula scope.
6. **Multi-country interaction.** All M2 work was single-actor.
   Multi-country command scripts, foreign interference, alliance
   coordination, etc., live beyond M2.

## 5. Test surface at M2 close

- **627 doctest cases** total (up from 595 at the start of
  Phase B, up from 562 at the start of M2.16).
- **3 integration tests** in `m2_end_to_end_test.cpp` pin the
  player-operation surface:
  1. `command script + replay reproduces source via
     compare_states` — every gameplay-relevant field in
     `compare_states` agrees between a source state and a
     target state replayed from its `applied_commands` log.
  2. `gate atomicity across EnactPolicy and AdjustBudget` — a
     mixed script with one applied + one rejected + one
     unreached entry leaves state exactly where it should.
  3. `same script + same setup produces byte-identical 5
     artefacts` — M1.17's determinism contract extended through
     a 31-day run that applies commands at day 0.
- The M1.17 integration tests (1-year scenario, 10-year soak,
  5-artefact determinism) still pass unchanged.

## 6. Cross-links

- M1 exit report (`milestone-1-result.md`) — the floor M2
  built on.
- RFC-020 (`../rfc/RFC-020-politics-internal.md`) — the
  long-term internal-politics design M2 ships a first slice of.
- RFC-090 (`../rfc/RFC-090-roadmap.md`) — milestone roadmap.
- Per-milestone design notes for M2.1 – M2.21 in this directory.

## 7. M2 closes here

Subsequent work resumes under M3 or as targeted follow-up
sub-milestones. The M-pacing rule (one sub-milestone per PR,
no overlap) carries forward unchanged.
