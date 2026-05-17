# M4.7 - HTML legend skeleton

Companion notes for `feature/rfc090-m4-07-html-legend-skeleton`.

This is RFC-090 §M4 (SVG map + UI), continuing where M4.6
left off. M4.6 styled the M4.5 HTML wrapper with three CSS
rules. M4.7 adds a **static legend** to `map.html` so a
viewer can decode which palette colour belongs to which
country — the chart and the colours have been on the page
since M4.3, but until M4.7 there was no key.

The branch keeps the explicit `rfc090-` prefix to
disambiguate from the 2026-05-17 rolled-back invented-M4.X
governance work (lesson recorded in
`docs/milestone-3-result.md` §7).

## 1. Scope

What ships:

```text
include/leviathan/systems/svg_export.hpp     updated header doc
                                             (output-shape +
                                             scope-limit blocks)
src/leviathan/systems/svg_export.cpp         render_map_html emits
                                             <ul class="legend">
                                             after the inline SVG
                                             + 3 new CSS rules
tests/systems/svg_export_test.cpp            +1 helper
                                             8 new doctest cases
tests/systems/runner_test.cpp                1 new doctest case
docs/m4-7-html-legend-skeleton.md            this file
README.md / docs/README.md / rfc/README.md   M4.7 ledger entries
                                             810 → 819 doctest cases
```

`provinces.svg` byte output is **unchanged** by M4.7 — the
legend lives only in the HTML wrapper. The standalone-SVG
path stays CSS-free, legend-free for downstream consumers
(SVG-to-PNG pipelines, vector tools).

## 2. Output shape

After the inline SVG body, the HTML wrapper now emits:

```html
<ul class="legend">
  <li data-owner="0"><svg class="swatch" viewBox="0 0 16 16" width="16" height="16"><circle cx="8" cy="8" r="8" fill="#4682b4"/></svg>GER &mdash; Germany</li>
  <li data-owner="1"><svg class="swatch" viewBox="0 0 16 16" width="16" height="16"><circle cx="8" cy="8" r="8" fill="#cd5c5c"/></svg>FRA &mdash; France</li>
  <li data-owner="2"><svg class="swatch" viewBox="0 0 16 16" width="16" height="16"><circle cx="8" cy="8" r="8" fill="#daa520"/></svg>JPN &mdash; Japan</li>
</ul>
```

Three new CSS rules join the existing M4.6 block:

```css
.legend {
  list-style: none;
  padding: 0;
  margin: 20px auto;
  max-width: 1000px;
  font-family: sans-serif;
}
.legend li {
  display: flex;
  align-items: center;
  margin: 4px 0;
}
.legend .swatch {
  width: 16px;
  height: 16px;
  margin-right: 8px;
  flex-shrink: 0;
}
```

Design choices:

- **One `<li>` per `state.countries[i]`, in vector order.**
  Same insertion-order rule as `interest_groups.csv`,
  `<circle>`, and `<text>` — no sort.
- **`data-owner="<i>"` on each `<li>`.** Stable identity
  attribute so future renderers / tests can locate a row
  without parsing presentation values. Mirrors the
  `data-owner` attribute on each `<circle>` in the SVG body.
- **Per-`<li>` 16x16 inline SVG swatch.** A tiny
  `<svg viewBox="0 0 16 16" width="16" height="16">` with a
  single `<circle>` filled by `color_for_owner(CountryId{i})`.
  Using inline SVG (not an HTML `<span>` with `background-color`)
  means **no inline `style="..."` attribute is needed** —
  preserving the M4.6 constraint. The `fill` attribute on
  the `<circle>` is an SVG presentation attribute, not an
  HTML inline style.
- **Text content: `"<id_code> &mdash; <name>"`.** Both
  pieces XML-text-escaped via the M4.4 `xml_text_escape`
  helper. The em-dash entity (`&mdash;`) is a readable
  separator and a familiar HTML idiom.
- **Empty `state.countries` produces an empty `<ul>`.**
  Always-present-file contract preserved (mirrors the
  empty-state header-only `<svg>` behaviour from M4.2).

The three new CSS rules:

- **`.legend`** — strip the default list bullets / padding,
  centre the list horizontally at the same `max-width: 1000px`
  as the SVG above it (so the legend feels aligned with the
  map card), and explicitly pick sans-serif so the row text
  matches the SVG-label font M4.6 chose.
- **`.legend li`** — `display: flex` + `align-items: center`
  vertically centres the swatch beside the text; `margin: 4px 0`
  gives a small breath between rows.
