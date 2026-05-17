# M3.9 - InterestGroup-derived military_loyalty pressure (sibling of M3.4)

Companion notes for `feature/m3-09-military-pressure-channel`.

M3.9 adds the **second** authority-layer pressure channel.
After M3.4 wired Bureaucracy-kind loyalty into
`bureaucratic_compliance`, M3.9 wires Military-kind loyalty
into `military_loyalty` at the same rate. M3.4 and M3.9 are
**siblings**, not nested: both run at the "authority" layer
of the rate ladder, both run after the M3.3 country-feedback
step, both leave the other three `government_authority`
sub-fields untouched.

No new artefact. No save schema bump. No formula change to
M3.2 / M3.3 / M3.4. No new gameplay.

## 1. Scope

After M3.4 the rate ladder was:

```text
group mood              (M3.2)  0.05
country stability       (M3.3)  0.02
bureaucratic_compliance (M3.4)  0.01
```

M3.9 adds:

```text
military_loyalty        (M3.9)  0.01
```

M3.4 and M3.9 share the 0.01 rate because they sit at the same
authority layer. They are siblings, not nested. The other two
`government_authority` sub-fields — `intelligence_capability`
and `media_control` — stay inert until their own sibling
channels land in future PRs (M3.X / M3.X+1 candidates).

### Why this exact shape

- **Military-kind only.** "Military loyalty" reads as the
  armed forces' loyalty to the regime; Military-kind interest
  groups (general staff, officer corps, etc.) are the
  natural input. Other kinds (Bureaucracy, Workers, Religious)
  have their own channels (existing or future).
- **Loyalty, not radicalism.** Symmetric with M3.4's
  Bureaucracy-loyalty → bureaucratic_compliance choice.
  Radicalism already drives stability through M3.3;
  reusing it on the authority layer would double-count one
  negative signal.
- **Rate 0.01.** Matches M3.4 so neither channel dominates
  the authority drift. A future balance pass can split them
  if the data shows asymmetric volatility.
- **No new CSV artefact.** M3.4 originally shipped without a
  trace CSV (M3.6 retrofitted them for the M3.3 / M3.4 pair).
  M3.9 follows that same shape — system first, CSV later.
  The trace vector still surfaces through
  `MonthlyOutcome::interest_group_military_pressure_trace_rows`
  so tests and a future M3.X runner extension can consume it.

## 2. Public API

`include/leviathan/systems/interest_group_system.hpp`:

```cpp
namespace leviathan::systems::interest_group {

inline constexpr double kInterestGroupMilitaryPressureRate = 0.01;

struct MilitaryPressureOutcome {
    int countries_updated = 0;
};

struct MilitaryPressureTraceRow {
    core::GameDate date;
    int            country_id      = -1;
    std::string    country_id_code;
    int            matched_groups  = 0;
    double         weight_sum                 = 0.0;
    double         weighted_military_loyalty  = 0.0;
    double         target_military_loyalty    = 0.0;
    double         military_loyalty_before    = 0.0;
    double         military_loyalty_after     = 0.0;
    double         military_loyalty_delta     = 0.0;
};

core::Result<MilitaryPressureOutcome> military_pressure(
    core::GameState& state,
    std::vector<MilitaryPressureTraceRow>* trace_out = nullptr);

}  // namespace
```

### Formula

For each country index `ci`:

```cpp
double weighted_sum = 0.0;
double weight_sum   = 0.0;
for (const auto& g : state.interest_groups) {
    if (g.country.value() != ci)                                continue;
    if (g.kind != core::InterestGroupKind::Military)            continue;
    if (g.influence <= 0.0)                                     continue;
    weighted_sum += g.influence * g.loyalty;
    weight_sum   += g.influence;
}
if (weight_sum <= 0.0)                                          continue;

const double target = weighted_sum / weight_sum;
auto& ml = state.countries[ci].government_authority.military_loyalty;
ml += (target - ml) * kInterestGroupMilitaryPressureRate;
ml  = std::clamp(ml, 0.0, 1.0);
++outcome.countries_updated;
```

Skipped countries (no matching Military groups, or all matches
have zero influence) emit no trace row.

### Preflight

Strict, mirroring M3.4. Validates every input the step actually
reads:

- every `group.country` indexes into `state.countries`;
- every `group.influence` is finite + `[0, 1]`;
- every `group.loyalty` is finite + `[0, 1]`;
- every country's `government_authority.military_loyalty` is
  finite + `[0, 1]`.

A single bad entry rejects the whole call with no mutation —
atomicity across the list.

`radicalism` and `country.stability` are NOT preflighted
because M3.9 does not read them.

## 3. Monthly pipeline position

`monthly::tick_all_countries` now runs FOUR global steps after
the per-country pipeline:

```text
for each country (vector order):
    faction::react / stability::tick / economy::tick    (M1.x)
interest_group::react                                    (M3.2, 0.05)
interest_group::country_feedback(&cf_trace)              (M3.3, 0.02)
interest_group::authority_pressure(&ap_trace)            (M3.4, 0.01)
interest_group::military_pressure(&mp_trace)             (M3.9, 0.01)  <- NEW
```

`military_pressure` runs after `authority_pressure` to make
the rate ladder visible in code order. The two could in
principle commute (they touch different output fields and
their input filters never overlap because `kind` is a single
value), but pinning the order explicitly is cheaper than
proving non-interference for every future reader.

