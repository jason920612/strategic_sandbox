# M6 closeout audit

- Status: **AUDIT COMPLETE — M6 REMAINS OPEN**
- Type: Closeout audit + representative residual implementation
- Scope: RFC-090 §M6 (hidden truth / information distortion) and
  the RFC-080 §8 information-accuracy / bias-noise formula it
  implements
- Date authored: 2026-05-19
- Authoring approval gate: **only Jason may declare "M6 closed"**;
  this document explicitly does NOT.

## 1. Executive decision

M6 closeout audit completed; M6 remains open pending remaining
RFC-080 §8 blockers.

The M6.x sub-milestones M6.1 through M6.9 have all shipped
through individual PRs (#100 → #117). Each ship was reviewed as
an *implementation* milestone close-out, not a *full original
RFC acceptance* close-out (per the convention in
`rfc/README.md` introduced after RCR-1). RFC-090 §M6 phrasing
("玩家看到的不一定是真相") is now observable through the engine
path:

- Events emit `publicText` (RFC-060 §3 vocabulary, sourced from
  M6.2 `EventDefinition.visible_report`) on every fire.
- Non-debug `events.jsonl` filters out the M6.1 `true_cause`
  string.
- Debug mode reveals `true_cause` verbatim.
- Distortion fields (`information_accuracy`,
  `reported_intensity`, `noise_sample`) appear on country-
  anchored fires in both modes.

What RFC-080 §8 actually specifies — the *Information Accuracy
formula* and the `ReportedValue = TrueValue + Bias + Noise`
composition — is only *partially* implemented:

- **Implemented accuracy positives**: `+ IntelligenceCapacity`
  (M6.6 split into capability + budget) and `+ MediaFreedomSignal`
  (shipped here as a representative residual).
- **Implemented accuracy negative**: `- Corruption` (M6.7).
- **Implemented Bias term**: `+ PropagandaBias` (shipped here as
  a representative residual).
- **Implemented Noise**: M6.5 deterministic hash sample with
  amplitude `1 - InformationAccuracy`.

What is still missing per RFC-080 §8 and the closure binding
declared in the goal:

- Five accuracy modifiers (`+ BureaucraticProfessionalism`,
  `+ AuditCapacity`, `- FactionCapture`, `- LeaderIsolation`,
  `- LocalAutonomyOpacity`).
- Two bias terms (`FactionInterestBias`,
  `BureaucraticSelfProtection`).
- Per-event `TrueValue` source. The event_firer still anchors
  every fired event to `TrueValue = 1.0`, which fails the goal's
  "Fixed TrueValue=1.0 不足以 close" binding.
- Separate player-facing `EventReport` artefact. The goal binds
  this as an explicit closeout blocker.

Recommendation: **do NOT close M6**. Treat the remaining items
above as a deferred-scope backlog (see §9), to be addressed
through future PRs each scoped narrowly to one residual term or
artefact. The closure decision is reserved for Jason.

## 2. M6.1 – M6.9 implementation-milestone compliance matrix

| Sub-milestone | RFC-090 §M6 phrasing                  | PR     | Status | Notes                                                                              |
| ------------- | ------------------------------------- | ------ | ------ | ---------------------------------------------------------------------------------- |
| M6.1          | `6.1 為事件加入 true_cause`           | #100   | [X]    | EventDefinition.true_cause required non-empty at load                              |
| M6.2          | `6.2 加入 visible_report`             | #101   | [X]    | EventDefinition.visible_report required non-empty at load                          |
| M6.3          | `6.3 實作 information_accuracy`       | #102   | [X]    | Helper shape shipped; M6.6 / M6.7 / closeout audit added formula bodies            |
| M6.4          | `6.4 實作 reported value`             | #103   | [X]    | `reported_value::from_true_value` — placeholder linear damp; remains M6.4 semantic |
| M6.5          | `6.5 實作 bias/noise`                 | #104   | [X]    | Deterministic FNV-1a + splitmix64 hash, no `state.rng` consumption                 |
| M6.6          | `6.6 加入情報預算影響`                | #113   | [X]    | Adds intel_capability + budget.intelligence positive contributors                  |
| M6.7          | `6.7 加入腐敗影響`                    | #114   | [X]    | Adds corruption negative term; strict validation                                   |
| (hardening)   | strict-numeric / asymptotic ratio add | #115   | [X]    | Cross-cutting hardening — not an M-milestone (per `feedback_hardening_not_milestone`) |
| M6.8          | `6.8 debug 模式顯示真相`              | #116   | [X]    | true_cause metadata key always recorded; jsonl writer filters in non-debug         |
| M6.9          | `6.9 非 debug 模式隱藏真相`           | #117   | [X]    | publicText + distortion metadata on event_fired log entries                        |

All RFC-090 sub-milestones M6.1–M6.9 are implementation-shipped.
**The implementation milestone scope ≠ the original RFC scope.**
RFC-090 §M6's "玩家看到的不一定是真相" is observable, but RFC-080
§8's full information-accuracy and bias-noise formulas are NOT
fully ground-implemented — see §3 and §5.

## 3. RFC-050 / RFC-060 / RFC-080 matrix

| RFC                | Clause                                      | Status     | Where                                                                                                |
| ------------------ | ------------------------------------------- | ---------- | ---------------------------------------------------------------------------------------------------- |
| RFC-050 §5         | "隱藏真相": true / govt-report / media      | partial    | true_cause + publicText shipped; "media narrative" not modelled                                      |
| RFC-050 §6         | Accuracy influences (8 inputs)              | partial    | 3/8 inputs land in helper (intel, corruption, media); 5/8 deferred — see §5                          |
| RFC-050 §7         | Event-data field list (12 fields)           | partial    | id_code / category / triggers / visible_report / true_cause / options / effects / followup ship; `weight_modifiers` ship; `information_accuracy_required` / `source_type` / `tags` NOT shipped |
| RFC-060 §3         | `EventLogEntry { date, type, title, publicText, debugTruth }` | partial | `date`, `type`, `publicText`, `debugTruth` (= `true_cause`) emit on `event_fired` log entries; `title` lives in `message` only |
| RFC-080 §8         | InformationAccuracy formula (full)          | partial    | 4/9 RFC §8 terms wired (BaseAccuracy + IntelligenceCapacity{cap, bud} + MediaFreedomSignal − Corruption); 5/9 deferred — see §5 |
| RFC-080 §8         | `ReportedValue = TrueValue + Bias + Noise`  | partial    | Bias has 1/3 terms (PropagandaBias); Noise present (M6.5); TrueValue is anchor 1.0 (closure blocker) |
| RFC-080 §8         | `Noise = RandomNormal(0, 1 - InformationAccuracy)` | game-model approximation | Replaced with deterministic-hash sample in `[-amp, +amp]`; same monotonicity in `1 - accuracy`; non-RNG by reviewer call (PR #103) |

## 4. Shipped evidence (engine-observable from a normal `leviathan.exe` run)

All claims below are observable from `leviathan.exe --commands … --debug` /
`leviathan.exe` runs on the canonical or compliance scenario,
without invoking helper-only test paths.

- **M6.1 true_cause loaded into state.events**: every event JSON
  in `data/events/*.json` authors `true_cause`; loader rejects
  empty / missing.
- **M6.2 visible_report loaded into state.events**: same as
  M6.1; loader rejects empty / missing.
- **M6.3 / M6.6 / M6.7 / closeout-audit information_accuracy
  helper**: `information_accuracy::compute_for_country` returns
  a strict-validated `[0, 1]` ratio for any country in state;
  called from `event_firer::record_match` and
  `event_firer::record_followup` on every country-anchored
  event fire.
- **M6.4 reported_value helper**: pure `(true_value, accuracy)
  → reported = true_value × accuracy`. Engine consumer in
  event_firer with `TrueValue = 1.0` anchor (closure blocker —
  see §9).
- **M6.5 bias_noise helper**: pure deterministic FNV-1a +
  splitmix64 hash producing `[-amp, +amp]` samples. Engine
  consumer in event_firer with `amplitude = 1 - accuracy`.
- **M6.8 debug truth in artefact**: `true_cause` metadata key
  appended to every `event_fired` log entry;
  `logging::export_jsonl` filters the key out unless
  `RunnerOptions::debug_mode == true`.
- **M6.9 publicText / distortion in artefact**: `publicText`
  metadata key on every `event_fired` log entry; numeric
  distortion keys on country-anchored fires.
- **M6 closeout-audit MediaFreedomSignal in accuracy formula**:
  `information_accuracy::compute_for_country` now reads
  `government_authority.media_control` and contributes
  `(1 - media_control)` weighted by
  `kInformationAccuracyMediaFreedomWeight = 0.20`. Strict-
  validated as a `[0, 1]` ratio.
- **M6 closeout-audit PropagandaBias as new metadata key**:
  `propaganda_bias::compute_for_country` returns
  `kPropagandaBiasMaxMagnitude × media_control` (max 0.3) and
  is emitted as `propaganda_bias_sample` between
  `information_accuracy` and `reported_intensity` on every
  country-anchored fire.

Hardening (PR #115, not an M-milestone): every M6 helper now
returns `core::Result<T>` and rejects non-finite / out-of-range
ratio inputs loudly. `feedback_no_silent_degradation` and
`feedback_api_signature_expresses_failure` are honoured at the
helper boundary; the lone remaining `std::clamp` in
`information_accuracy.cpp` is a documented single-ULP floating-
point safety net (explicitly NOT an input-validation fallback —
see the in-file comment), not a defensive clamp on bad input.

## 5. RFC-080 §8 residual classification

Each residual records: RFC source / status / needed fields /
existing fields / research grounding / formula shape / evidence
vs assumption / tests / shipped here / remaining work.

### 5.1 Bias terms

#### FactionInterestBias (Bias, deferred)

- **RFC source**: RFC-080 §8 `Bias = FactionInterestBias + ...`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a faction's interest *direction* relative to
  the report's content. No existing CountryState / FactionState /
  InterestGroupState field encodes a signed alignment between a
  faction's interest and the event being reported.
- **Existing fields**: `FactionState.influence` (clout),
  `FactionState.preferred_policies` (string list), per-IG
  `radicalism` / `loyalty`. None of these say "this faction
  wants reports about this *event* skewed in *this* direction".
- **Research grounding**: V-Dem (faction influence on policy
  reporting); Egorov, Guriev & Sonin (QJP 2009) on selective
  reporting under regime constraints. Direction is
  research-defensible (factions skew reports toward favoured
  policies); magnitude is a game-model assumption.
- **Formula shape (proposal)**:
  `FactionInterestBias = w_faction × Σ_f (influence_f × alignment_f(event))`
  where `alignment_f(event) ∈ [-1, +1]` requires per-(faction,
  event-category) authoring.
- **Evidence vs assumption**: directional yes; coefficient and
  per-event alignment table are pure authoring.
- **Tests**: would need per-faction × per-event-category
  alignment fixtures + scorer tests, deterministic across
  reload.
- **Shipped here**: NO.
- **Remaining work**: design + author the (faction-category,
  event-category) alignment table; ship `faction_interest_bias`
  helper sibling to `propaganda_bias`; wire into event_firer as
  `faction_interest_bias_sample` metadata; bump artefact and
  tests.

#### BureaucraticSelfProtection (Bias, deferred)

- **RFC source**: RFC-080 §8 `Bias = ... + BureaucraticSelfProtection + ...`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a "bureaucracy faction is reporting on its
  own performance" signal. Could read
  `country.government_authority.bureaucratic_compliance` (M2.16,
  [0, 1]) as the strength of bureaucratic capture of the
  reporting channel, but the polarity ("low compliance → more
  self-protective filtering" vs "high compliance → more
  state-aligned filtering") is contestable.
- **Existing fields**: `government_authority.bureaucratic_compliance`,
  any IG with `kind = Bureaucracy` (radicalism / loyalty /
  influence). Both surfaces are plausible inputs.
- **Research grounding**: Bendor & Meirowitz "Spatial Models of
  Delegation" (APSR 2004); Banerjee, Hanna & Mullainathan
  "Corruption" (Handbook 2013). Both establish that bureaucracies
  filter reports to protect their position; neither pins a
  coefficient.
- **Formula shape (proposal)**:
  `BureaucraticSelfProtection = w_bsp × (1 - bureaucratic_compliance)`
  on the assumption that low compliance proxies for "the
  bureaucracy is filtering reports to protect itself from
  central oversight". Game-model assumption per RFC-080 §11.
- **Evidence vs assumption**: directional yes; polarity choice
  needs design review; coefficient is game-model.
- **Tests**: ratio sweep + strict validation, like
  `propaganda_bias_test.cpp`.
- **Shipped here**: NO.
- **Remaining work**: pick polarity (above or its inverse);
  ship `bureaucratic_self_protection` helper; wire into firer;
  bump artefact and tests.

#### PropagandaBias (Bias, **shipped here**)

- **RFC source**: RFC-080 §8 `Bias = ... + PropagandaBias`
- **Status**: SHIPPED in this PR as a representative residual.
- **Needed fields**: a "state suppresses independent media"
  signal. `government_authority.media_control` (M2.16, [0, 1])
  is exactly that.
- **Existing fields**: `government_authority.media_control`.
- **Research grounding**: V-Dem propaganda indicators;
  Egorov, Guriev & Sonin (QJP 2009); King, Pan & Roberts (APSR
  2013); DellaVigna & Kaplan (QJE 2007) on quantified media
  effects. Directional grounding is strong; magnitude is a
  game-model assumption per RFC-080 §11.
- **Formula shape (shipped)**:
  `propaganda_bias = kPropagandaBiasMaxMagnitude × media_control`
  with `kPropagandaBiasMaxMagnitude = 0.3`. Sign: positive
  (propaganda inflates the state-favoured narrative).
- **Evidence vs assumption**: directional yes; numeric
  coefficient is game-model — explicitly flagged in helper
  comment.
- **Tests**: `tests/systems/propaganda_bias_test.cpp` —
  formula matching, monotonicity, range bounds, NaN/Inf/out-of-
  range rejection, state-immutability, determinism.
- **Shipped here**: YES.
- **Remaining work for full RFC §8 closure**: wire into a
  `perceived_intensity = TrueValue + Bias + Noise` composition
  (currently emitted as separate metadata keys for a future
  EventReport artefact to compose).

### 5.2 Accuracy modifiers

#### FactionCapture (negative accuracy modifier, deferred)

- **RFC source**: RFC-080 §8 `- FactionCapture`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a signal that a specific faction has
  *captured* the reporting channel. Plausible input:
  `FactionState.influence × FactionState.radicalism` for the
  bureaucracy / intelligence / media faction in a country.
- **Existing fields**: per-country faction list with
  influence / radicalism / loyalty.
- **Research grounding**: Stigler "The Theory of Economic
  Regulation" (Bell J 1971) on regulatory capture; Boas &
  Hidalgo (APSR 2011) on capture of administrative agencies.
  Establishes the concept; no numeric Game-coefficient.
- **Formula shape (proposal)**: pick a single faction kind
  (e.g. Bureaucracy or Intelligence); compute
  `capture = influence × radicalism` for that kind's strongest
  in-country instance.
- **Evidence vs assumption**: directional yes; choice of
  faction kind and coefficient is game-model.
- **Tests**: per-faction sweep; missing-faction is success
  (capture = 0).
- **Shipped here**: NO.
- **Remaining work**: design + ship.

#### LeaderIsolation (negative accuracy modifier, deferred)

- **RFC source**: RFC-080 §8 `- LeaderIsolation`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a signal that the leader is isolated from
  honest reporting. No existing CountryState field maps; would
  need a new `leader_isolation` ratio on CountryState OR derive
  from `(1 - bureaucratic_compliance) × (1 - media_freedom)`
  composite.
- **Existing fields**: indirect — `bureaucratic_compliance`
  + `media_control`. Both already feed other RFC §8 terms;
  reusing them for LeaderIsolation would double-count.
- **Research grounding**: Tullock "Autocracy" (1987); Geddes,
  Wright & Frantz "How Dictatorships Work" (2018) on the
  authoritarian leader's information problem. Established
  concept; no coefficient.
- **Formula shape (proposal)**: NEW persistent field
  `country.leader_isolation` in [0, 1] authored per country;
  bump save schema; loader / save / diagnostics / tests +
  baseline rebake. Alternatively: a derived term, but then
  needs a careful "no double-count" argument.
- **Evidence vs assumption**: directional yes; coefficient
  and field choice are game-model.
- **Tests**: schema test, load/save round-trip, ratio sweep
  with strict validation, plus rebake of every determinism
  baseline that touches save bytes.
- **Shipped here**: NO.
- **Remaining work**: pick field-vs-derived; if field,
  schema bump + author every country; if derived, document
  the no-double-count argument.

#### LocalAutonomyOpacity (negative accuracy modifier, deferred)

- **RFC source**: RFC-080 §8 `- LocalAutonomyOpacity`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a signal that local government withholds
  information from the centre. `1 - central_control` is the
  obvious input (M1.1, [0, 1]).
- **Existing fields**: `country.central_control`.
- **Research grounding**: Besley & Persson (RFC-080 §2) on state
  capacity; Wallis "Federalism and Local Government" (Handbook of
  Public Economics) on subnational reporting incentives.
  Directional yes; coefficient is game-model.
- **Formula shape (proposal)**:
  `LocalAutonomyOpacity = w_lao × (1 - central_control)`
- **Evidence vs assumption**: directional yes; coefficient is
  game-model.
- **Tests**: ratio sweep + strict validation.
- **Shipped here**: NO.
- **Remaining work**: ship a small helper sibling to
  `information_accuracy`'s media-freedom term — or extend the
  `information_accuracy` helper with one more positive-axis
  contributor (closer in shape to media-freedom).

#### MediaFreedomSignal (positive accuracy modifier, **shipped here**)

- **RFC source**: RFC-080 §8 `+ MediaFreedomSignal`
- **Status**: SHIPPED in this PR as a representative residual.
- **Needed fields**: a "free press" signal.
  `1 - government_authority.media_control` (M2.16, [0, 1]).
- **Existing fields**: `government_authority.media_control`.
- **Research grounding**: V-Dem methodology (decomposes press
  freedom + media bias as first-class regime variables);
  Egorov, Guriev & Sonin (QJP 2009) on dictators tolerating
  freer media specifically as an information aggregator.
  Directional grounding strong; coefficient is game-model.
- **Formula shape (shipped)**: composed as an outer weighted
  blend with the intelligence-pair score:
  `positive_axis = (1 - w_media) × intel_score + w_media × (1 - media_control)`
  with `w_media = kInformationAccuracyMediaFreedomWeight = 0.20`.
  Preserves the inner `kInformationAccuracyCapabilityWeight +
  kInformationAccuracyBudgetWeight = 1.0` invariant.
- **Evidence vs assumption**: directional yes; coefficient is
  game-model — flagged in the helper comment.
- **Tests**: `tests/systems/information_accuracy_test.cpp` —
  MediaFreedomSignal contribution monotonicity, ceiling /
  floor at maxed and minimal inputs, formula match,
  NaN/Inf/out-of-range rejection, deterministic short-circuit.
- **Shipped here**: YES.
- **Remaining work**: none for this term itself.

#### BureaucraticProfessionalism (positive accuracy modifier, deferred)

- **RFC source**: RFC-080 §8 `+ BureaucraticProfessionalism`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a "bureaucracy is staffed by trained, non-
  partisan professionals" signal. No existing CountryState
  field directly maps. `country.administrative_efficiency`
  (M1.1, [0, 1]) is adjacent but not the same concept; reusing
  it would conflate execution quality with reporting honesty.
- **Existing fields**: `administrative_efficiency`,
  `bureaucratic_compliance` (M2.16) — neither is a clean fit.
- **Research grounding**: Evans & Rauch "Bureaucracy and Growth"
  (ASR 1999) on meritocratic recruitment as an institutional
  variable; V-Dem state-bureaucracy indicators. Concept is well-
  attested; no numeric coefficient.
- **Formula shape (proposal)**: NEW persistent field
  `government_authority.bureaucratic_professionalism` in [0, 1];
  save schema bump; loader / save / diagnostics / tests.
- **Evidence vs assumption**: directional yes; field design is
  scope work; coefficient game-model.
- **Tests**: schema + ratio sweep + strict validation +
  determinism rebakes.
- **Shipped here**: NO.
- **Remaining work**: design + ship the new field, then a
  helper extension.

#### AuditCapacity (positive accuracy modifier, deferred)

- **RFC source**: RFC-080 §8 `+ AuditCapacity`
- **Status**: NOT shipped. Closure blocker.
- **Needed fields**: a "the state has an independent audit
  function" signal. No existing field. Adjacent fields:
  `central_control`, `fiscal_capacity` (M1.1) — both speak to
  state capacity but not specifically to audit independence.
- **Existing fields**: none rigorous.
- **Research grounding**: Besley & Persson "Origins of State
  Capacity" (RFC-080 §2); World Bank Public Expenditure
  Tracking Surveys; Olken & Pande "Corruption in Developing
  Countries" (Annu Rev Econ 2012). Direction established; no
  numeric coefficient.
- **Formula shape (proposal)**: NEW persistent field
  `government_authority.audit_capacity` in [0, 1];
  save schema bump; loader / save / tests.
- **Evidence vs assumption**: directional yes; field design is
  scope work; coefficient game-model.
- **Tests**: schema + ratio sweep + strict validation +
  determinism rebakes.
- **Shipped here**: NO.
- **Remaining work**: design + ship.

### 5.3 Other RFC-080 §8 closure blockers

#### Per-event TrueValue source (closure blocker, deferred)

- **RFC source**: RFC-080 §8
  `ReportedValue = TrueValue + Bias + Noise`. TrueValue is the
  per-event truth anchor; it cannot be a constant if the formula
  is to mean what the RFC says.
- **Status**: NOT shipped. The event_firer pins
  `TrueValue = 1.0` for every fired event (`event_firer.cpp`
  inside `compute_distortion_fields`).
- **Goal binding**: "Fixed TrueValue=1.0 不足以 close;需
  per-event TrueValue source. No fallback to 1.0."
- **Needed fields**: per-event truth intensity. Options below.
- **Existing fields**: none usable. EventDefinition has no
  intensity field.
- **Design proposal (recommended)**: add a new persistent field
  `EventDefinition.true_intensity: double` (finite, > 0).
  Required at load. Save schema bump v18 → v19. All 10 events
  in `data/events/*.json` author the field. Loader rejects
  missing / non-finite / non-positive. event_firer reads
  `definition.true_intensity` instead of the literal `1.0`.
- **Alternative**: deterministic derivation from the trigger
  threshold's distance — e.g. `true_intensity = trigger.value -
  state_value` for the satisfied trigger. Risk: trigger
  thresholds and effect magnitudes share the same numeric scale,
  so the derivation overloads the trigger semantics; AND a
  multi-trigger event has no canonical "distance". The recommended
  authored-field approach is simpler.
- **Research grounding**: not applicable — this is a *modelling
  surface*, not a coefficient. The authored field is the
  cleanest extension consistent with the RFC.
- **Evidence vs assumption**: 100% authoring decision.
- **Tests**: loader-rejection tests (missing, NaN, Inf,
  non-positive); save round-trip; event_firer tests confirming
  the new field flows into `reported_intensity`; canonical and
  compliance determinism baselines rebaked.
- **Shipped here**: NO (design only).
- **Remaining work**: ship in a dedicated PR. Estimated blast
  radius: save schema bump + loader + save + 10 event JSONs +
  event_firer + tests + determinism baselines.

#### Separate player-facing EventReport artefact (closure blocker, deferred)

- **RFC source**: RFC-050 §6 / §7 narrative split between
  truth-side and player-side; RFC-060 §5 artefact list.
- **Status**: NOT shipped. The non-debug `events.jsonl`
  artefact filters `true_cause` out but is otherwise the SAME
  artefact for the player and the debugger; the goal binds a
  SEPARATE player-facing artefact.
- **Goal binding**: "Separate player-facing EventReport
  artifact 是 closeout blocker."
- **Design proposal (recommended)**:
  - **Filename**: `event_reports.jsonl` in `RunnerOptions::
    output_dir` (default `<output_dir>/event_reports.jsonl`).
    Unconditional artefact like `events.jsonl` /
    `annual_world_stats.csv`. Bumps artefact contract from 11
    to 12.
  - **Per-record schema** (one JSON object per fired event):
    `date`, `event_id_code`, `country_id_code`, `publicText`,
    `reported_intensity`, `propaganda_bias_sample`,
    `noise_sample`, `perceived_intensity`.
  - **`perceived_intensity` semantic**: RFC-080 §8 strict
    composition `TrueValue + Bias + Noise`. With this PR's
    shipped helpers,
    `perceived_intensity = TrueValue + propaganda_bias_sample
      + noise_sample`. When the per-event TrueValue blocker
    above also ships, TrueValue flows from
    `EventDefinition.true_intensity`.
  - **Filter contract**: `true_cause` MUST NEVER appear in
    `event_reports.jsonl`, in any mode. Debug mode does not
    relax this filter — the artefact is player-facing by
    definition. `true_cause` continues to surface through
    `events.jsonl` in debug mode and through `save.json`
    `logs` always (for replay determinism).
  - **Emitter**: a new free function in `logging_system` (e.g.
    `logging::export_event_reports_jsonl`) called from
    `runner::end_tick` alongside the existing `export_jsonl`
    call.
- **Research grounding**: not applicable — this is a modelling
  / artefact surface.
- **Evidence vs assumption**: 100% authoring decision.
- **Tests**: artefact contract bump (10 → 11 → 12); presence
  test on every scenario sweep; "no true_cause in
  event_reports.jsonl ever" pin in both debug and non-debug
  modes; per-record schema test; integration test verifying
  `perceived_intensity == reported_intensity + bias + noise`
  on a hand-built firing state.
- **Shipped here**: NO (design only).
- **Remaining work**: ship in a dedicated PR. Estimated blast
  radius: runner / end_tick / logging_system / RunnerOptions
  / tests; artefact-count bump everywhere it is asserted.

## 6. Representative scope shipped in this PR

This PR ships the M6 closeout audit and TWO representative
RFC-080 §8 residuals. Per the goal: "1 bias term + 1 accuracy
modifier + per-event TrueValue source design or implementation +
EventReport artifact design or implementation".

| Item                              | Required | Shipped here                        |
| --------------------------------- | -------- | ----------------------------------- |
| 1 bias term                       | yes      | YES — `PropagandaBias` (code)        |
| 1 accuracy modifier               | yes      | YES — `MediaFreedomSignal` (code)    |
| Per-event TrueValue source        | yes (design OR impl) | DESIGN — §5.3 above              |
| EventReport artefact              | yes (design OR impl) | DESIGN — §5.3 above              |
| M6 closeout audit doc             | yes      | YES — this file                      |

Both shipped residuals were chosen because they reuse the same
existing CountryState field (`government_authority.media_control`
on `GovernmentAuthorityState`, M2.16, [0, 1] strict-validated).
"Rigorously" per the goal here means: (a) reads a field that
already exists, (b) the field is already validated as a `[0, 1]`
ratio at the data layer, (c) the term has named research
direction in the cited literature, (d) the coefficient is
explicitly disclosed as a game-model assumption per RFC-080 §11.

What this PR deliberately does NOT do:

- No save schema bump (still v18; both residuals reuse
  pre-existing CountryState surface).
- No new state field.
- No new persistent artefact in this PR.
- No `EventDefinition.true_intensity` field (per-event TrueValue
  remains a remaining blocker — see §5.3).
- No claim that M6 is closed. M6 remains OPEN.
- No new RFC milestone number. This PR is an M6 closeout audit,
  NOT M7 (per `feedback_milestone_direction_gate`) and NOT
  RCR-2 (per `feedback_rcr_recovery_track`).
- No new player-facing command, no new player-facing CLI flag,
  no new gameplay mechanic.

The shipped numeric changes are:

- `information_accuracy::compute_for_country` now reads
  `government_authority.media_control` and returns a different
  numeric value for any country with non-zero `media_control`.
- `propaganda_bias::compute_for_country` is a new helper.
- `event_firer::record_match` / `record_followup` emit
  `propaganda_bias_sample` as a new metadata key (10th key on
  country-anchored fires; 6th on vacuous-actor fires).

Per `feedback_pr_framing_precision`: this PR DOES change a
mechanics surface (the information-accuracy formula and the
per-fire metadata schema). It does NOT introduce a new RFC-090
milestone feature, a new player-facing command, a new save
schema field, or a new artefact.

## 7. Tests and verification

Test totals on the M6 closeout-audit tree:

- `cmake --build build --config Debug` succeeds.
- `build/bin/Debug/leviathan_tests.exe` reports
  **1301 test cases, 96221 assertions, 0 failed**.
- The pre-audit count was 1276 test cases, 96033 assertions
  (per memory `project_milestone_state.md`). Delta:
  - +13 new tests in `propaganda_bias_test.cpp`.
  - +6 new MediaFreedomSignal tests in `information_accuracy_test.cpp`.
  - +6 renamed / extended tests in `information_accuracy_test.cpp`
    pre-existing surface (no functional regression).
- Canonical `1930_minimal` 365-day sweep: byte-identical
  determinism baseline preserved (canonical events tuned not to
  fire; no event-firer-emitted metadata changes; no information_accuracy
  consumer outside event_firer).
- Compliance `1930_rfc_compliance` 25567-day sweep: passes (per
  `tests/integration/rcr_1_rfc_compliance_test.cpp`). Numeric
  metadata bytes in `events.jsonl` change because the
  information_accuracy formula now reads `media_control`; the
  integration test only checks key PRESENCE, not byte values.
- Debug / non-debug toggle: events.jsonl behaviour unchanged for
  the debug-gating contract (M6.8). The new
  `propaganda_bias_sample` key is emitted in BOTH modes, just
  like the other M6.9 distortion keys (it is a distortion
  observation, not a truth observation).
- Strict failure surfaces: `propaganda_bias::compute_for_country`
  rejects NaN / ±Inf / out-of-range `media_control` loudly;
  rejects invalid CountryId loudly; rejects empty
  `state.countries` loudly. `information_accuracy::
  compute_for_country` additionally rejects bad `media_control`
  on the same field, with the validation ordering
  cap → bud → corruption → media_control (deterministic
  short-circuit).
- Save format: v18 unchanged.
- Artefact contract: 11 unchanged (the new
  `propaganda_bias_sample` is a metadata key on an existing
  artefact, not a new artefact).
- `state.rng` consumption: unchanged (none of the new helpers
  consumes RNG).

## 8. Docs sweep

Stale wording removed / corrected in this PR:

- README.md / docs/README.md / rfc/README.md milestone state
  lines updated to reflect "M6.1–M6.9 shipped; closeout audit
  ran but M6 REMAINS OPEN pending RFC-080 §8 blockers".
- `docs/rfc-090-010-compliance-audit.md` extended with the
  closeout-audit deferred-scope backlog.
- `m6-9-non-debug-mode-distortion.md` references the closeout-
  audit additions where it discusses the metadata schema.

No "M6.6 latest" / "M6.7 latest" / "M6.8 next" / "publicText
deferred" / "debugTruth incomplete" / "visible_report emitted as
event metadata" / "defensive clamp" / "UI complete" / "M6 closed"
wording remains in the documentation.

## 9. Remaining blockers (consolidated)

For closure of M6 per the goal binding, the following must ALL
ship and Jason must explicitly approve the closure statement.

### 9.1 RFC-080 §8 Bias terms still missing

- [ ] FactionInterestBias (see §5.1)
- [ ] BureaucraticSelfProtection (see §5.1)

### 9.2 RFC-080 §8 Accuracy modifiers still missing

- [ ] FactionCapture (see §5.2)
- [ ] LeaderIsolation (see §5.2)
- [ ] LocalAutonomyOpacity (see §5.2)
- [ ] BureaucraticProfessionalism (see §5.2)
- [ ] AuditCapacity (see §5.2)

### 9.3 Other closure blockers

- [ ] Per-event TrueValue source (see §5.3) — RFC-080 §8 strict
- [ ] Separate player-facing EventReport artefact (see §5.3) —
  RFC-050 §6 / §7, RFC-060 §5

### 9.4 Optional but recommended cleanup

- The M6.4 `reported_value::from_true_value` placeholder
  (multiplicative damp) is no longer the only path the engine
  needs once the per-event TrueValue source and the
  EventReport artefact ship. The strict RFC-080 §8 additive
  composition (`reported = TrueValue + Bias`, noise separate)
  can replace the multiplicative placeholder in the engine path
  once both blockers above clear. Until then, the multiplicative
  damp is kept as a placeholder to preserve the M6.9
  `reported_intensity` semantic that already-shipped artefacts
  promise.

### 9.5 What "research" backs each residual

| Residual                       | Direction grounded? | Coefficient grounded? | Authoring required?      |
| ------------------------------ | ------------------- | --------------------- | ------------------------ |
| FactionInterestBias            | yes (V-Dem; Egorov-Guriev-Sonin) | no  | per (faction, event) alignment table |
| BureaucraticSelfProtection     | yes (Bendor-Meirowitz; Banerjee-Hanna-Mullainathan) | no | polarity choice; coefficient |
| PropagandaBias (shipped)       | yes (V-Dem; King-Pan-Roberts; DellaVigna-Kaplan) | no | coefficient (`kPropagandaBiasMaxMagnitude`) |
| FactionCapture                 | yes (Stigler; Boas-Hidalgo) | no | faction-kind choice; coefficient |
| LeaderIsolation                | yes (Tullock; Geddes-Wright-Frantz) | no | new field OR derived; coefficient |
| LocalAutonomyOpacity           | yes (Besley-Persson; Wallis) | no | coefficient |
| MediaFreedomSignal (shipped)   | yes (V-Dem; Egorov-Guriev-Sonin) | no | coefficient (`kInformationAccuracyMediaFreedomWeight`) |
| BureaucraticProfessionalism    | yes (Evans-Rauch; V-Dem) | no | new field; coefficient |
| AuditCapacity                  | yes (Besley-Persson; Olken-Pande) | no | new field; coefficient |
| Per-event TrueValue            | not applicable (modelling surface) | not applicable | new EventDefinition field |
| EventReport artefact           | not applicable | not applicable | artefact contract bump |

**No paper proves any of the chosen coefficients.** The cited
literature establishes the *direction* of each term and pins
the conceptual content; numeric coefficients are explicit
game-model assumptions per RFC-080 §1 / §11, flagged in helper
comments per `feedback_numbers_from_research`.

## 10. Recommendation

1. **Treat this PR as the M6 closeout audit only.** Do not
   advance to M7 (派系深化) without Jason's explicit go-ahead,
   per `feedback_milestone_direction_gate`.
2. **Resolve §9 blockers in narrow PRs**, each scoped to one
   residual term or one of the two artefact-level blockers
   (per-event TrueValue, EventReport). The narrowest order is
   probably: per-event TrueValue → EventReport (because the
   artefact composes the TrueValue field), then the remaining
   bias / accuracy terms in any order.
3. **Each future PR must cite research and disclose game-model
   coefficients explicitly**, matching the pattern shipped by
   `PropagandaBias` / `MediaFreedomSignal` here.
4. **Do not write "M6 closed" anywhere** until Jason approves.
   The audit's executive decision (§1) is the authoritative
   summary of M6's status as of 2026-05-19: **OPEN**.
