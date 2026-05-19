# M6.9 — Non-debug mode hides the truth (RFC-090 §6.9)

- **RFC anchor:** RFC-090 §6.9 `非 debug 模式隱藏真相`.
- **Cross-references:**
  - RFC-050 §5 "隱藏真相" — three-layer truth model (truth / public
    report / media narrative).
  - RFC-060 §3 `EventLogEntry { ... publicText; debugTruth }` —
    canonical event-log shape. M6.8 shipped `debugTruth`; M6.9
    ships `publicText`.
  - RFC-080 §8 `ReportedValue = TrueValue + Bias + Noise` and
    `Noise = RandomNormal(0, 1 - InformationAccuracy)` —
    numeric distortion model.
- **Save format:** unchanged (`v18`).
- **Artefact contract:** unchanged (`11`).
- **State.rng:** zero consumption — bias_noise is hash-
  deterministic per its M6.5 contract.

## 1. What M6.9 ships

`event_firer::record_match` and `event_firer::record_followup`
compose the three M6 helpers into four metadata keys per
`event_fired` LogEntry, sourced from the matching
`EventDefinition` and the first-actor country:

| Metadata key | Source | Range |
|---|---|---|
| `visible_report` | M6.2 `EventDefinition.visible_report` (verbatim) | non-empty string |
| `information_accuracy` | M6.3 / M6.6 / M6.7 `information_accuracy::compute_for_country` | `[0, 1]` |
| `reported_intensity` | M6.4 `reported_value::from_true_value(1.0, accuracy)` | `[0, 1]` |
| `noise_sample` | M6.5 `bias_noise::sample_for_event(event_id, country_id, fired_on, 1 - accuracy)` | `[-amplitude, +amplitude]` |

All four keys are appended UNCONDITIONALLY at fire time (per-
event atomicity: if any helper fails, the EventInstance + the
LogEntry are NOT appended; see §3). The M6.8 `true_cause` key
continues to be filtered out of the events.jsonl artefact in
non-debug mode; the four M6.9 keys are emitted in BOTH debug
and non-debug modes.

## 2. Numeric anchor for the M6 distortion model

RFC-080 §8 declares `ReportedValue = TrueValue + Bias + Noise`.
M6.9 needs a `TrueValue` scalar per fired event. M6.9 fixes
`TrueValue = 1.0` ("the event happened with intensity 1.0").
Under this anchor:

- `reported_intensity = reported_value::from_true_value(1.0, accuracy) = 1.0 × accuracy = accuracy`.
- `noise_amplitude = 1 - accuracy` (RFC-080 §8: `Noise = RandomNormal(0, 1 - InformationAccuracy)`).
- `noise_sample ∈ [-(1-accuracy), +(1-accuracy)]`.
- Player's perceived intensity = `reported_intensity + noise_sample = accuracy + noise`.

Properties:

- Maxed intelligence (cap = 1, budget = 1, corruption = 0) →
  accuracy = 1.0 → reported = 1.0, noise = 0 → player sees the
  truth.
- Zero intelligence + maxed corruption → accuracy = 0.0 →
  reported = 0.0, noise ∈ [-1, +1] → player's perception is
  pure noise.
- Mid-range (e.g. cap = 0.5, budget = 0.0, corruption = 0.2) →
  accuracy ≈ 0.53 → reported ≈ 0.53, noise envelope ±0.47 →
  partial distortion.

The **bias** side of `Bias + Noise` is not yet modelled (would
be `FactionInterestBias + BureaucraticSelfProtection +
PropagandaBias` per RFC-080 §8). M6.9 ships Noise; Bias remains
deferred to a future M6.x sub-milestone with its own RFC anchor.

## 3. Atomicity and failure propagation

`event_firer::record_match` is promoted to
`Result<bool> record_match(...)` (and `record_matches` to
`Result<FireOutcome>`, `record_followup` to `Result<bool>`).
The composition pipeline can fail at any of three points:

1. `information_accuracy::compute_for_country` rejects a non-
   finite or out-of-range
   `intelligence_capability` / `budget.intelligence` /
   `corruption` (M6.7 strict validation).
2. `reported_value::from_true_value(1.0, accuracy)` rejects a
   non-finite accuracy (cannot happen on the success path of
   step 1, but the call is still strict).
3. `bias_noise::sample_for_event(...)` rejects an empty
   event_id_code / country_id_code or a non-finite / out-of-
   range amplitude.

On any failure the EventInstance is NOT appended to
`state.event_history` and the LogEntry is NOT appended to
`state.logs`. The event_engine call sites
(`event_engine::tick_events` + `recurse_followups_impl`) all
propagate the Result with the event id_code in the error
context. No silent fallback per
`feedback_no_silent_degradation` +
`feedback_api_signature_expresses_failure`.

Vacuous-actor case: if the match has no first-actor country
(`inst.actors.empty()` OR `first_actor.country_id_code.empty()`),
the distortion fields are SKIPPED but the call still
succeeds. The `visible_report` key is always emitted
regardless. This is not a silent degradation — vacuous events
are degenerate, not malformed; the M5.1 schema rejects them
at load time, so the firer can only see them via hand-built
test fixtures.

