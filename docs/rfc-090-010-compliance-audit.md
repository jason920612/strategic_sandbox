# RFC-090 / RFC-010 Compliance Audit

Companion to issue #105
(*"RFC audit: closed milestones drift from RFC-090 roadmap scope"*).

Docs-only governance alignment. Adopts issue #105's
**Option B** path: clarify the docs / roadmap language now,
keep missing original RFC scope as an explicit backlog, do
not rewrite code history, do not pretend original RFC
acceptance was complete. Option A items (actually
implementing the missing RFC scope) may be picked up later
as explicit follow-up milestones / PRs.

This document is the **single canonical reference** for what
the current implementation does and does not satisfy
relative to `rfc/RFC-090-roadmap.md` and
`rfc/RFC-010-prototype-v0_1.md`. Future milestone close-out
docs cross-link here rather than re-litigating the gap.

## 1. Status

Current implementation milestones **M0 – M5 are closed as
implementation milestones**. M6 is in progress at **M6.9**
(`event_firer` now composes `information_accuracy` +
`reported_value` + `bias_noise` to emit `visible_report` +
distortion numerics on every `event_fired` LogEntry — see
§1.4 below; RFC-060 §3 `EventLogEntry { publicText;
debugTruth }` is now complete: M6.8 shipped `debugTruth`,
M6.9 ships `publicText`). Predecessor: **M6.8** (debug
mode reveals truth — see §1.3); before that, the post-M6.7
hardening sweep applied `feedback_no_silent_degradation`
project-wide and migrated ratio-target `add` from linear to
asymptotic — see §1.2.
The `information_accuracy::compute_for_country` body now
implements an RFC-080 §8 subset:

- M6.6 (RFC-090 §6.6 "加入情報預算影響") shipped the
  intelligence-budget baseline
  `0.4 + 0.6 × (0.7 × intelligence_capability + 0.3 × budget.intelligence)`.
- M6.7 (RFC-090 §6.7 "加入腐敗影響") layers the RFC-080 §8
  `-Corruption` term on top:
  `accuracy = m6_6_baseline - 0.4 × corruption`.

Function-level range is now **`[0.0, 1.0]`** (M6.6's
`[0.4, 1.0]` widened by the corruption subtraction).
`kMinInformationAccuracy = 0.4` remains as **the M6.6
contribution floor**, not the M6.7 lower bound — the
corruption subtraction can push the effective total below it.
The whole `compute_for_country` body strictly validates
inputs per `feedback_no_silent_degradation`: each of
`government_authority.intelligence_capability`,
`budget.intelligence`, `corruption` must be a finite ratio in
`[0, 1]`, otherwise `Result::failure` (naming country
`id_code` + field + numeric value). Helper still has no
production caller — RFC-090 §6.9 will be the first
downstream consumer.

