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
implementation milestones**. M6 is in progress at M6.5
(`bias_noise` helper skeleton). The first **RCR** (RFC
Compliance Recovery) batch, RCR-1, has shipped and cleared
the data-shaped portion of the audit-doc backlog
(see §6 for the precise per-item status). M6.6
implementation work is paused while the RCR track runs.

The implementation is internally consistent and passes the
test surface it actually owns:

- `leviathan_tests.exe` reports **1107 cases / 62585
  assertions** all passing on current main.
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

## 2. Finding 1 — RFC-090 original M3 drift

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

Concretely, on current main:

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

## 3. Finding 2 — RFC-090 original M5 drift

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

Concretely, on current main:

- There is **no `WeightModifier` type and no event-weight
  system**. RFC-090 §5.3 / §5.6 unmet. The current
  evaluator uses an "ANY-entity-satisfies / AND across
  triggers" Boolean match, not weighted selection.
- There is **no `EventOption` type and no event-options /
  event-choices system**. RFC-090 §5.4 / §5.8 unmet. The
  applicator path applies a fixed `definition.effects`
  list; there is no player-choice surface on `EventInstance`.
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
schema fields, M6.3 / M6.4 / M6.5 helper skeletons) layers
on top of this skeleton event engine. It does not retroactively
satisfy the deferred RFC-090 §M5 scope.

## 4. Finding 3 — RFC-010 v0.1 acceptance floors remain deferred

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

### 5.1 RCR identifiers are NOT milestone numbers

Recovery PRs that clear items from §6 use the identifier
**`RCR-N`** (RFC Compliance Recovery, batch N). `RCR-N` is a
**recovery-track identifier**, not an RFC milestone number. It
**does not** consume or replace M0–M9 / RFC-090 §M3 / §M5
numbering. The 2026-05-17 force-reset lesson (don't invent
milestone numbers that don't map to an RFC section — see
memory file `feedback_rfc_milestone_alignment`) stands; RCR
identifiers are deliberately *outside* the M-number sequence
so they don't pollute the RFC milestone history.

Concretely:

- `RCR-1` (this batch) clears the data + selection items
  that don't require save schema changes.
- `RCR-2` (planned) will clear the items currently labelled
  "deferred to RCR-2" in §6 — primarily the save schema
  bump v16 → v17 batching `EventOption` / `WeightModifier` /
  `EventDefinition.followup_event_ids` / a `GameState`-level
  relationships matrix, plus the annual world stats CSV
  artefact, the AI policy *apply* path, and the
  events.jsonl per-fire emission.
- Subsequent `RCR-N` batches continue until every bullet
  in §6.1 / §6.2 / §6.3 is `[X]`.

PR titles for the track look like
`RCR-N RFC-090 / RFC-010 compliance recovery batch`. PR bodies
must contain a section stating "RCR-N is a recovery-track PR,
not an M-number milestone" so reviewers don't misread the
identifier as an invented milestone number.

RCR work does NOT mark M3 / M5 closed-as-full-RFC. The two
result docs stay labelled as *implementation milestone*
close-outs (see the governance notes at the top of
`docs/milestone-3-result.md` and
`docs/milestone-5-result.md`). RCR work only crosses items
off the §6 backlog.

Once every backlog item is `[X]`, this audit doc gets a
"RFC-090 / RFC-010 v0.1 fully satisfied as of PR #NNN" note
at the top and `RCR-*` identifiers stop being load-bearing.

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
[X] RFC-090 §3.3  20–30 country fixture set                — RCR-1
                  (20 countries in
                  data/scenarios/1930_rfc_compliance.json;
                  the canonical 1930_minimal.json stays at
                  3 countries to preserve byte-identical
                  M1.17 / M2.22 / M3.7 / M4.23 / M5.9
                  determinism baselines)
[~] RFC-090 §3.5  AI policy selection                      — RCR-1 (partial)
                  selection-only skeleton shipped
                  (leviathan::systems::ai_policy::
                  select_policies). The corresponding
                  *apply* path (calling
                  policy::apply_policy_effects for each
                  AI selection) is deferred to a future
                  RCR PR because it touches M1.5
                  pre-flight atomicity + M1.15
                  active_policies bookkeeping + every
                  byte-identical determinism baseline.
