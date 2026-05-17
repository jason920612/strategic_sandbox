# M4.5 - SVG HTML viewer skeleton

Companion notes for `feature/rfc090-m4-05-html-viewer-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.4
left off. M4.4 added one `<text>` label per `<circle>`.
M4.5 wraps the existing SVG body in a minimal HTML5
document so the map opens cleanly in a browser without the
raw-XML chrome standalone `.svg` files attract. The reviewer
recommended this path over a "legend / metadata skeleton"
alternative on PR #70 approval.

The branch keeps the explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp     +render_map_html
                                             +write_map_html
                                             updated header doc
src/leviathan/systems/svg_export.cpp         +render_svg_root helper
                                             (refactor; byte-stable)
                                             +render_map_html / write
                                             +write_string_to helper
include/leviathan/systems/runner.hpp         +map_html_path option
                                             +map_html_path outcome
                                             updated artefact-list doc
src/leviathan/systems/runner.cpp             end_tick writes map.html
                                             as the 10th artefact
tests/systems/svg_export_test.cpp            7 new doctest cases
tests/systems/runner_test.cpp                5 new doctest cases
tests/integration/m{1,2,3}_end_to_end_test   byte-identical 9 → 10
docs/m4-5-html-viewer-skeleton.md            this file
README.md / docs/README.md / rfc/README.md   M4.5 ledger entries
                                             792 → 804 doctest cases
```

`provinces.svg` byte output is **unchanged** by M4.5 — the
refactor extracted a shared `render_svg_root` helper and
`render_provinces` continues to emit exactly the same bytes
it did at M4.4 (verified by the existing M4.x tests staying
green without modification, and by M1.17 / M2.22 / M3.7
byte-identical determinism contracts continuing to pass).

## 2. Output shape

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Leviathan Map</title>
</head>
<body>
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 1000" width="1000" height="1000">
  <circle cx="520.00" cy="440.00" r="8" fill="#4682b4" data-id="berlin" data-owner="0"/>
  <text x="520.00" y="462.00" text-anchor="middle">Berlin</text>
  ...
</svg>
</body>
</html>
```

Design choices:

- **Inline SVG, not external reference.** The HTML embeds
  the SVG body directly in `<body>` rather than loading
  `provinces.svg` via `<img>`, `<object>`, or `<iframe>`.
  Self-contained file means no path / CORS pitfalls when
  opened via `file://`, and the file can be emailed /
  shared standalone. Cost: the SVG body appears twice on
  disk (once in `provinces.svg`, once inlined in
  `map.html`) — small price for the simplicity.
- **No XML prolog inside the HTML.** The
  `<?xml version="1.0" encoding="UTF-8"?>` line is invalid
  inside an HTML5 document. M4.5 refactored the renderer to
  expose a `render_svg_root(state)` helper (anonymous
  namespace) that produces only the `<svg>...</svg>` body;
  `render_provinces` prepends the XML prolog,
  `render_map_html` does not.
- **Minimum HTML5 boilerplate.** `<!DOCTYPE html>`,
  `<html lang="en">`, `<head>` with `<meta charset="UTF-8">`
  + `<title>`, `<body>` with the inline SVG. No `<style>`,
  no `<script>`, no `<link>`, no `<meta name="viewport">`,
  no inline event handlers. The reviewer's spec said
  "minimum"; M4.5 honours it.
- **`lang="en"` attribute.** Tiny accessibility hint; safe
  default. Future i18n work can swap this.
- **LF terminators throughout** (matches the M4.2 SVG file
  convention).
- **Empty `state.provinces` still emits a valid HTML doc.**
  The embedded `<svg>` is header-only (no `<circle>` /
  `<text>` children), but the HTML document is fully
  formed. Mirrors M4.2's empty-state contract.

## 3. Public API change

`svg_export.hpp` adds two new public functions:

```cpp
std::string render_map_html(const core::GameState& state);
core::Result<bool> write_map_html(const core::GameState& state,
                                  const std::filesystem::path& path);
```

The M4.2 `render_provinces(state)` and
`write_provinces(state, path)` signatures are unchanged.
Both new functions are pure render / render+write parallels
of the existing pair, so the call-site ergonomics are
identical.

## 4. Runner integration

`end_tick` writes `map.html` as the **tenth unconditional
artefact**, immediately after `provinces.svg`. The write
order is:

```text
save.json
events.jsonl
summary.csv                             (opt-in)
countries.csv                           (opt-in)
factions.csv                            (opt-in)
interest_groups.csv                     (unconditional)
interest_group_country_feedback.csv     (unconditional)
interest_group_authority_pressure.csv   (unconditional)
provinces.svg                           (unconditional)
map.html                                (unconditional, NEW)
```