- **`.legend .swatch`** — fixed `16x16` so the swatch SVG
  doesn't stretch under flex layout; `margin-right: 8px`
  separates swatch from text; `flex-shrink: 0` keeps the
  swatch from collapsing if the text is long.

## 3. What changed in the public API

Nothing. `render_map_html(state)` and
`write_map_html(state, path)` keep the same signatures.
Callers (just `runner::end_tick` today) need zero source
change.

## 4. Runner integration

None new. The M4.5 wiring (unconditional write of
`map.html` as the 10th artefact, optional
`RunnerOptions::map_html_path` override defaulting to
`<output_dir>/map.html`, resolved path on
`RunOutcome::map_html_path`) carries forward unchanged.

The M2.9 pre-`end_tick` no-artefact contract and the M3.6
mid-`end_tick` non-transactional caveat both continue to
hold identically.

## 5. Artefact set + save format unchanged

- **Artefact set still 10.** `provinces.svg` and `map.html`
  are the only renderer outputs; M4.7 modifies only the
  bytes of `map.html`.
- **Save format still v12.** M4.7 reads `state.countries`
  (which has been v8+ persistent state for years) but does
  not add, derive, or persist any new field.
- M1.17 / M2.22 / M3.7 byte-identical determinism contracts
  continue to pass by construction — both same-seed runs
  produce the same legend bytes.

## 6. Tests added

`tests/systems/svg_export_test.cpp` (1 helper + 8 new cases):

- **Helper**: `country(int idx, id_code, name)` factory
  alongside the existing `node(...)` factory.
- (1) `render_map_html: emits a <ul class="legend"> after
  the inline SVG` — pins position (after `</svg>`, before
  `</body>`).
- (2) `render_map_html: empty state.countries → empty
  <ul class="legend">` — pins the always-present-file
  contract.
- (3) `render_map_html: one country → one <li> with
  id_code + name + swatch` — pins the per-row content and
  the swatch class / viewBox / fill.
- (4) `render_map_html: three countries → three <li>s in
  vector order` — pins vector-order preservation and that
  the per-owner palette swatch matches each row's owner.
- (5) `render_map_html: legend text content is XML-text-escaped`
  — pins escape on `id_code` + `name` with metacharacters.
- (6) `render_map_html: <style> block carries the M4.7
  legend CSS rules` — pins the three new selectors.
- (7) `render_map_html: legend is deterministic across
  repeat calls`
- (8) `render_provinces (standalone SVG) does NOT include
  the M4.7 legend` — pins legend isolation from the
  standalone-SVG path.

`tests/systems/runner_test.cpp` (1 new case):

- (9) `run: canonical scenario emits M4.7 legend listing
  GER / FRA / JPN in map.html` — pins canonical content
  (id_code + name, owner index, palette colour) and that
  `provinces.svg` remains legend-free.

Total: 9 new doctest cases (810 → 819).

## 7. What M4.7 explicitly does NOT do

```text
no JavaScript / <script>
no <link> to external stylesheet
no inline event attributes (onclick / onmouseover / onload / ...)
no inline style="..." attributes on individual elements
no <meta name="viewport">
no CSS animations / transitions / media queries / @import / @font-face
no click handlers / clickable UI / hover state
no tooltips
no state mutation from the viewer
no font-family / font-size on the SVG <text> elements
   themselves (M4.4 contract preserved; only the CSS
   selectors `svg text` and `.legend` set the font, and
   only for the HTML viewer)
no ownership-dynamics layer (provinces / countries are static)
no neighbour / adjacency edges
no terrain / resources / population overlays
no events / AI / command integration
no new PlayerCommandKind
no runner CLI flag
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new gameplay
no atomic end_tick writes
no change to provinces.svg bytes
```

The next M4 sub-milestone is unspec'd and waits for the
reviewer.

## 8. Cross-references

- [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md)
  — the M4.6 `<style>` block M4.7 extends with three more
  selectors.
- [`m4-5-html-viewer-skeleton.md`](m4-5-html-viewer-skeleton.md)
  — the M4.5 HTML wrapper M4.7 grows by adding a `<ul>`
  after the inline SVG.
- [`m4-3-svg-owner-color-skeleton.md`](m4-3-svg-owner-color-skeleton.md)
  — the M4.3 palette M4.7's swatches read via
  `color_for_owner`.
- [`m4-4-svg-labels-skeleton.md`](m4-4-svg-labels-skeleton.md)
  — the `xml_text_escape` helper M4.7 reuses for legend
  text content.
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants future milestones must preserve (M4.7 is
  consistent: no schema change, no new artefact, no
  command-gate change, no logs / events from interest
  groups; the bytes of `map.html` are documented load-bearing
  and same-state-deterministic).