[ ] RFC-090 §3.6  relationship values                      — deferred to RCR-2
                  (requires new persistent state + save
                  schema bump v16 -> v17 + scenario_loader
                  + diagnostics walk + every test
                  fixture update; out of scope for the
                  single-PR RCR-1 batch)
[~] RFC-090 §3.7  threat values
                  Field `country.threat_perception`
                  already exists on `core::CountryState`
                  since M1.1 and is loaded / saved /
                  diagnosed. No system currently *drives*
                  the value. RCR-1 ships 20 countries
                  with hand-authored threat_perception
                  initial values; the system that
                  computes / mutates it remains future
                  work (likely combined with §3.6 in
                  RCR-2).
[~] RFC-090 §3.8  simple military values
                  Field `country.military_power` already
                  exists on `core::CountryState` since
                  M1.1 and is loaded / saved / diagnosed.
                  No system currently *drives* the
                  value. RCR-1 ships 20 countries with
                  hand-authored military_power initial
                  values; the system that computes /
                  mutates it remains future work (likely
                  combined with §3.6 / §3.7 in RCR-2).
[ ] RFC-090 §3.9  annual world statistics                  — deferred to RCR-2
                  (new artefact bumps the unconditional
                  artefact contract from 10 to 11;
                  requires runner wiring + opt-in CLI
                  flag pattern mirroring M1.14 / M1.16
                  + new tests; out of scope for RCR-1)
