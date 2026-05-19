# M6.8 — Debug mode reveals truth (RFC-090 §6.8)

- **RFC anchor:** RFC-090 §6.8 `debug 模式顯示真相`.
- **Cross-references:** RFC-050 §5 隱藏真相 (the truth-vs-report
  layering); RFC-060 §3 `EventLogEntry { ... publicText;
  debugTruth }` (the canonical event-log shape).
- **Save format:** unchanged (`v18`).
- **Artefact contract:** unchanged (`11`).
- **State.rng:** zero consumption — the flag is presentation-only.

## 1. What M6.8 ships

A single boolean toggle on the runner that controls whether the
events.jsonl artefact reveals each fired event's
`EventDefinition.true_cause` (the M6.1 schema field) on its
`event_fired` log line.

- CLI: `--debug` (boolean; no value follows the flag).
- `RunnerOptions::debug_mode` (default `false`).
- When `true`: every `event_fired` JSONL line gains a
  `"true_cause": "<verbatim M6.1 string>"` metadata key, emitted
  in insertion order after the existing `event_id_code` /
  `actor_kind` / `actor_id_code` / `country_id_code` keys.
- When `false`: the `true_cause` key is filtered out of the
  events.jsonl artefact. The truth is still stored in
  `state.logs` and therefore in the save.json `logs` array —
  only the artefact differs.

## 2. Why this shape

RFC-050 §5 names three layers per event: the truth, the
government report, and the media narrative. RFC-060 §3 spells
out the binary `publicText` / `debugTruth` pair on
`EventLogEntry`. M6.8's strict scope is the `debugTruth` reveal:
when debug mode is on, the player sees the truth (the M6.1
`true_cause` string) verbatim.

M6.9 — explicitly NOT in this PR — will add the `publicText`
side (`visible_report` from M6.2) with the M6.4 `reported_value`
and M6.5 `bias_noise` distortion applied in non-debug mode.
That work is what makes the non-debug player-facing surface
"lie" in a research-grounded way (RFC-080 §8
`ReportedValue = TrueValue + Bias + Noise`).

## 3. Why the truth is recorded UNCONDITIONALLY

`event_firer::record_match` and `event_firer::record_followup`
attach the `true_cause` metadata key to every `event_fired`
LogEntry at fire time, regardless of `debug_mode`. The
events.jsonl writer (`logging::write_jsonl_line`) filters the
key out unless its `debug_mode` parameter is `true`.

This split keeps three invariants clean:

1. **`state.logs` is the audit trail.** The truth survives in
   memory for any in-process inspection (tests, replay tools,
   future M6.9 diff helpers) regardless of the CLI flag.
2. **`save.json` is debug-flag-agnostic.** Save serialises
   `state.logs` directly, so two same-seed runs produce
   byte-identical `save.json` whether `--debug` is passed or
   not. The intervention tooling, replay tests, and canonical
   determinism baselines all depend on this.
3. **The artefact is the only thing that branches.** The single
   surface that differs between debug-on and debug-off is the
   events.jsonl content (specifically, the presence of the
   `true_cause` metadata key on `event_fired` lines).