**Issue #112 strict-RFC corrective PR** is the final step in
the compliance recovery sequence: RCR-1 (PR #107) shipped the
data layer + helpers; issue #108 fix (PR #109) wired the
helpers into the monthly tick but with a first-policy stub;
issue #110 corrective replaced that stub with a deterministic
scorer AND wired the event-engine helpers — but the reviewer
audit (issue #112) found the new wiring still re-interpreted
RFC text in three places (fire-all-matched instead of "事件
抽選", unconditional followups instead of "條件連鎖", options[0]
default called "player choices complete"). Issue #112 replaces
those carve-outs with the literal RFC semantics:

- **Per-country / per-category weighted-random draw** using
  `state.rng`. ONE event fires per (country, category) per
  tick; same template may fire for multiple countries
  independently. Per-country isolation: `country.*` and
  `interest_group.*` triggers evaluate strictly within the
  drawing country's scope.
- **Recursive conditional followup chain**. Each followup
  must satisfy its OWN triggers AFTER the parent applies;
  if multiple followups match, weighted draw picks one;
  recursion runs depth-N up to `kMaxFollowupDepth=5` with a
  visited-set cycle guard.
- **Author-controlled `EventOptionEffectMode`** (OptionOnly /
  BaseThenOption / OptionThenBase) plus a state-based AI
  option chooser (NOT options[0]); player-country events
  defer effects until `PlayerCommandKind::ChooseEventOption`
  resolves the pending entry.
- **Pressure-gated, capacity-bounded AI policy selection**:
  countries below `kPressureThreshold=0.80` emit zero
  selections; above the gate, capacity returns 1/2/3 picks
  based on administrative_efficiency / bureaucratic_compliance
  / budget headroom.

From issue #112 onward, every `[X]` mark in §6 is backed by
behaviour observable from an ordinary headless
`leviathan.exe` run, not by a callable helper that no
production code path invokes (per the project's RFC-as-
contract standard).

The corrective batch ships:

- 20 country fixtures + 20 policy fixtures + 10 event
  definitions + 10 cross-country interest groups in a new
  `data/scenarios/1930_rfc_compliance.json` (canonical
  `1930_minimal.json` unchanged so M1.17 / M2.22 / M3.7 /
  M4.23 / M5.9 byte-identical determinism baselines stay
  green).
- Save schema bumped twice in the recovery sequence:
  - **v16 → v17** (RCR-1) covered: military_strength,
    weight_modifiers, options, followup_event_ids,
    relationships.
  - **v17 → v18** (issue #112) covers: `EventDefinition.category`
    (required non-empty), `EventDefinition.option_effect_mode`
    (required when options non-empty; absent when options
    empty), `GameState.pending_player_events` (deferred player
    choices). `AppliedPlayerCommand` also gains the
    `ChooseEventOption` kind with `{event_history_index,
    option_id_code}` payload.
  - `scenario_loader::load_into_state` preflight now rejects
    9 containers (the original 7 plus `pending_player_events`
    + `event_history` runtime-carryover surfaces) per
    issue #112 §7.
- New artefact `annual_world_stats.csv` bumps the runner's
  unconditional artefact contract from 10 to 11 (RFC-090
  §3.9 / RFC-010 §5).
- New `leviathan::systems::ai_policy` module with both
  `select_policies` and `apply_selected_policies` (RFC-090
  §3.5 / RFC-010 §2.2).
- New `leviathan::systems::annual_stats` module.
- `event_evaluator::rank_weighted_events` deterministic
  weighted ranker (RFC-090 §5.6) — produces the per-event
  weight vector. **The RFC-090 §5.7 weighted DRAW is wired
  in `event_engine::tick_events`** (issue #112), which feeds
  these weights into `random::weighted_choice(state.rng, …)`
  on each (country, category) bucket. `select_weighted_event`
  still exists as a deterministic top-pick helper for ad-hoc /
  diagnostic callers but is NOT the §5.7 implementation —
  `tick_events` is.
- **Event option flow (issue #112)**: for non-player
  countries `event_effects::select_best_option_for_country`
  scores options by their effects' desire-alignment with the
  country's pressures (NOT options[0]) and
  `event_effects::apply_option_effects_with_mode` applies in
  the author-controlled `EventOptionEffectMode` (OptionOnly /
  BaseThenOption / OptionThenBase, a NEW required v18 field).
  For the player country `tick_events` defers — appends a
  `core::PendingPlayerEvent` and waits for
  `PlayerCommandKind::ChooseEventOption` (reachable via the
  existing `--commands` script / `apply_pending` channel). The
  legacy `event_effects::select_default_option` /
  `apply_default_option_effects` helpers are retained for
  tests but are NOT the engine path. Two extended event
  fixtures (`legitimacy_crisis`, `corruption_scandal`) author
  representative options to exercise this path.
- `event_effects::resolve_followup_ids` (resolves ids to
  `state.events` indices) **plus `event_firer::record_followup`**
  which appends a followup `EventInstance` to
  `state.event_history` with parent-inherited actors plus
  a per-fire `event_fired` `LogEntry` (RFC-090 §5.12).
  Two extended event fixtures (`bureaucratic_strain`,
  `corruption_scandal`) author followup chains
  (`-> budget_shortfall_warning`, `-> legitimacy_crisis`).
- `event_firer::record_match` now emits one per-fire
  `LogEntry` into `state.logs` so the M0.6 `events.jsonl`
  artefact records fired events (RFC-090 §5.9).
- New integration tests: 365-day compliance run, 5-year
  annual-stats run, **1930–2000 (25567-day) sweep with
  byte-deterministic repeated run + annual CSV row pin**,
  10-year event stress test.

The implementation is internally consistent and passes the
test surface it actually owns:

- `leviathan_tests.exe` reports **1229 cases / 64274
  assertions** all passing on the M6.7 branch (verified via
  direct binary execution per `feedback_ctest_masks_doctest`).
  PR-by-PR delta along the recovery sequence: issue #112
  branch 1188 / 63657 → PR #113 (M6.6) 1216 / 64196 → PR #114
  (M6.7) 1229 / 64274.
- The canonical 70-year scenario (`data/scenarios/1930_minimal.json`,
  `--days 25567`) runs to `2000-01-01` with zero sanity
  issues and emits the 10 expected runner artefacts.

That internal consistency is **not** the same as full
original RFC-090 / RFC-010 acceptance. Several closed
implementation milestones diverge from the scope their
matching RFC section actually lists. This audit doc
distinguishes the two precisely so future readers and
future milestone work do not silently build on the wrong
assumption.

The distinction this audit pins down:

```text
implementation milestone closed
    !=
original RFC-090 / RFC-010 acceptance fully satisfied
```

### 1.1 Post-issue-#108 residual fixes

After RCR-1 (PR #107) landed, issue #108 flagged four residual mismatches:
the AI policy helpers shipped in RCR-1 were not wired into the normal
monthly simulation; the `military_strength` field on every compliance
country was the loader-default `0.0` rather than authored fixture
content; the `GameState.relationships` block was empty on the compliance
scenario despite the schema landing; and sections 2 / 3 / 4 below still
read as "current main" snapshots from the pre-RCR-1 era. The PR that
followed issue #108 (per memory rule
[[feedback-respect-rfc-over-skeleton]] "respect RFC over skeleton
minimalism"):

- Wires `ai_policy::apply_selected_policies` into
  `monthly::tick_all_countries` as the new step 7 (between M3
  state-wide and M5.8 `event_engine::tick_events`). AI policy
  selection + apply is now part of every monthly tick on the
  compliance scenario, not just a callable helper. The `MonthlyOutcome`
  exposes `ai_policies_considered` / `applied` / `skipped` / `failed`
  counters. M1 byte-identical assertions on canonical scenarios were
  rebaked deliberately (e.g. the M1.13 scenario now sees 12 monthly
  AI-applied entries per country in addition to the 2 day-0
  enactments).
- Authors a non-zero `military_strength` on every compliance country
  fixture (20 countries; values range from 10.0 — Mexico — to 90.0 —
  USA — for a rough-historical relative ordering). The save layer
  reflects authored values, not loader defaults.
- Authors 10 representative pairwise relationship/threat entries on
  `data/scenarios/1930_rfc_compliance.json` (GER↔FRA, GER↔POL,
  JPN↔CHN, USA↔GBR, SOV↔POL). Scenario loader extended with a new
  optional `relationships` manifest block + `ManifestRelation` POD;
  `load_into_state` resolves `from`/`to` id_codes against
  `state.countries`. Round-trip + range-validation tests in place.
- Reframes sections 2–4 below as "Issue #105 baseline before RCR-1"
  (not "current main"), so the doc no longer contradicts itself.

Save schema is unchanged (still v17 — no new persistent field; the
relationships block was added in RCR-1's v17). Artefact contract
unchanged (11). Test count rose from 1154 → 1158 (+4 new tests in
`monthly_pipeline_test.cpp` covering the new AI auto-apply step) /
62951 → 62982 (+31 assertions for the new compliance content
checks).

### 1.2 Post-M6.7 hardening sweep (NOT an RFC milestone)

After M6.7 landed, a cross-cutting hardening PR
(`feature/hardening-strict-numeric-validation`) applies
`feedback_no_silent_degradation` project-wide. The motivation:
late-game numeric bugs were nearly impossible to debug under the
pre-sweep `field = std::clamp(field + delta, 0.0, 1.0)` pattern,
because a single bad upstream value silently saturated to a bound
and propagated through hundreds of monthly ticks without surfacing
an error. The sweep is composed of two textually distinct passes:

- **Sweep A — numeric validation.** Every silent
  `std::clamp` / NaN-tolerance / silent-skip site in
  `policy_system`, `commands::AdjustBudget`, `faction_system`,
  `stability_system`, `economy_system`,
  `interest_group_system`, `effect_desire`, `ai_policy`,
  `random_service` now surfaces as `Result::failure` naming
  the entity kind, `id_code`, field path, and offending
  value. A new shared header
  `include/leviathan/systems/internal/numeric_guards.hpp`
  provides `require_unit_ratio`, `require_finite_double`,
  and `require_nonneg_finite` predicates.
- **Sweep B — strict structural fallback.**
  `event_effects::resolve_followup_ids` now rejects unknown
  `id_code`; `apply_event_effects` rejects vacuous actors;
  `event_effects::select_best_option_for_country` returns
  `Result<const EventOption*>`; `annual_stats::snapshot`
  rejects empty `state.countries`; the runner gates the
  year-boundary `annual_stats::snapshot` call with
  `!state.countries.empty()` (so an empty-world
  `leviathan.exe` run remains a valid time-tick exercise
  but emits no annual-stats row at the year boundary).

Helper signatures that can fail are promoted to `Result<T>` per
`feedback_api_signature_expresses_failure`:
`effect_desire::for_country`, `random_service::draw_bool`,
`annual_stats::snapshot`,
`event_effects::resolve_followup_ids`,
`event_effects::select_best_option_for_country`. All callers
propagate; no silent `value_or(default)`, no fallback score,
no skip-bad-candidate-and-continue.

Removing the silent clamp exposed a bounded-ratio update
problem: long-horizon AI policy application + linear `add`
on `[0, 1]` ratio fields + strict no-overshoot are jointly
incompatible — under the strict pass, the compliance scenario
saturated within months and cascaded into rejections. The
fix in the same PR is a formula migration: ratio-target `add`
now uses an **asymptotic** update

```
positive delta: new = old + delta * (1 - old)
negative delta: new = old + delta * old
```

so the strict validator passes by construction on ratio-target
`add`. The bounded / diminishing-returns shape is
**literature-aligned** (Polity IV / V-Dem bounded indicators
support a `[0, 1]` cap; Besley & Persson state-capacity stock
dynamics support diminishing returns near the bound) but the
exact functional form is a **game-model assumption**, not
derived from any single paper. Non-ratio fields
(`country.gdp`, `country.budget_balance`, `country.tax_revenue`,
`faction.resources`) keep linear `add` (consistent with
Barro-style additive-flow growth accounting; specific weight
coefficients remain authored gameplay parameters, not measured
empirical estimates).

This PR is **not** an RFC milestone:

- No new RFC-090 §6.X feature lands.
- No save schema bump (still v18).
- No new artefact (still 11).
- No new player-facing command, no new save-layer surface,
  no new gameplay system module.
- No M6 progression — M6 stays at M6.7; M6.8 / M6.9 are not
  green-lit by this PR.

Canonical numeric baselines are deliberately rebaked because
asymptotic-`add` produces different post-values than PR #114's
linear-`add`. Same-branch / same-seed determinism is preserved
(running the canonical `1930_minimal` 365-day scenario twice
produces byte-identical save / events.jsonl / CSVs). Compliance
`1930_rfc_compliance` 25 567-day (1930→2000) sweep completes
with `Sanity issues : 0`. Test count is 1 251 / 1 251 cases,
95 876 assertions, 0 failed, including 4 new trajectory-shape
tests in `tests/systems/asymptotic_add_trajectory_test.cpp`
per `feedback_trajectory_observation_tests` (bounded-from-
above approach; bounded-from-below approach;
1 000-input no-overshoot fuzz; symmetry around the midpoint),
plus 17 additional hardening cases pinning Result propagation
on `random_service::weighted_choice`,
`event_evaluator::rank_weighted_events`,
`event_evaluator::select_weighted_event`, and
`event_engine::tick_events` (malformed weights / non-finite
modifier inputs reject loudly; `state.rng.counter` is stable
on every failure path).

Design note:
[`hardening-strict-numeric-validation.md`](hardening-strict-numeric-validation.md).

### 1.3 M6.8 debug mode reveals truth (RFC-090 §6.8)

After the hardening sweep, M-numbered work resumed with
**M6.8** (`6.8 debug 模式顯示真相`). The change is a
presentation-layer toggle: when `--debug` is passed, every
`event_fired` LogEntry on the events.jsonl artefact gains
a `"true_cause": "<verbatim M6.1 string>"` metadata key
sourced from `state.events[match.event_index].true_cause`
(for parent fires; `EventDefinition.true_cause` directly for
followups). When `--debug` is omitted, the `true_cause`
metadata key is filtered out of the artefact at
`logging::write_jsonl_line` write time.

Three invariants make this a low-risk surface:

1. **Truth is recorded unconditionally.**
   `event_firer::record_match` and
   `event_firer::record_followup` push the `true_cause`
   metadata key onto every `event_fired` LogEntry regardless
   of `debug_mode`. The truth lives in `state.logs` and is
   serialised into the `save.json` `logs` array on every run.
2. **Save format is debug-flag-agnostic.** Two same-seed
   runs produce **byte-identical `save.json`** whether
   `--debug` is passed or not — the only diverging artefact
   is `events.jsonl`. No save schema bump (still `v18`).
3. **No state-engine effect.** `--debug` does not change
   which events fire (event_engine and event_evaluator are
   untouched), does not advance `state.rng`, does not mutate
   any country / faction / interest-group field. Canonical
   `1930_minimal` 365-day events.jsonl is byte-identical with
   and without `--debug` because no events fire on canonical
   (M5 invariant preserved). Compliance `1930_rfc_compliance`
   25 567-day (1930→2000) sweep completes with
   `Sanity issues : 0` on both debug and non-debug runs.

RFC-060 §3 declares the canonical `EventLogEntry { ...
publicText; debugTruth }` shape. M6.8 implements the
`debugTruth` reveal. M6.9 (`6.9 非 debug 模式隱藏真相`)
remains in-scope-but-not-shipped and will add the
`publicText` (M6.2 `visible_report`) flow with the M6.4
`reported_value::from_true_value` and M6.5
`bias_noise::sample_for_event` distortion applied in
non-debug mode — the first downstream consumer of the
`information_accuracy` family (M6.3 / M6.6 / M6.7).

Design note:
[`m6-8-debug-mode-reveals-truth.md`](m6-8-debug-mode-reveals-truth.md).

### 1.4 M6.9 non-debug mode hides the truth (RFC-090 §6.9)

After M6.8 landed, M-numbered work resumed with **M6.9**
(`6.9 非 debug 模式隱藏真相`). The change composes the three
M6 read-only helpers into `event_firer::record_match` /
`record_followup`:

1. `information_accuracy::compute_for_country(state, country)`
   → `accuracy ∈ [0, 1]` per the M6.3 / M6.6 / M6.7 formula
   (`BaseAccuracy + IntelligenceCapacity − Corruption`).
2. `reported_value::from_true_value(1.0, accuracy)` →
   `reported_intensity = 1.0 × accuracy = accuracy` per the
   M6.4 multiplicative formula. The M6.9 `TrueValue = 1.0`
   anchor is documented in `docs/m6-9-non-debug-mode-distortion.md`
   §2; it means "this event happened with intensity 1.0".
3. `bias_noise::sample_for_event(event_id_code, country_id_code, fired_on, 1 - accuracy)`
   → `noise_sample ∈ [-(1-accuracy), +(1-accuracy)]` per the
   M6.5 deterministic-hash formula. `state.rng` is NEVER
   consumed.

The composition emits new metadata keys per `event_fired`
LogEntry:

- **`publicText`** — verbatim string from M6.2
  `EventDefinition.visible_report`. The metadata key uses
  the RFC-060 §3 `EventLogEntry.publicText` vocabulary; the
  schema-level field keeps its M6.2 name. **Emitted on every
  `event_fired` LogEntry.**
- **`information_accuracy`, `reported_intensity`,
  `noise_sample`** — numeric distortion fields. **Emitted
  only on country-anchored events** (which is every event
  the M5.1 schema accepts at load time, because triggers
  must bind to a country / interest-group entity). The
  vacuous-actor hand-built test-only case has no country
  anchor for `information_accuracy::compute_for_country`,
  so these three keys are skipped while `publicText` is
  still emitted.

The M6.9 keys are emitted in **both debug and non-debug
modes**; only the M6.8 `true_cause` key remains
`--debug`-gated. RFC-060 §3 `EventLogEntry { publicText;
debugTruth }` shape is now structurally satisfied: M6.8
emits `true_cause` (the `debugTruth` side) and M6.9 emits
`publicText`.

RFC-080 §8 anchor: `ReportedValue = TrueValue + Bias + Noise`
with `Noise = RandomNormal(0, 1 - InformationAccuracy)`.
M6.9 ships Noise. The remaining RFC-080 §8 residual
classification — what is left of `Bias`, the missing accuracy
modifiers, the per-event `TrueValue` source, and the
separate `EventReport` artefact — is now tracked in
`docs/m6-closeout-audit.md` (the closeout-audit doc that ran
*after* M6.9 and explicitly does NOT close M6). The closeout
audit additionally ships two representative residuals
(`+ MediaFreedomSignal` accuracy positive term;
`+ PropagandaBias` Bias term, emitted as a new
`propaganda_bias_sample` metadata key alongside the M6.9
distortion keys) — see §1.5 below.

Per-event atomicity preserved: `event_firer::record_match`
and `record_followup` are promoted to `Result<bool>` (per
`feedback_api_signature_expresses_failure`). On failure
from any of the three helpers, the LogEntry is NOT appended
and the EventInstance is NOT appended to
`state.event_history`. `event_engine::tick_events` and
`recurse_followups_impl` propagate the Result with the
event id_code in the error context.

Determinism contract:

- **Canonical `1930_minimal` 365-day events.jsonl** is
  BYTE-IDENTICAL to the PR #116 (M6.8) baseline. No events
  fire on the canonical scenario (M5 invariant preserved),
  so no `event_fired` LogEntries land, so no M6.9 keys are
  appended.
- **Compliance `1930_rfc_compliance` 25 567-day** events.jsonl
  gains the four M6.9 keys per fired event line on the
  country-anchored compliance events (41 events × 4 keys =
  164 new metadata entries; every compliance event is
  country-anchored under the M5.1 schema). `Sanity issues
  : 0` on both debug and non-debug runs.
- **Same seed → byte-identical artefacts.** `bias_noise` is
  pure hash; no `state.rng` consumption means same-input
  same-output across runs. Pinned by a new integration test
  (`M6.9 integration: same seed produces deterministic
  distorted publicText`).

No save schema bump (still v18); no artefact contract
change (still 11); no new player-facing command; no new
save-layer surface; no new gameplay system module.

Design note:
[`m6-9-non-debug-mode-distortion.md`](m6-9-non-debug-mode-distortion.md).

### 1.5 M6 closeout audit (after M6.9)

After M6.9 landed (PR #117), an **M6 closeout audit** ran on
the next branch. The audit is documented in
[`m6-closeout-audit.md`](m6-closeout-audit.md); the closure
decision lives there, not in this compliance-audit doc. The
short summary:

- **M6 REMAINS OPEN.** The audit's executive decision (§1 of
  the audit doc) explicitly declines to close M6. Only Jason
  may write "M6 closed".
- The audit shipped **two representative RFC-080 §8 residuals**
  on top of the M6.1 – M6.9 base:
  1. **`+ MediaFreedomSignal`** — added to
     `information_accuracy::compute_for_country` as an outer
     weighted blend on the positive axis. Reads
     `government_authority.media_control` (M2.16 field, [0, 1]
     strict-validated). New constant
     `kInformationAccuracyMediaFreedomWeight = 0.20`; the
     existing inner intel-pair weight invariant
     (`kInformationAccuracyCapabilityWeight +
     kInformationAccuracyBudgetWeight = 1.0`) is preserved.
     Coefficient is a game-model assumption per RFC-080 §11;
     directional grounding cites V-Dem and Egorov-Guriev-Sonin
     QJP 2009.
  2. **`+ PropagandaBias`** — new
     `leviathan::systems::propaganda_bias` helper sibling to
     `information_accuracy`. Body:
     `kPropagandaBiasMaxMagnitude × media_control` with
     `kPropagandaBiasMaxMagnitude = 0.3`. Sign positive. Emitted
     by `event_firer::record_match` / `record_followup` as a
     new `propaganda_bias_sample` metadata key on every
     country-anchored fire (between `information_accuracy` and
     `reported_intensity`). Coefficient is a game-model
     assumption per RFC-080 §11; directional grounding cites
     V-Dem propaganda indicators, King-Pan-Roberts APSR 2013,
     and DellaVigna-Kaplan QJE 2007.
- **Remaining RFC-080 §8 residuals (closure blockers)**: the
  audit doc §5 / §9 enumerates each with research grounding,
  formula proposal, and implementation cost:
  - Bias terms: `FactionInterestBias`,
    `BureaucraticSelfProtection`.
  - Accuracy modifiers: `-FactionCapture`, `-LeaderIsolation`,
    `-LocalAutonomyOpacity`, `+BureaucraticProfessionalism`,
    `+AuditCapacity`.
  - Per-event TrueValue source (event_firer still pins
    `TrueValue = 1.0`).
  - Separate player-facing `event_reports.jsonl` artefact.
- **No save schema bump** (still v18). **No artefact contract
  change** (still 11). **No new player-facing command. No new
  RFC-090 milestone feature.** Metadata count on country-
  anchored fires bumped from 9 to 10 (the new
  `propaganda_bias_sample` key).

The audit is NOT M7 (per
`feedback_milestone_direction_gate`) and NOT RCR-2 (per
`feedback_rcr_recovery_track`). The compliance-audit doc here
remains the canonical place to look for issue-#105 governance
backlog; the closeout-audit doc remains the canonical place to
look for the M6 closure decision and the RFC-080 §8 residual
classification.

## 2. Finding 1 — RFC-090 original M3 drift (issue #105 baseline; see §6.1 for post-RCR-1 + #108 state)

> **Section reframing:** the prose below describes `main` **at the
> time of issue #105** (i.e. **before** PR #107 / RCR-1 and the issue
> #108 residual fix PR). The current per-item status — every
> actionable bullet `[X]` — lives in §6 below. This historical
> framing is kept verbatim so future readers can match the issue
> #105 narrative against the implementing work without re-deriving.

`rfc/RFC-090-roadmap.md` defines **Milestone 3：多國模擬**
(*multi-country simulation*) with the goal **"20–30 國能
自動跑"** and the following ten task list:

```text
3.1  定義 CountryData schema
3.2  寫 10 國資料
3.3  寫 20–30 國資料
3.4  實作多國 GameState
3.5  實作 AI policy selection
3.6  實作關係值
3.7  實作威脅值
3.8  實作簡單軍力值
3.9  實作年度世界統計
3.10 跑 1930–2000 自動測試
```

`docs/milestone-3-result.md` records that the closed M3
implementation milestone shipped:

- `core::InterestGroupState` + root-level vector + save v10
  → v11 (M3.1)
- `interest_group::react` mood drift system (M3.2)
- `interest_group::country_feedback` reverse-direction
  channel into `country.stability` (M3.3)
- `interest_group::authority_pressure` reverse-direction
  channel into `country.government_authority.bureaucratic_compliance`
  (M3.4)
- Three observability CSVs (`interest_groups.csv`,
  `interest_group_country_feedback.csv`,
  `interest_group_authority_pressure.csv`) (M3.5 / M3.6)
- M3 reaction-loop integration checkpoint (M3.7)
- Canonical Bureaucracy interest-group fixtures, one per
  country (M3.8)
- M3 exit / close-out doc (M3.9)

That is a valid implementation milestone — a small, closed,
deterministic reaction loop between country state and
interest-group state — but it is **not the same scope** as
RFC-090's original M3 multi-country simulation milestone.

Concretely, at the issue #105 baseline snapshot before the
compliance recovery PRs:

- `data/countries/` contains **3 country JSON files** (GER /
  FRA / JPN). RFC-090 §3.2 calls for 10 countries; §3.3 calls
  for 20–30 countries. Both unmet.
- There is **no AI policy-selection system** under
  `include/leviathan/systems/` or `src/leviathan/systems/`.
  RFC-090 §3.5 unmet.
- There is **no relationship-value system**, no
  **threat-value system**, and no **simple-military-value
  system**. RFC-090 §3.6 / §3.7 / §3.8 unmet.
- There is **no annual world-statistics artefact** (the
  monthly `summary.csv` is per-`month_changed`, not an
  annual aggregate; the per-country / per-faction CSVs are
  also per-month). RFC-090 §3.9 unmet in the form the RFC
  describes.
- A full **1930–2000 automated test** matching the original
  M3 scope (20–30 countries running automatically with AI
  policy selection, relationships, threat, military, annual
  stats) does not exist. The 70-year canonical run shows the
  current 3-country fixture can survive 70 simulated years,
  which is a useful determinism property but is narrower
  than RFC-090 §3.10's intended acceptance.

**Conclusion.** `docs/milestone-3-result.md` is a valid
close-out of the *interest-group reaction-loop
implementation milestone*. It is not full original RFC-090
M3 multi-country-simulation acceptance.

## 3. Finding 2 — RFC-090 original M5 drift (issue #105 baseline; see §6.2 for post-RCR-1 + #108 state)

> **Section reframing:** prose below describes `main` **at the time
> of issue #105** (before RCR-1 / issue #108 fix). Current per-item
> status is in §6.2.

`rfc/RFC-090-roadmap.md` defines **Milestone 5：事件引擎**
(*event engine*) with the goal **"條件加權事件"** and the
following twelve task list:

```text
5.1  定義 EventData
5.2  定義 TriggerCondition
5.3  定義 WeightModifier
5.4  定義 EventOption
5.5  定義 EventEffect
5.6  實作事件權重
5.7  實作事件抽選
5.8  實作事件選項
5.9  實作事件 log
5.10 寫 10 個事件
5.11 跑 10 年事件壓力測試
5.12 實作事件鏈 followup
```

`docs/milestone-5-result.md` records that the closed M5
implementation milestone shipped:

- Typed `core::EventDefinition` / `core::EventTrigger`
  schema (M5.1)
- `event_evaluator::match_events` read-only trigger
  evaluator (M5.2)
- `EventMatch` actor-binding skeleton with first-in-vector
  selection policy (M5.3)
- `core::EventInstance` + `state.event_history`
  append-only data layer + save v13 → v14 (M5.4)
- `event_firer::record_match` (M5.5)
- `event_effects::apply_event_effects` reusing the M1.5
  policy applicator via the `policy::apply_effects_to_actor`
  extract (M5.6)
- `event_engine::tick_events` composition helper with
  snapshot evaluation (M5.7)
- Monthly wiring of `tick_events` as step 7 of
  `monthly::tick_all_countries` (M5.8)
- Event observability checkpoint (M5.9)
- M5 exit / close-out doc (M5.10)
- A canonical event fixture file
  `data/events/1930_core_events.json` containing **two**
  event definitions (`low_stability_unrest`,
  `radical_interest_group_warning`), deliberately tuned so
  neither fires on the canonical scenario.

That is a valid implementation milestone — a complete event
*engine* skeleton with schema, evaluator, firer, applicator,
composition helper, and monthly wiring — but it is **not the
same scope** as RFC-090's original M5 event-engine milestone.

Concretely, at the issue #105 baseline snapshot before the
compliance recovery PRs:

- There was **no `WeightModifier` type and no event-weight
  system**. RFC-090 §5.3 / §5.6 unmet. The baseline
  evaluator uses an "ANY-entity-satisfies / AND across
  triggers" Boolean match, not weighted selection.
- There was **no `EventOption` type and no event-options /
  event-choices system**. RFC-090 §5.4 / §5.8 unmet. The
  applicator path applied a fixed `definition.effects`
  list; the later issue #112 fix adds player-choice resolution via
  `PendingPlayerEvent` and `PlayerCommandKind::ChooseEventOption`.
- There are **2 event definitions**, not 10. RFC-090 §5.10
  unmet. `docs/milestone-5-result.md` §5.7 acknowledges
  that the canonical events are deliberately tuned to NOT
  fire on the canonical scenario.
- There is **no 10-year event stress test**. RFC-090 §5.11
  unmet. The closest existing surface is the M5.9
  cross-leg integration suite (5 cases), which pins
  evaluator → firer → applicator seams on hand-built
  states, not a 10-year canonical-scenario event-firing
  load test.
- There is **no event-chain / followup-event model**.
  RFC-090 §5.12 unmet. There is no parent / child
  event-id linkage; `EventInstance` records do not chain.
- The lifecycle log artefact `events.jsonl` does **not**
  emit per-fire records for `EventInstance`s. M5.9 tests
  explicitly pin that canonical event id_codes never
  appear in `events.jsonl`. RFC-090 §5.9's "event log"
  is partially met by the in-memory `state.event_history`
  vector but is **not** met as a per-fire log artefact.

**Conclusion.** `docs/milestone-5-result.md` is a valid
close-out of the *event-engine skeleton implementation
milestone*. It is not full original RFC-090 M5 event-engine
acceptance (which expects weights + options + 10 events +
stress test + followup chains).

The M6 hidden-truth work currently in progress (M6.1 / M6.2
schema fields, M6.3 / M6.4 / M6.5 helper skeletons, M6.6
intelligence-budget formula body for `information_accuracy`)
layers on top of this skeleton event engine. It does not
retroactively satisfy the deferred RFC-090 §M5 scope.

## 4. Finding 3 — RFC-010 v0.1 acceptance floors remain deferred (issue #105 baseline; see §6.3 for post-RCR-1 + #108 state)

> **Section reframing:** prose below describes `main` **at the time
> of issue #105** (before RCR-1 / issue #108 fix). Current per-item
> status is in §6.3.

`rfc/RFC-010-prototype-v0_1.md` §5 lists the v0.1
acceptance criteria:

```text
- 同 seed 結果可重現
- 可連續跑 70 年不崩潰
- 至少 20 國自動運作
- 至少 20 個政策
- 至少 10 個事件模板
- 至少 6 個派系
- SVG 地圖可更新國家顏色
- 可輸出年度統計 CSV
```

Current implementation status against that list:

```text
- 同 seed 結果可重現                              MET   (M1.17 + M2.22 + M3.7 + M5.9 byte-identical
                                                       determinism contracts; 5- / 8- / 10-artefact)
- 可連續跑 70 年不崩潰                            MET   (canonical 70-year run reaches 2000-01-01
                                                       with zero sanity issues on current main)
- 至少 20 國自動運作                              UNMET (3 country JSONs: GER, FRA, JPN)
- 至少 20 個政策                                  UNMET (10 policy JSONs under data/policies/)
- 至少 10 個事件模板                              UNMET (2 event definitions under
                                                       data/events/1930_core_events.json)
- 至少 6 個派系                                   PARTIAL (3 legacy faction JSONs, all GER-scoped;
                                                       3 canonical Bureaucracy interest groups via
                                                       M3.8; not yet the 6–8 faction list from
                                                       RFC-010 §2.5 across countries)
- SVG 地圖可更新國家顏色                          MET   (M4.2 SVG renderer + M4.3 owner-color fill
                                                       + 10-artefact set includes provinces.svg
                                                       and map.html)
- 可輸出年度統計 CSV                              UNMET (current CSVs are monthly: summary /
                                                       countries / factions / 3 interest-group;
                                                       no annual aggregate artefact)
```

**Conclusion.** Two of eight RFC-010 v0.1 acceptance items
are currently UNMET (countries floor, policies floor, events
floor, annual statistics CSV — counted four UNMET in the
list above) and one is PARTIAL (factions floor). The
implementation can run and is well tested, but the project
should not be described as having progressed past roadmap
stages that depend on these v0.1 acceptance criteria unless
those criteria are met or the RFC is deliberately revised.

## 5. Governance rule going forward

Milestone close-out docs **must** state whether they are:

1. *implementation milestone* close-outs, or
2. *full original RFC acceptance* close-outs.

When the two diverge, the close-out doc must:

- Say so explicitly near the top.
- Link here for the canonical deferred-scope list.
- Add any milestone-specific deferred RFC acceptance items
  to the relevant subsection below.

This audit doc is the **only** place that has to be updated
when a future PR actually implements one of the deferred
items: cross out the line, link the implementing PR, move
on. New milestone close-out docs link to this audit doc
rather than re-litigating the gap.

### 5.1 RCR is a one-time corrective action, NOT a new track

`RCR` (RFC Compliance Recovery) is the identifier for **a
single corrective batch PR** that closes the RFC-090 §M3 /
§M5 / RFC-010 v0.1 gap discovered by issue #105. It is **not**
an RFC milestone number, and it is **not** a new long-running
recovery track. The 2026-05-17 force-reset lesson — don't
invent milestone numbers that don't map to an RFC section
(see memory file `feedback_rfc_milestone_alignment`) — still
stands; RCR is deliberately *outside* the M-number sequence
so the corrective batch doesn't pollute RFC milestone history.

The reviewer's framing intent, recorded after the PR #107
review:

> RCR is an emergency correction, not a new development lane.
> RCR 不是新的灰色開發線。RCR 是一次性 corrective action。

Concretely:

- **`RCR-1` is the corrective PR for this drift**. Once it
  lands and the §6 backlog is `[X]`, execution returns to
  the M-numbered milestone sequence (M6.6 resumes per
  RFC-090 §6.6 on explicit go-ahead).
- **There is no planned `RCR-2` / `RCR-3` sequence.** If a
  later audit ever discovers a *new* governance drift, that
  audit may necessitate another corrective PR which could
  re-use the `RCR-` prefix for symmetry — but that would be
  an exception triggered by a fresh finding, not a
  continuation of this corrective batch's roadmap.
- The default for partial items in §6 is to `[X]` (cleared
  in RCR-1). An item is left `[ ]` or `[~]` only with an
  inline per-item justification of why it could not safely
  fit into this one corrective PR. "Scope size" or "batching
  convenience" are explicitly NOT acceptable deferral
  rationales.

RCR work does NOT mark M3 / M5 closed-as-full-RFC. The two
result docs stay labelled as *implementation milestone*
close-outs (see the governance notes at the top of
`docs/milestone-3-result.md` and
`docs/milestone-5-result.md`). RCR-1 only crosses items off
the §6 backlog; the milestone close-out semantics for M3 / M5
are unchanged.

Once every backlog item is `[X]`, this audit doc gets a
"RFC-090 / RFC-010 v0.1 fully satisfied as of PR #NNN" note
at the top and the `RCR-` identifier stops being load-bearing.

This rule applies retroactively to the three close-outs that
already shipped (M0 / M1 / M2 / M3 / M4 / M5). M3 and M5 are
the two with material drift; their result docs have been
annotated with top-of-file governance notes (this PR). M0 /
M1 / M2 / M4 are not annotated in this PR because:

- M0 is technical bootstrapping (RFC-060 + RFC-070 §6 + the
  RFC-090 §M0 task list verbatim); no material RFC-vs-impl
  drift.
- M1 is single-country internal politics and ships the
  RFC-090 §M1 task list essentially verbatim (1.1 – 1.17).
- M2 is the player-operation prototype and ships the
  RFC-090 §M2 task list essentially verbatim, plus
  replay-verify and command-gate work that extended the
  surface without contradicting RFC §M2.
- M4 is the SVG-map / UI prototype and ships the RFC-090
  §M4 task list with some additions (HTML viewer,
  accessibility / focus / hover polish) that are
  RFC-faithful extensions rather than scope skips.

If a future audit finds drift in any of those four, this
section gains a corresponding Finding.

## 6. Deferred RFC compliance backlog

Each bullet is one tracked deferred original-RFC scope
item. Order within each subsection mirrors the RFC's own
task ordering where reasonable.

Format: each item is one line, prefixed with the RFC
task ID for cross-reference. Strikethrough (or removal)
indicates the item has been implemented since this audit
landed; the PR that implements it should edit this list
inline.

### 6.1 Original RFC-090 M3 backlog (multi-country simulation)

```text
[X] RFC-090 §3.2  10-country fixture set                   — RCR-1
[X] RFC-090 §3.3  20-30 country fixture set                — RCR-1
                  (20 countries in
                  data/scenarios/1930_rfc_compliance.json;
                  canonical 1930_minimal.json unchanged so
                  M1.17 / M2.22 / M3.7 / M4.23 / M5.9
                  byte-identical determinism baselines stay
                  green)
[X] RFC-090 §3.5  AI policy selection                      — issue #112
                  Issue #112 layered TWO gates on top of the
                  issue #110 scorer (RFC-040 §4 inputs):
                  - **Pressure gate** (`kPressureThreshold=0.80`):
                    countries whose `compute_total_pressure`
                    (sum of stability + legitimacy + corruption
                    + budget + threat (inbound rel-threat OR
                    military-gap) + IG-radicalism, each
                    normalised to [0,1]) falls below the
                    threshold emit ZERO selections that tick.
                    `ApplyOutcome.pressure_below_threshold_skipped`
                    counts them.
                  - **Capacity bound** (`capacity_to_count`):
                    countries above the gate emit 1/2/3 picks
                    based on
                    `0.5×admin_efficiency + 0.3×bcomp + 0.2×budget_headroom`
                    (kCapacityLowMax=0.30, kCapacityMediumMax=0.60).
                  The scorer itself is unchanged from #110 —
                  `score_policy` still reads stability, legitimacy,
                  corruption, budget, threat, military_strength,
                  IG influence/loyalty/radicalism, policy
                  category, and per-effect target/op/value.
                  No-stacking rule preserved: candidates already
                  active+unexpired are excluded; chooser picks
                  next-best.
                  Calibration: with 0.80 threshold, all 20
                  compliance countries clear the gate at least
                  some months over 365 days (240 active-policy
                  entries / 20 countries / 9 distinct id_codes
                  in the live compliance run).
                  Tests: tests/integration/issue_110_ai_scorer_test.cpp
                  six pressure-above-threshold cases prove the
                  scorer steers selection per input axis;
                  ai_policy_test.cpp covers low-pressure → 0,
                  high-pressure → 1/2/3 (capacity), no-stacking,
                  player skip.
[X] RFC-090 §3.6  relationship values                      — issue #110
                  Schema: core::CountryRelation POD
                  ({from, to, relationship, threat}) +
                  GameState::relationships vector (save v17).
                  Authored: 10 pairwise entries in
                  data/scenarios/1930_rfc_compliance.json
                  (GER<->FRA, GER<->POL, JPN<->CHN, USA<->GBR,
                  SOV<->POL).
                  Wired: the issue #110 AI scorer reads each
                  `to`-country's max inbound threat AND the
                  `from`-country's military_strength to feed
                  the policy.military_power desire term — so
                  AI behaviour observably tracks authored
                  relationship hostility. The relationship-DRIVING
                  side (M8 / RFC-040 diplomacy AI) remains out
                  of scope; the relationship-READING side is now
                  shipped and tested
                  (issue_110_ai_scorer_test.cpp::
                  high_threat_country_picks_military_policy).
[X] RFC-090 §3.7  threat values                            — issue #110
                  Folded into CountryRelation.threat ([0, 1])
                  AND CountryState.threat_perception. The issue
                  #110 scorer's `country.military_power` desire
                  term reads `max(threat_perception, max-over-
                  inbound-relationships of threat)`, so both
                  surfaces actually steer behaviour rather than
                  sitting as inert data.
[X] RFC-090 §3.8  simple military values                   — issue #110
                  Schema: CountryState.military_strength
                  absolute scalar (>= 0), distinct from the
                  military_power ratio. Save schema v17 makes
                  it required.
                  Authored: every one of the 20 compliance
                  country JSONs carries a non-zero value
                  (10.0 — Mexico — up to 90.0 — USA — for a
                  rough-historical ordering).
                  Wired: the issue #110 scorer's
                  `country.military_power` desire term also
                  reads `max(neighbour.military_strength) -
                  this_country.military_strength` (normalised
                  by 100) and adds it to the
                  effective_threat to determine how strongly
                  AI countries militarise. Test:
                  issue_110_ai_scorer_test.cpp::
                  military_strength_disparity_picks_military_policy
                  (which uses a zero-threat relationship so
                  only the strength gap drives the pick).
[X] RFC-090 §3.9  annual world statistics                  — RCR-1
                  New leviathan::systems::annual_stats
                  module + new unconditional
                  annual_world_stats.csv artefact (bumps
                  artefact contract 10 -> 11). Header +
                  per-year rows emitted on every
                  year-boundary crossing. Byte-stable
                  CSV format mirrors M1.14 / M1.16.
[X] RFC-090 §3.10 full 1930-2000 automated test            — RCR-1
                  tests/integration/rcr_1_rfc_compliance_test.cpp
                  contains the 25567-day sweep on the 20-country
                  compliance scenario: reaches 2000-01-01 with
                  zero sanity issues; annual_world_stats.csv
                  carries 71 rows (initial 1930 + 70 year
                  boundaries); two repeated runs produce
                  byte-identical save.json, annual_world_stats.csv,
                  and events.jsonl. Plus a 365-day load test
                  and a 5-year annual-stats test alongside.
```

Legend: `[ ]` = unmet; `[X]` = cleared; `[~]` = partially
cleared (read the inline note for what shipped vs. what
remains).

Note: §3.1 (`CountryData` schema) and §3.4 (multi-country
`GameState`) are both met by current main (`CountryState`
exists in `include/leviathan/core/entities.hpp`;
`GameState::countries` is a vector). They are not in this
backlog.

### 6.2 Original RFC-090 M5 backlog (event engine)

```text
[X] RFC-090 §5.3  WeightModifier model                      — RCR-1
                  New core::WeightModifier POD
                  ({target, op, value, weight_delta}) +
                  EventDefinition.weight_modifiers vector.
                  Save schema v17 makes the block required at
                  the save layer; scenario loader (parse_event_file)
                  parses optional weight_modifiers[] from
                  event JSON; diagnostics::compare_states
                  walks the field. Three extended event
                  fixtures (bureaucratic_strain,
                  budget_shortfall_warning,
                  military_loyalty_concern) author
                  representative modifiers; round-trip + load
                  + save tests pin the path.
[X] RFC-090 §5.4  EventOption model                         — RCR-1
                  New core::EventOption POD
                  ({id_code, label, effects[]}) +
                  EventDefinition.options vector. Save +
                  scenario loader + diagnostics same shape
                  as §5.3. Two extended event fixtures
                  (legitimacy_crisis, corruption_scandal)
                  author two options each. The
                  event_effects::apply_default_option_effects
                  helper (§5.8 below) is the callable
                  effect-application path for the first
                  option.
[X] RFC-090 §5.6  event weights system                      — issue #110
                  rank_weighted_events (RNG-free; base
                  kBaseWeight=1.0 + sum of matching modifier
                  weight_deltas; stable-sort descending by
                  weight, tie-break on event vector index).
                  Wired into `event_engine::tick_events`:
                  weights feed `random::weighted_choice` in
                  the per-country / per-category draw, so
                  authored `weight_modifiers` shape selection
                  probability (issue #112 §1 / §2). Tests:
                  event_engine_issue_112_test::"different
                  seeds can produce different selected events
                  from the same matched pool" (200-seed
                  statistical proof both events are drawn);
                  + "single-candidate bucket still consumes
                  one RNG draw" (documents the
                  random::weighted_choice contract).
[X] RFC-090 §5.7  weighted event selection                  — issue #112
                  `tick_events` now performs a REAL weighted
                  random draw (RFC-090 §5.7 "事件抽選").
                  Each (country, category) bucket draws
                  exactly ONE event via
                  `random::weighted_choice(state.rng, weights,
                  tag)`; ties are handled naturally by the
                  weighted draw (NOT by deterministic
                  highest-weight pick). Per-country isolation:
                  one country's draw does NOT consume another
                  country's match pool (issue #112 §3 / E1 —
                  same template fires for multiple countries
                  in the same tick). Non-selected matched
                  events do NOT fire, do NOT log, do NOT
                  apply effects. Canonical 1930_minimal
                  preserves zero-fire (its events deliberately
                  never match the canonical state); compliance
                  scenario fires ~32 events / 365 days in the
                  live run. Tests: event_engine_issue_112_test
                  ::"fires ONE event per (country, category)
                  bucket", "fires ONE event per DIFFERENT
                  category", "same template fires for multiple
                  countries independently", "no matched events
                  → state.rng.counter unchanged".
[X] RFC-090 §5.8  event options / player choices            — issue #112
                  Issue #112 replaces the issue #110
                  options[0] default with TWO real surfaces:
                  - **AI / non-player chooser** (state-based):
                    `select_best_option_for_country` scores
                    each option's `effects` via
                    `effect_desire::for_country` — option that
                    most-improves the country's pressure-axes
                    wins, with stable tie-break on lower
                    option vector index.
                  - **Player chooser** (command-layer): when
                    `tick_events` draws an event with non-
                    empty options for `state.player_country`,
                    it records the parent EventInstance but
                    applies NO effects and processes NO
                    followups; a `PendingPlayerEvent` is
                    appended to `state.pending_player_events`.
                    The player resolves via
                    `PlayerCommandKind::ChooseEventOption`
                    (`event_history_index` +
                    `option_id_code`), reachable through
                    the existing `--commands` script /
                    `apply_pending` path. NOTE: this satisfies
                    "player choices" through the COMMAND
                    LAYER; a graphical UI prompt remains a
                    future UI milestone if/when interactive
                    UI lands. We do NOT claim "UI complete".
                  Author-controlled
                  `EventOptionEffectMode` (OptionOnly /
                  BaseThenOption / OptionThenBase) is a new
                  required field (v18) on option-bearing
                  events and gates how base + chosen-option
                  effects compose.
                  Tests: event_engine_issue_112_test::"AI
                  option chooser picks state-best option, NOT
                  options[0]", "option_effect_mode = OptionOnly
                  / BaseThenOption / OptionThenBase" (one per
                  mode), "player-country event with options
                  creates pending entry, applies NO effects,
                  processes NO followups".
[X] RFC-090 §5.9  per-fire events.jsonl emission            — RCR-1
                  event_firer::record_match now appends
                  one LogEntry with category "event_fired"
                  per fired EventInstance, with
                  event_id_code / actor_kind / actor_id_code
                  / country_id_code as metadata. Canonical
                  scenarios at M5 remain no-fire so
                  canonical events.jsonl bytes stay
                  byte-identical with the M5 close-out.
                  The M5.7 / M5.5 "no logs append" unit
                  invariants are deliberately migrated to
                  "appends one per fired event".
[X] RFC-090 §5.10 10 event definitions                      — RCR-1
                  (2 canonical events from
                  data/events/1930_core_events.json +
                  8 extended events from new
                  data/events/1930_rfc_extended_events.json;
                  the compliance scenario references both
                  files. Canonical 1930_minimal.json is
                  unchanged so the M5 canonical-non-fire
                  property holds.)
[X] RFC-090 §5.11 10-year event stress test                 — RCR-1
                  tests/integration/rcr_1_rfc_compliance_test.cpp
                  ships a 10-year run on a hand-built firing
                  state asserting event_history grows by
                  100+ entries, events.jsonl records
                  event_fired entries with the fired id_code,
                  and the save round-trip preserves them.
[X] RFC-090 §5.12 event-chain / followup-event model        — issue #112
                  Issue #112 replaces the issue #110 depth-1
                  unconditional record with a CONDITIONAL,
                  recursive chain matching RFC-050 §3 "事件
                  鏈是條件連鎖":
                  - Each followup must satisfy its OWN
                    triggers AFTER the parent's effects apply
                    (F1+G1). Followups whose triggers don't
                    match are NOT recorded and DO NOT apply
                    effects.
                  - If multiple followups match, a weighted
                    random draw (using `state.rng`) selects
                    ONE — same primitive as the parent-fire
                    draw.
                  - `record_followup` uses the IMMEDIATE
                    PREDECESSOR (not the root parent), so
                    events.jsonl `followup_of` metadata points
                    to the direct chain step.
                  - Recursion runs depth-N up to
                    `event_engine::kMaxFollowupDepth = 5`,
                    with a visited-event-id_code cycle guard
                    (faithful because the loader rejects
                    duplicate id_codes). The depth guard and
                    cycle guard run simultaneously.
                  Two extended event fixtures exercise the path
                  (bureaucratic_strain ->
                   budget_shortfall_warning;
                   corruption_scandal -> legitimacy_crisis).
                  Tests: event_engine_issue_112_test::
                  "followup whose triggers fail post-parent-
                   apply is NOT recorded", "followup whose
                   triggers DO match post-parent-apply records
                   + applies", "followup chain immediate-
                   predecessor `followup_of` metadata",
                  "followup cycle guard stops A → B → A",
                  "followup max-depth guard stops at
                   kMaxFollowupDepth = 5".
```

Note: §5.1 (`EventData`) is met by `EventDefinition` (M5.1);
§5.2 (`TriggerCondition`) is met by `EventTrigger` (M5.1)
and `event_evaluator` (M5.2); §5.5 (`EventEffect`) is met
by the `PolicyEffect`-reuse decision in M5.1 and the
`event_effects` applicator (M5.6).

### 6.3 RFC-010 v0.1 backlog

```text
[X] RFC-010 §5    20-country floor                          — RCR-1
                  (20 countries in
                  data/scenarios/1930_rfc_compliance.json;
                  see §6.1 RFC-090 §3.3 entry above)
[X] RFC-010 §5    20-policy floor                           — RCR-1
                  (20 policy JSONs under data/policies/;
                  the compliance scenario loads all 20)
[X] RFC-010 §5    10-event floor                            — RCR-1
                  (see §6.2 RFC-090 §5.10 entry above)
[X] RFC-010 §5    6+-faction / actor floor across countries — RCR-1
                  (10 interest groups across 10
                  countries: GER / FRA / JPN
                  Bureaucracy + GBR Workers + USA
                  Business + SOV Military + CHN Farmers
                  + ITA Media + IND Religious + SWE
                  Technocrats; spread satisfies the "not
                  all GER-scoped" requirement. The 3
                  legacy faction JSONs are still
                  GER-scoped and unchanged.)
[X] RFC-010 §2.2  AI countries auto-select policies         — RCR-1 + #108
                  Selection + apply both shipped AND wired
                  into `monthly::tick_all_countries` as step
                  7 (issue #108 fix). See §6.1 RFC-090 §3.5
                  entry above for the full surface
                  description.
[X] RFC-010 §5    annual statistics CSV                     — RCR-1
                  New annual_world_stats.csv unconditional
                  artefact (see §6.1 RFC-090 §3.9 entry
                  above).
```

Met items (kept for reference, not in the backlog):

```text
[X] RFC-010 §5    同 seed 結果可重現
[X] RFC-010 §5    可連續跑 70 年不崩潰
[X] RFC-010 §5    SVG 地圖可更新國家顏色   (M4.2 / M4.3 + 10-artefact set)
```

### 6.4 Other RFC scope deferred for now

Items mentioned in RFC-090 §M7+ / RFC-020 / RFC-030 /
RFC-040 / RFC-050 / RFC-080 that current main also doesn't
implement but that are **outside the M3 / M5 / RFC-010
v0.1 acceptance gap this audit covers**. They live in
their own RFCs' task lists and will be picked up by their
own milestones. This audit doc does not duplicate them.

Pointers (read the RFCs directly):

- `rfc/RFC-020-politics-internal.md` — internal politics
  beyond the M3 reaction loop.
- `rfc/RFC-030-economy-budget.md` — inflation / debt /
  industry / trade.
- `rfc/RFC-040-diplomacy-war-ai.md` — diplomacy, world AI,
  war (overlaps §3.5 / §3.6 / §3.7 / §3.8 above).
- `rfc/RFC-050-events-hidden-truth.md` — event content
  and hidden-truth direction (M6 in progress at M6.6
  covers part of the hidden-truth side).
- `rfc/RFC-080-research-formulas.md` — formula expansions
  M1.7 / M1.8 stripped down (WelfareSatisfaction /
  EconomicGrowth was added in M1.12; InflationPressure /
  WarDamage / InequalityProxy / WarWeariness /
  BudgetCrisis still deferred).

## 7. What the compliance recovery still does NOT do

RCR-1 plus the issue #108 / #110 / #112 follow-up fixes deliberately
do NOT:

- Modify M6 hidden-truth helper behaviour beyond what
  RCR-1 / #108 / #110 / #112 already documented.
  M6.6 — RFC-090 §6.6 "加入情報預算影響" — landed in its own PR
  following this corrective batch, replacing the M6.3
  constant-1.0 placeholder body of
  `information_accuracy::compute_for_country` with the affine
  `accuracy = 0.4 + 0.6 × (0.7 × intelligence_capability +
  0.3 × budget.intelligence)` formula (range
  `[kMinInformationAccuracy=0.4, 1.0]`). **M6.7 — RFC-090
  §6.7 "加入腐敗影響" — has now also landed**, layering the
  RFC-080 §8 `-Corruption` term on top of the M6.6 baseline:
  `accuracy = m6_6_baseline - 0.4 × corruption`. Function-level
  range graduates to `[0.0, 1.0]`. New public constant
  `kInformationAccuracyCorruptionWeight = 0.4` (symmetric to
  `kMinInformationAccuracy`). The whole `compute_for_country`
  body now follows the `feedback_no_silent_degradation` rule:
  out-of-range / non-finite inputs are rejected with
  `Result::failure` (naming country `id_code` + field + value),
  not silently clamped. `reported_value` and `bias_noise`
  bodies are unchanged; M6.8 (debug bypass) / M6.9 (first
  non-debug caller) remain out of scope. Canonical
  `1930_minimal` byte-identical baselines stay green because
  no production code path calls `compute_for_country` yet.
- Add debug-mode / non-debug-mode hidden-truth display
  (RFC-090 §6.8 / §6.9, separate M6 work).
- Add UI / map visualisation / SVG renderer changes.
- Add war or full diplomacy AI (M9 / RFC-040 territory).
- Make the event pipeline globally RNG-free. The legacy helper
  APIs (`rank_weighted_events`, `select_weighted_event`,
  `select_default_option`, `resolve_followup_ids`) remain
  deterministic, but PR #111's production `tick_events` path
  consumes `state.rng` through `random::weighted_choice` whenever
  a scenario produces selectable events or followups. The canonical
  1930_minimal scenario preserves byte-identical baselines because
  its authored events still do not match, so no draw is consumed.
- Rewrite `rfc/RFC-090-roadmap.md` or
  `rfc/RFC-010-prototype-v0_1.md` themselves. The roadmap
  RFC stays the source of truth for the intended scope;
  this audit doc is the source of truth for what shipped
  vs. what is deferred.
- Mark M3 / M5 closed-as-full-RFC. Their result docs stay
  labelled as *implementation milestone* close-outs per
  the governance notes at the top of each file.
- Mark M6 as closed.
- Close issue #105 automatically. Closing is the
  reviewer's call after RCR-1 lands and the audit-doc
  backlog reads as "fully cleared".
- Open `RCR-2` or any "next batch" PR. RCR is a one-time
  corrective action, not a roadmap stage (§5.1).

## 8. How an item moves from `[ ]` to `[X]`

After RCR-1, the §6 backlog is fully cleared (every actionable
item is `[X]`). If a future audit ever discovers a *new* RFC
governance drift, the procedure for clearing items here is:

1. The implementing PR opens under a matching RFC milestone
   PR (preferred), or — only if the corrective scope spans
   multiple unrelated RFC sections in a way that doesn't fit
   any single milestone — a fresh corrective PR which may
   re-use the `RCR-` prefix for symmetry with RCR-1. **Do
   not invent new milestone numbers that don't map to an RFC
   section** — that lesson is captured in
   `docs/milestone-3-result.md` §7 and in memory file
   `feedback_rfc_milestone_alignment`. **Do not treat
   `RCR-` as a long-running parallel track** — see §5.1.
2. The implementing PR edits this audit doc inline:
   - Change `[ ]` to `[X]` on the line that ships.
   - Use `[~]` when an item is only partially cleared and
     write a one-paragraph note explaining exactly what
     shipped vs. what is still missing (an inline per-item
     technical-impossibility justification, not "scope
     size" or "batching convenience").
   - Add the PR / RCR identifier at the end of the line:
     `[X] RFC-090 §5.10 10 event definitions — RCR-1`
     or `[X] RFC-090 §3.5 AI policy selection — PR #NNN`.
3. If a future PR ships an *implementation milestone* that
   maps to the original RFC scope cleanly, its close-out
   doc can be both an implementation close-out **and** a
   full original RFC acceptance close-out — say so
   explicitly at the top.
4. Once every bullet in §6.1 / §6.2 / §6.3 is `[X]` *and
   stays cleared* under a future audit, this audit doc gets
   a final "RFC-090 / RFC-010 v0.1 fully satisfied as of PR
   #NNN" note at the top and stops being load-bearing for
   new work.