[~] RFC-090 §3.10 full 1930–2000 automated test aligned to
                  RFC-090 M3                               — RCR-1 (partial)
                  RCR-1 ships a 365-day automated test
                  against the 20-country compliance
                  scenario in
                  tests/integration/rcr_1_rfc_compliance_test.cpp
                  ("compliance scenario survives a
                  365-day run with zero sanity issues").
                  The full 1930–2000 (25567-day) sweep
                  aligned to AI auto-policy + annual
                  stats + relationships is deferred
                  until §3.5 apply path + §3.6 + §3.9
                  ship in RCR-2.
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
[ ] RFC-090 §5.3  WeightModifier model                      — deferred to RCR-2
                  (schema addition on EventDefinition;
                  requires save schema bump v16 -> v17 +
                  every event-related test fixture
                  update; batched with §5.4 / §5.12 in
                  RCR-2 under one save bump)
[ ] RFC-090 §5.4  EventOption model                         — deferred to RCR-2
                  (same save-bump batching as §5.3)
[ ] RFC-090 §5.6  event weights system                      — deferred to RCR-2
                  (depends on §5.3 model)
[ ] RFC-090 §5.7  weighted event selection (current
                  selection is deterministic
                  ANY-entity-satisfies / AND across
                  triggers / first-in-vector, not
                  weighted)                                 — deferred to RCR-2
                  (depends on §5.3 / §5.6)
[ ] RFC-090 §5.8  event options / player choices on
                  EventInstance                             — deferred to RCR-2
                  (depends on §5.4 model)
[ ] RFC-090 §5.9  per-fire event log artefact (events.jsonl
                  emission for fired EventInstances; current
                  state.event_history is in-memory only)    — deferred to RCR-2
                  (small firer change but flips the M5.9
                  "events.jsonl semantics unchanged"
                  invariant and would rebake M5.9 tests;
                  batched with the other M5-extension
                  items so the M5.9 invariant migrates
                  once)
[X] RFC-090 §5.10 10 event definitions                      — RCR-1
                  (2 canonical events from
                  data/events/1930_core_events.json +
                  8 extended events from new
                  data/events/1930_rfc_extended_events.json;
                  the compliance scenario references both
                  files. The extended events deliberately
                  use thresholds tuned NOT to fire on
                  the compliance scenario's authored
                  initial values — same canonical-non-fire
                  property M5 added.)
[ ] RFC-090 §5.11 10-year event stress test                 — deferred to RCR-2
                  (needs a stress scenario with
                  thresholds tuned to fire + ties into
                  §5.9 fire-record emission for
                  assertions)
[ ] RFC-090 §5.12 event-chain / followup-event model        — deferred to RCR-2
                  (schema addition on EventDefinition;
                  batched with §5.3 / §5.4 under one
                  save bump)
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
[~] RFC-010 §2.2  AI countries auto-select policies         — RCR-1 (partial)
                  Selection-only skeleton shipped
                  (`leviathan::systems::ai_policy::select_policies`).
                  Apply path deferred to RCR-2 (see §6.1
                  RFC-090 §3.5 entry above).
[ ] RFC-010 §5    annual statistics CSV (current CSVs are
                  monthly)                                  — deferred to RCR-2
                  (see §6.1 RFC-090 §3.9 entry above)
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
  and hidden-truth direction (M6 in progress at M6.5
  covers part of the hidden-truth side).
- `rfc/RFC-080-research-formulas.md` — formula expansions
  M1.7 / M1.8 stripped down (WelfareSatisfaction /
  EconomicGrowth was added in M1.12; InflationPressure /
  WarDamage / InequalityProxy / WarWeariness /
  BudgetCrisis still deferred).

## 7. What this PR explicitly does NOT do

This audit doc is the entirety of the substantive content
of the governance-alignment PR that introduces it. The PR
ships alongside:

- A pointer block at the top of root `README.md`'s `## Status`.
- A new `## RFC compliance / milestone drift` section in
  `rfc/README.md`.
- A pointer paragraph in `docs/README.md`'s `## What's next`.
- Top-of-file governance notes inserted into
  `docs/milestone-3-result.md` and
  `docs/milestone-5-result.md`.

It deliberately does NOT:

- Change any code.
- Add any test.
- Change any save format (still v16).
- Edit any data fixture (still 3 countries, 10 policies,
  2 events, 3 factions, 3 canonical interest groups).
- Touch any system header or implementation file under
  `include/leviathan/` or `src/leviathan/`.
- Implement M6.6 or any other in-progress sub-milestone.
- Close issue #105 automatically — closing waits for the
  reviewer.
- Mark M6 as closed.
- Rewrite `rfc/RFC-090-roadmap.md` or
  `rfc/RFC-010-prototype-v0_1.md` themselves. The roadmap
  RFC stays the source of truth for the intended scope;
  this audit doc is the source of truth for what shipped
  vs. what is deferred.

## 8. How a future PR clears a deferred item

When a future PR implements one of the bullets in §6:

1. The implementing PR opens under the **`RCR-N`**
   recovery-track identifier (preferred), or — if the
   work cleanly aligns with an upcoming RFC milestone — a
   matching RFC milestone PR. **Do not invent new
   milestone numbers that don't map to an RFC section** —
   that lesson is captured in `docs/milestone-3-result.md`
   §7 and in memory file
   `feedback_rfc_milestone_alignment`. RCR identifiers are
   the safe default for clearing audit-doc items; see §5.1
   for the rule.
2. The implementing PR edits this audit doc inline:
   - Change `[ ]` to `[X]` on the line that ships.
   - Use `[~]` when an item is only partially cleared and
     write a one-paragraph note explaining what shipped
     vs. what remains (see RCR-1's `[~]` entries for the
     pattern).
   - Add the PR / RCR identifier at the end of the line:
     `[X] RFC-090 §5.10 10 event definitions — RCR-N`
     or `[X] RFC-090 §3.5 AI policy selection — PR #NNN`.
3. If a future PR ships an *implementation milestone* that
   maps to the original RFC scope cleanly, its close-out
   doc can be both an implementation close-out **and** a
   full original RFC acceptance close-out — say so
   explicitly at the top.
4. Once every bullet in §6.1 / §6.2 / §6.3 is `[X]`, this
   audit doc gets a final "RFC-090 / RFC-010 v0.1 fully
   satisfied as of PR #NNN" note at the top and stops
   being load-bearing for new work. `RCR-*` identifiers
   then stop being load-bearing too.