`RunnerOptions::map_html_path` is an `optional<path>` that
defaults to `<output_dir>/map.html` when unset. No CLI flag
is wired through `parse_args` — mirrors the M3.5 / M3.6 /
M4.2 unconditional-artefact pattern. The resolved path lands
in `RunOutcome::map_html_path`.

The M2.9 pre-`end_tick` no-artefact contract extends
automatically to `map.html` (the new write happens inside
`end_tick`; nothing before `end_tick` touches disk for it).

The M3.6 mid-`end_tick` non-transactional caveat extends
similarly: `end_tick` writes its ten artefacts sequentially;
a mid-write I/O failure can leave a partial set on disk.
Switching to atomic temp-file + rename remains a deferred
item.

## 5. Effect on the nine-artefact invariant

M4.2's PR satisfied the M3-exit-report §5 "adding a 9th
requires its own sub-milestone with the contracts
documented" rule. M4.5 follows the same template for the
10th: this sub-milestone documents the cadence
(unconditional, one write per `end_tick`), determinism
(same state → byte-stable output), and pre-`end_tick`
contract (no artefact on pre-`end_tick` failure) for
`map.html`. The test suite pins all three.

## 6. Save format stays v12

M4.5 reads `state.provinces` but does not store, derive, or
persist any new field. Save format remains v12.

## 7. Tests added

`tests/systems/svg_export_test.cpp` (7 new cases):

1. `render_map_html: empty state still produces a valid HTML document`
2. `render_map_html: does NOT emit an XML prolog (invalid inside HTML)`
3. `render_map_html: inlines the same <svg> body as render_provinces`
   — pins the byte-equivalence between the standalone-SVG body
   and the inlined-in-HTML body.
4. `render_map_html: no <style>, <script>, <link>, or inline event attributes`
   — pins the "minimum wrapper" scope.
5. `render_map_html: deterministic across repeat calls`
6. `write_map_html: writes file matching render_map_html byte-for-byte`
7. `write_map_html: creates parent directories`

`tests/systems/runner_test.cpp` (5 new cases):

8. `run: map.html is written by end_tick (unconditional, empty-state still a valid HTML doc)`
9. `run: map.html defaults to <output_dir>/map.html`
10. `run: map_html_path override is honoured`
11. `run: canonical scenario embeds full SVG body inside map.html`
    — asserts canonical labels (Berlin/Paris/Tokyo) and the
    GER palette colour appear inside `map.html`, and that no
    XML prolog leaked in.
12. `run: map.html preserves byte-identical determinism on same seed`

`tests/integration/m{1,2,3}_end_to_end_test.cpp`:

- Byte-identical determinism comparisons extended from 9 to
  10 artefacts (each adds one `read_file(... / "map.html")`
  check). The M3 integration test's case names and file-
  header comment updated from "9 artefacts" → "10 artefacts".

Total: 12 new doctest cases (792 → 804).

## 8. What M4.5 explicitly does NOT do

```text
no click handlers
no clickable UI / event handlers / hover state
no tooltips
no state mutation from the viewer (map.html is read-only)
no legend / colour key
no CSS / <style> / inline style attributes (other than what
   M4.3 puts on circles via `fill`)
no JavaScript / <script>
no <link> / external resource references
no <meta name="viewport"> (defer to a future presentation
   sub-milestone if responsive sizing becomes a need)
no font-family / font-size on <text> (M4.4 contract carried
   forward unchanged)
no ownership-dynamics layer
no neighbour / adjacency edges
no terrain / resources / population overlays
no events / AI / command integration
no new PlayerCommandKind
no runner CLI flag
no save schema bump (still v12)
no new state field
no new gameplay
no atomic end_tick writes
```

The next M4 sub-milestone is unspec'd and waits for the
reviewer.

## 9. Cross-references

- [`m4-4-svg-labels-skeleton.md`](m4-4-svg-labels-skeleton.md)
  — the M4.4 labels M4.5's HTML wrapper inlines verbatim.
- [`m4-3-svg-owner-color-skeleton.md`](m4-3-svg-owner-color-skeleton.md)
  — the M4.3 palette M4.5's HTML wrapper renders alongside.
- [`m4-2-svg-exporter-skeleton.md`](m4-2-svg-exporter-skeleton.md)
  — the M4.2 renderer M4.5 extends via a new HTML wrapper
  (the M4.2 standalone-SVG output is byte-unchanged by M4.5).
- [`m4-1-svg-map-data-skeleton.md`](m4-1-svg-map-data-skeleton.md)
  — the M4.1 data layer all four M4.x renderers read.
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants future milestones must preserve (M4.5 is
  consistent: no schema change, no command-gate change,
  no logs / events from interest groups; the 9 → 10 artefact
  growth is the second instance of the documented "growing
  the set needs its own sub-milestone" rule).
