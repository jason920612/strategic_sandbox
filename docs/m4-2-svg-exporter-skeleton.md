# M4.2 - SVG exporter skeleton

Companion notes for `feature/rfc090-m4-02-svg-exporter-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.1
(`docs/m4-1-svg-map-data-skeleton.md`) left off. M4.1 shipped
the typed `core::ProvinceNode` data layer; M4.2 ships the
**first renderer that turns that data into pixels** — a
minimal deterministic SVG artefact written unconditionally by
`end_tick`. Later M4 sub-milestones can extend it (HTML
viewer, clickable map, colour-by-owner, neighbour-adjacency
lines, terrain, etc.); M4.2 only ships the data → SVG
transform.

The branch name carries an explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (the redo lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp           new module
src/leviathan/systems/svg_export.cpp               new module
runner end_tick                                    writes provinces.svg
RunnerOptions::provinces_svg_path                  optional override
RunOutcome::provinces_svg_path                     resolved path
tests/systems/svg_export_test.cpp                  8 new doctest cases
tests/systems/runner_test.cpp                      5 new doctest cases
tests/integration/m{1,2,3}_end_to_end_test.cpp     extended to 9 artefacts
artefact set                                       8 -> 9 files
764 -> 776 doctest cases
```

Two free functions on `leviathan::systems::svg_export`:

```cpp
std::string render_provinces(const core::GameState& state);
core::Result<bool> write_provinces(const core::GameState& state,
                                   const std::filesystem::path& path);
```

`render_provinces` is the pure transform (useful for tests);
`write_provinces` is render + file write (used by `end_tick`).

## 2. Output shape

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000" width="1000" height="1000">
  <circle cx="520.00" cy="440.00" r="8" fill="black" data-id="berlin" data-owner="0"/>
  <circle cx="470.00" cy="480.00" r="8" fill="black" data-id="paris" data-owner="1"/>
  <circle cx="830.00" cy="550.00" r="8" fill="black" data-id="tokyo" data-owner="2"/>
</svg>
```

Design choices:

- **`viewBox="0 0 1000 1000"`.** Multiplies the M4.1
  normalised `[0, 1]` coords by `kSvgCoordScale = 1000`.
  Defers the projection / pixel-size / canvas decision to
  the renderer-side: any viewer can wrap the SVG in its own
  container without coordinate gymnastics.
- **`<circle>` per node, `r="8"`.** Smallest renderable
  marker that survives a quick zoom-out. No per-country
  variation in size.
- **`fill="black"`.** Explicit single colour. The spec is
  explicit: no map colours, no ownership-driven palette. A
  future M4.x sub-milestone introduces colour with its own
  contract.
- **`data-id` + `data-owner` attributes.** Stable identity
  surface for future renderers, tests, and tooling without
  parsing presentation values. Mirrors the
  `interest_groups.csv` row pattern: identity columns first,
  presentation values second.
- **Insertion order preserved.** No sort. Matches M3.5's
  `interest_groups.csv` rule so the order is predictable
  across the artefact set.
- **Coords formatted via `std::fixed` + `setprecision(2)`.**
  Byte-stable across platforms (locale-independent fixed
  notation, never scientific, never decimal-comma).
- **LF line terminators, two-space indent, trailing newline.**
  Same conventions as the CSV writers.
- **Empty `state.provinces` produces a header-only SVG.**
  The `<svg>` element is still written; no `<circle>`
  children. The artefact-on-disk contract stays consistent
  (always-present file), mirroring M3.5 `interest_groups.csv`.

## 3. Runner integration

`end_tick` writes `provinces.svg` as the **ninth
unconditional artefact**, immediately after the M3.6
authority-pressure trace CSV. The write order is:

```text
save.json
events.jsonl
summary.csv                             (opt-in)
countries.csv                           (opt-in)
factions.csv                            (opt-in)
interest_groups.csv                     (unconditional)
interest_group_country_feedback.csv     (unconditional)
interest_group_authority_pressure.csv   (unconditional)
provinces.svg                           (unconditional, NEW)
```

`RunnerOptions::provinces_svg_path` is an `optional<path>`
that defaults to `<output_dir>/provinces.svg` when unset. No
CLI flag is wired through `parse_args` — mirrors the M3.5 /
M3.6 unconditional-artefact pattern. The resolved path lands
in `RunOutcome::provinces_svg_path`.

The M2.9 pre-`end_tick` no-artefact contract extends
automatically to `provinces.svg` because `end_tick` is still
the only function on the runner side that touches disk for
output artefacts.

The mid-`end_tick` non-transactional caveat from M3.6
extends similarly: `end_tick` writes its nine artefacts
sequentially; a mid-write I/O failure can leave a partial
set on disk. Switching to atomic temp-file + rename remains
a deferred item.

## 4. Effect on the eight-artefact invariant

M3's exit report (`docs/milestone-3-result.md` §5) said:

> The artefact set is eight files. Adding a ninth requires
> its own sub-milestone with the cadence / determinism /
> pre-end_tick contracts documented alongside.

M4.2 is that sub-milestone. The cadence (unconditional, one
write per `end_tick`), determinism (same state → byte-stable
output), and pre-`end_tick` contract (no artefact written on
pre-`end_tick` failure) are documented above and pinned by
the test suite.

## 5. Tests added

- **`tests/systems/svg_export_test.cpp` (8 cases)**: empty
  state produces header-only SVG; one node produces one
  `<circle>` with stable coords; insertion order preserved
  (no sort); two render calls byte-identical; write +
  re-read matches the in-memory render byte-for-byte; parent
  directory created automatically; empty-state write still
  valid; coordinate formatting stable across platforms (via
  the same render-determinism case).
- **`tests/systems/runner_test.cpp` (5 cases)**: empty-state
  unconditional write; default path resolution; programmatic
  path override; canonical scenario renders all three
  M4.1 nodes with correct `data-id` / `data-owner`
  attributes; same-seed byte-identical determinism.
- **`tests/integration/m{1,2,3}_end_to_end_test.cpp`**:
  byte-identical comparisons extended from 8 to 9 artefacts.
  The M3 integration test header comment + the case names
  updated from "8 artefacts" → "9 artefacts".

## 6. What M4.2 explicitly does NOT do

```text
no HTML viewer
no clickable UI
no event handlers
no hover state / tooltips
no map colours (every node renders with fill="black")
no per-country palette
no ownership-driven colour mapping
no ownership dynamics (provinces are static reads)
no neighbour / adjacency edges
no controller-vs-owner split
no terrain / resources / population overlays
no labels / text elements
no events / AI / command integration
no new PlayerCommandKind
no runner CLI flag (--svg or similar)
no save schema bump (still v12)
no new state field
no new gameplay
no atomic end_tick writes
```

The next M4 sub-milestone is unspec'd and waits for the
reviewer.

## 7. Cross-references

- [`m4-1-svg-map-data-skeleton.md`](m4-1-svg-map-data-skeleton.md)
  — the M4.1 data layer M4.2 renders.
- [`m3-6-interest-group-feedback-trace-csv.md`](m3-6-interest-group-feedback-trace-csv.md)
  — the M3.6 unconditional-artefact pattern M4.2 mirrors.
- [`m3-5-interest-group-csv-surface.md`](m3-5-interest-group-csv-surface.md)
  — the M3.5 unconditional-artefact pattern M4.2 mirrors.
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  eight-artefact invariant M4.2 grows to nine.