The filter in `logging::write_jsonl_line` is keyed on the
metadata key name (`"true_cause"`), not on the entry category.
This is on purpose: if some future caller emits a `true_cause`
key on a non-`event_fired` LogEntry by accident, the filter
still hides it. A unit test pins this invariant
(`logging_system_test.cpp` "M6.8 export_jsonl: non-event-fired
entries with a `true_cause` key are unaffected").

## 4. What M6.8 deliberately does NOT do

- No new player-facing command. `--debug` is a CLI flag, not a
  `PlayerCommandKind`.
- No new save-layer surface. `state.logs` already serialised
  `metadata` as a flexible key/value list; M6.8 just adds one
  key on `event_fired` entries.
- No save schema bump (still `v18`).
- No artefact contract change (still 11 artefacts).
- No new gameplay system module.
- No state-mutation hook. Debug mode does not modify any
  country / faction / interest-group field, does not change
  which events fire (event_engine and event_evaluator are
  untouched), and does not consume `state.rng`.
- **No M6.9 work.** This PR adds only the `debugTruth` reveal
  on the debug side. The non-debug `publicText` /
  `reported_value` / `bias_noise` flow is RFC-090 §6.9's scope,
  not green-lit by this PR.

## 5. Plumbing

| Surface | Pre-M6.8 | Post-M6.8 |
|---|---|---|
| `RunnerOptions::debug_mode` | — | new field, default `false` |
| Runner `parse_args` | — | recognises `--debug` (no value) |
| Runner help text | — | documents `--debug` |
| `event_firer::record_match` | metadata: 4 keys | metadata: 5 keys (adds `true_cause`) |
| `event_firer::record_followup` | metadata: 5 keys | metadata: 6 keys (adds `true_cause`) |
| `logging::write_jsonl_line(out, entry)` | unconditional | gains optional `bool debug_mode = false` |
| `logging::export_jsonl(out, state)` | unconditional | gains optional `bool debug_mode = false` |
| Runner `end_tick` events.jsonl write | `export_jsonl(out, state)` | `export_jsonl(out, state, opts.debug_mode)` |

The `bool debug_mode` defaults on `write_jsonl_line` and
`export_jsonl` are `false`, so existing test sites and
diagnostic callers that don't care about debug mode continue to
compile and produce the non-debug artefact shape without source
change. New call sites that DO care pass `true` explicitly.

## 6. Determinism / byte-identical guarantees

- **Canonical `1930_minimal` 365-day**: no events fire on the
  canonical scenario (M5 invariant), so no `event_fired`
  LogEntries land in `state.logs`, so neither `save.json` nor
  `events.jsonl` contains any `true_cause` key, so the
  artefacts are byte-identical with and without `--debug`. The
  PR #115 canonical baseline is preserved.
- **Compliance `1930_rfc_compliance` 25 567-day**: events fire,
  so `state.logs` (and the `save.json` `logs` array) gain
  `true_cause` metadata on every `event_fired` entry whether
  `--debug` was passed or not. The `save.json` is byte-
  identical across the debug toggle. The `events.jsonl`
  artefact differs only by the presence of those `true_cause`
  keys.
- **`state.rng.counter`**: identical across debug-on and
  debug-off for the same seed. Pinned by a new integration
  test (`m5_event_pipeline_test.cpp` "M6.8 integration:
  --debug does NOT advance state.rng.counter").

## 7. Tests

- `event_firer_test.cpp` — three new cases:
  - `record_match` appends `true_cause` metadata sourced from
    `state.events[match.event_index].true_cause`.
  - `record_followup` appends `true_cause` metadata sourced
    from the followup `EventDefinition`.
  - The `true_cause` key is appended LAST so existing metadata
    insertion order is preserved.
- `logging_system_test.cpp` — five new cases:
  - default (`debug_mode = false`) filters `true_cause`.
  - `debug_mode = true` reveals `true_cause` verbatim.
  - filter is local; `entry.metadata` is not mutated.
  - `export_jsonl` debug vs non-debug differ only on
    `true_cause`; lifecycle / time entries are byte-identical
    across the toggle.
  - filter is scoped on the metadata key name, not on the
    entry category (defensive against future misuse).
- `runner_test.cpp` — two new cases:
  - `parse_args`: `--debug` unset → `debug_mode = false`.
  - `parse_args`: `--debug` present (no value) → `debug_mode = true`.
- `m5_event_pipeline_test.cpp` — two new integration cases:
  - End-to-end run with a firing event:
    `--debug` reveals `true_cause` in events.jsonl;
    no `--debug` filters it;
    `save.json` is byte-identical across the toggle.
  - End-to-end run with a firing event:
    `state.rng.counter` and `event_history` are byte-identical
    across the toggle.

## 8. Forward work

- **M6.9** — non-debug player-facing surface: emit
  `visible_report` (M6.2) on every `event_fired` line, with
  the M6.4 `reported_value::from_true_value` and M6.5
  `bias_noise::sample_for_event` distortion applied. M6.9 is
  the first downstream consumer of the `information_accuracy`
  helper family (M6.3 / M6.6 / M6.7) and the first place
  where the strict-no-silent-degradation discipline (PR #115)
  surfaces in a player-visible artefact.
- **Future**: a separate "debug overlay" SVG / HTML view
  alongside `map.html` could surface fired-event truth on the
  map. That's out of M6 scope — it belongs to a UI milestone.