### MonthlyOutcome additions

```cpp
int interest_group_military_countries_updated = 0;
std::vector<interest_group::MilitaryPressureTraceRow>
    interest_group_military_pressure_trace_rows;
```

Same shape as the M3.4 counter + the M3.6 trace vector pair.
The new fields are NOT persisted — `MonthlyOutcome` is a
runtime aggregate that lives one boundary at a time.

## 4. Runner / artefact contract

**Unchanged.** The artefact set stays at 8 files. M3.9 does
NOT add a per-system CSV. The trace vector surfaces through
`MonthlyOutcome`; a future M3.X PR can wire it to a new CSV
file (`interest_group_military_pressure.csv`) and grow the
contract to 9 files. M3.9 deliberately keeps that work
separate so this PR is small and its review surface stays
focused on the system itself.

This mirrors how M3.4 shipped (system only) before M3.6
landed the CSV pair for M3.3 + M3.4 together.

## 5. Save schema

**Unchanged: v11.** No persistent state field changes. The
existing `GovernmentAuthorityState::military_loyalty` field
(M2.16) is the mutation surface; it has been a save-schema
required field since v10.

## 6. Sibling-field invariant

M3.9 mutates ONLY
`government_authority.military_loyalty`. The other three
sub-fields:

- `bureaucratic_compliance` (M3.4 surface);
- `intelligence_capability` (M3.X candidate);
- `media_control` (M3.X candidate);

are byte-identical before and after the call. Pinned by
`military_pressure: M3.4 sibling fields stay byte-identical`
in the unit test suite.

## 7. Trace pointer semantics

Identical to M3.6's pattern for `country_feedback` /
`authority_pressure`:

- default-null = byte-identical with "trace was never
  requested";
- non-null = one row appended per actually-updated country;
- skipped country = no row;
- preflight failure = no partial rows (the trace_out vector
  the caller passed in stays untouched);
- the pointer never changes the formula, the mutation, or
  `countries_updated`.

The "null vs non-null is byte-identical" guarantee is pinned
by M3.8-style tests at the end of the M3.9 section in
`interest_group_system_test.cpp`.

## 8. Test surface

18 new doctest cases:

`interest_group_system_test.cpp` (M3.9 section):

- empty state, skipped (no Military group), skipped (wrong
  kind), single Military group formula, two-group
  influence-weighted formula, zero-influence skip;
- preflight failure for out-of-range country, NaN loyalty,
  out-of-range influence, NaN military_loyalty;
- preflight-failure atomicity across countries;
- clamp keeps military_loyalty inside `[0, 1]`;
- sibling fields (bureaucratic_compliance /
  intelligence_capability / media_control) stay
  byte-identical;
- trace pointer emits one row per updated country (with
  exact numerics);
- trace pointer adds no row for skipped country;
- trace pointer adds no partial rows on preflight failure;
- M3.8-style null-vs-non-null byte-identical state on single
  group;
- M3.8-style null-vs-non-null byte-identical state on
  skipped country.

Existing tests extended:

- `monthly_pipeline_test.cpp` — the trace-vector helper test
  already covers `interest_group_military_pressure_trace_rows`
  shape via the canonical pipeline call; no edits needed
  because the test reads the fields generically.
- `m3_end_to_end_test.cpp` — the helper state now adds one
  Military-kind group so test A asserts all FOUR M3 reaction
  systems mutate the expected fields and surface all THREE
  trace vectors. Test B's `interest_groups_csv_rows`
  expectation grew from 3 to 6 (one extra group × three
  snapshot points). Tests B + C otherwise unchanged.

Total: 766 doctest cases on this branch (+18 vs main's 748).

## 9. What is NOT in scope

- no save schema bump (still v11);
- no new artefact (still 8);
- no formula change to M3.2 / M3.3 / M3.4;
- no new `InterestGroupKind` variants;
- no events / state.logs from M3.9;
- no AI / UI / CLI / REPL;
- no new `PlayerCommandKind`;
- no command-gate integration (M3.9 does NOT feed the
  M2.18 / M2.19 gate; that gate still reads only
  `bureaucratic_compliance`);
- no per-kind balancing changes;
- no atomic `end_tick` writes;
- no `intelligence_capability` / `media_control` channel
  (each its own future PR);
- no per-system CSV file for M3.9 (M3.X candidate after
  this lands).

## 10. Future M3.10+ candidates (none committed)

- Per-system CSV for `military_pressure` — would add a 9th
  artefact (`interest_group_military_pressure.csv`) on the
  same shape as the M3.6 trace CSVs. Smallest possible
  follow-up.
- `intelligence_capability` sibling channel (Media-kind?
  Bureaucracy-kind subset? — depends on whether intelligence
  is its own kind or a derived view).
- `media_control` sibling channel (Media-kind loyalty →
  media_control).
- Interest-group integration into the M2.18 / M2.19
  command-execution gate as an additional input beyond the
  already-drifting `bureaucratic_compliance`.
- Faction-aware multi-kind aggregation (weight Military
  loyalty by faction support if the country has a Military
  faction with high radicalism).
- Influence drift driven by event / policy outcomes.

Per the M-pacing rule, the next sub-milestone starts only
when the reviewer's approval message names a direction or
defaults to the top of this list.
