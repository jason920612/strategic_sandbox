# M7.4 — 加入派系衝突 (faction conflict)

- Status: shipped (PR pending)
- RFC anchors: **RFC-090 §7.4**, **RFC-020 §8**, RFC-080 §1 / §11
- Save format: **unchanged** (no persistent-field addition)
- Artefact contract: 11 (unchanged)
- Branch: `feature/m7-04-faction-conflict`
- Depends on: nothing (branches from main directly; independent
  of PR #119 / #120 / #121)

## 1. Scope

RFC-090 §7.4 reads `加入派系衝突` (add faction conflict).
RFC-020 §8 `派系鬥爭` enumerates five concrete rivalry pairs:

| RFC-020 §8                | Type pair                              |
| ------------------------- | -------------------------------------- |
| 軍方 vs 情報部門          | military ↔ intelligence                |
| 工會 vs 技術菁英          | workers ↔ technical_elites             |
| 中央官僚 vs 地方勢力      | bureaucracy ↔ local_elites             |
| 學生 vs 宗教勢力          | students ↔ religious                   |
| 媒體 vs 情報部門          | media ↔ intelligence                   |

Strictly the five RFC-020 §8 pairs; intelligence appears in
TWO pairs (against military, against media) — preserved
verbatim from the RFC enumeration.

What this PR ships:

- New module `leviathan::systems::faction_conflict` with
  one helper:
  `tick_apply_pressure(state) -> Result<PressureOutcome>`.
  For each country and each §8 rivalry pair, if BOTH sides
  of the pair have at least one faction in that country
  with influence strictly above
  `kFactionConflictInfluenceThreshold = 0.30`, apply
  asymptotic-add radicalism+
  `kFactionConflictAsymptoticRadicalismDelta = 0.01` on
  every participating faction.
- Wired into `monthly::tick_all_countries` as a new step 8
  (between ai_policy::apply and event_engine::tick_events).
- Two new counters on `MonthlyOutcome`:
  `faction_conflict_pairs_active` /
  `faction_conflict_factions_drifted`.

## 2. Strict RFC reading

- Exactly the five §8 pairs. No additional rivalries (e.g.
  no "religious vs technocrats", no "media vs military")
  — those are not in §8 and stay out of scope until a
  future sub-milestone authorises them.
- Pair list is HARD-CODED in the helper. The RFC names
  these five rivalries explicitly; making them
  data-driven is not authorised by §8 and would let an
  authoring oversight silently shift the simulation.
- The drift is on `radicalism` only — RFC-020 §8 is
  about INTER-faction RIVALRY (a grievance / instability
  dynamic). Loyalty and support drifts are not §8 mechanics.
- Symmetric mechanics: both sides drift radicalism+ when
  the pair is active; the RFC does not give one side
  asymmetric advantage.

## 3. Game-model coefficients (RFC-080 §1 / §11)

- **`kFactionConflictInfluenceThreshold = 0.30`** —
  influence floor below which the rivalry is dormant.
  Direction: RFC-020 §8 says the conflict is BETWEEN
  factions; a faction needs non-trivial political weight
  to participate. 0.30 is a game-model floor.
- **`kFactionConflictAsymptoticRadicalismDelta = 0.01`** —
  per monthly tick drift. Direction: Collier-Hoeffler
  grievance-opportunity model + Alesina-Perotti
  instability-via-discontent loop establish that elite
  conflict raises grievance. Per-tick magnitude is a
  pure game-model assumption; the asymptotic-add shape
  matches the post-PR #115 hardening convention so
  radicalism stays in `[0, 1]` by construction.

Neither coefficient is paper-derived. Both disclosed as
game-model assumptions per RFC-080 §1 / §11 in the helper
header.

## 4. Tests + verification

- `cmake --build build --config Debug` succeeds.
- `build/bin/Debug/leviathan_tests.exe` reports
  **1315 cases / 96 270 assertions / 0 failed** (verified
  via direct binary run per
  `feedback_ctest_masks_doctest`).
- Delta from main (1301 / 96 228): +14 unit tests in
  `tests/systems/faction_conflict_test.cpp` covering:
  each of the 5 RFC-020 §8 pairs activating, out-of-
  allowlist types not generating drift, threshold-gating
  (strictly > threshold), per-country scoping (X's
  military vs Y's intelligence does NOT activate),
  intelligence in TWO pairs compounding correctly,
  strict NaN-input rejection, asymptotic-add bound
  preservation near 1.0, determinism, empty-state.
- Canonical `1930_minimal` 365-day sweep:
  `Sanity issues : 0`. Canonical GER factions:
  bureaucracy (influence 0.60), workers (?), military
  (influence 0.70). bureaucracy has no `local_elites`
  rival in canonical; military has no `intelligence`
  rival; workers has no `technical_elites` rival. So
  no §8 pair activates on canonical — faction radicalism
  drifts are byte-identical with pre-M7.4 main.
- Compliance `1930_rfc_compliance` 25 567-day sweep:
  `Sanity issues : 0`. Compliance loads the same three
  GER factions as canonical, so no §8 pair activates on
  the 70-year sweep either.

## 5. What this PR deliberately DOES NOT do

- No persistent `FactionConflict` struct on GameState.
  The pair list lives in code (RFC-020 §8 is constant).
  A future sub-milestone may add per-pair intensity /
  history records if RFC anchors authorise it.
- No save schema version bump.
- No `state.rng` consumption.
- No new player-facing command.
- No new CLI flag.
- No new artefact.
- No loyalty / support drift — strictly radicalism per
  the §8 reading.
- No claim that M6 is closed.
- No M7.5+ work started.