## 4. M6.8 invariants preserved

- The M6.8 `true_cause` metadata key is still attached
  unconditionally at fire time.
- `logging::write_jsonl_line` still filters `true_cause` from
  the events.jsonl artefact in non-debug mode; the four M6.9
  keys are NOT filtered (they are the player-facing surface,
  not the debug-side truth).
- `save.json` is byte-identical across the `--debug` toggle:
  state.logs.metadata contains the truth AND the distortion
  fields regardless of debug_mode; only the events.jsonl
  writer branches.

## 5. Determinism

- Same seed + same scenario → byte-identical `events.jsonl`
  AND byte-identical `save.json`. `bias_noise::sample_for_event`
  is a pure hash function over
  `(event_id_code, country_id_code, fired_on, amplitude)`;
  `state.rng` is NEVER consumed by the M6.9 path. Verified by
  the new `M6.9 integration: same seed produces deterministic
  distorted publicText` test.
- Canonical `1930_minimal` 365-day events.jsonl is BYTE-
  IDENTICAL to the PR #116 (M6.8) baseline because no events
  fire on the canonical scenario (M5 invariant preserved); no
  `event_fired` LogEntries means no distortion fields are
  ever appended, and the canonical artefact is unchanged.
- Compliance `1930_rfc_compliance` 25 567-day events.jsonl
  gains the four M6.9 keys per fired event line (41 events
  fire over 1930→2000; each gains four keys). Save.json gains
  the same metadata in `state.logs` serialisation. Sanity
  issues = 0 on both debug and non-debug runs.

## 6. What M6.9 deliberately does NOT do

- **No save schema bump.** The four new metadata keys live in
  `state.logs.metadata`, which is already a flexible key-value
  list; no new persistent struct field is added. Save format
  stays at `v18`.
- **No new artefact.** Artefact contract stays at 11.
- **No new player-facing command.** No new `PlayerCommandKind`.
- **No new save-layer surface.**
- **No new gameplay system module.**
- **No event-engine / event_evaluator change beyond
  propagating the new Result-bearing record_match /
  record_followup signatures.**
- **No `state.rng` consumption.** bias_noise stays
  hash-deterministic per its M6.5 contract.
- **No Bias term (yet).** RFC-080 §8's
  `FactionInterestBias + BureaucraticSelfProtection +
  PropagandaBias` is not yet modelled. A future M6.x sub-
  milestone with its own RFC anchor will add it.
- **No remaining RFC-080 §8 negative terms.**
  `-FactionCapture` / `-LeaderIsolation` /
  `-LocalAutonomyOpacity` and the positive
  `+MediaFreedomSignal` / `+BureaucraticProfessionalism` /
  `+AuditCapacity` are still RFC-090 backlog (no §6.x task
  number yet).
- **No M7 / no M6 close.** RFC-090 M6 stays at M6.9; M7
  (派系深化) is not started.

## 7. Tests

### Unit (`tests/systems/event_firer_test.cpp`)

- `M6.9 record_match: visible_report metadata mirrors
  EventDefinition.visible_report verbatim` — RFC-060 §3
  publicText source pin.
- `M6.9 record_match: high accuracy -> reported_intensity
  close to 1.0 and small noise envelope` — RFC-080 §8 maxed-
  accuracy corner.
- `M6.9 record_match: low accuracy + high corruption ->
  larger distortion envelope (noise amplitude rises)` —
  RFC-080 §8 zero-accuracy corner; cross-compares against
  the high-accuracy corner.
- `M6.9 record_match: same inputs -> deterministic distortion
  sample` — `feedback_trajectory_observation_tests` /
  byte-identical determinism check.
- `M6.9 record_match: non-finite country.intelligence_capability
  FAILS LOUDLY` — `feedback_no_silent_degradation` /
  per-event atomicity pin.
- `M6.9 record_followup: distortion uses parent's first-actor
  country` — followup-chain semantics.

### Integration (`tests/integration/m5_event_pipeline_test.cpp`)

- `M6.9 integration: non-debug events.jsonl emits distorted
  publicText; --debug additionally reveals true_cause` — the
  full M6.8 + M6.9 surface on a single firing event.
- `M6.9 integration: same seed produces deterministic
  distorted publicText` — full end-to-end determinism.
- `M6.9 integration: save format stays at v18 (no schema
  bump)` — strict save-version pin per
  `feedback_save_version`.

## 8. Forward work

- **Bias term**: RFC-080 §8 declares `Bias =
  FactionInterestBias + BureaucraticSelfProtection +
  PropagandaBias`. None of these are RFC-090 §6.x sub-
  milestones today. A future RFC update would add them.
- **Per-event TrueValue**: M6.9 fixes TrueValue = 1.0. Some
  events have natural numeric magnitudes (e.g. an effect
  delta of `-0.05`) that a future sub-milestone could
  surface as the TrueValue, producing a more informative
  reported_intensity. Out of scope here.
- **EventReport type / artefact**: a structured
  player-facing event log file (separate from events.jsonl)
  would let the player view their distorted history at a
  glance. Out of scope; UI milestone.
