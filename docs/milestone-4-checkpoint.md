# Milestone 4 Checkpoint

Status: in progress

Companion notes for `feature/rfc090-m4-09-dom-contract-checkpoint`.

M4.9 is a checkpoint, not an exit report. M4 remains in
progress: this sub-milestone formally pins the M4.2–M4.8
SVG / HTML DOM contract at the seam between M4 and any
future M4.x (clickable UI, hover, tooltips, adjacency,
terrain, etc.). No new system, formula, artefact, save-
schema bump, runner CLI flag, or any interactivity.

The companion exit report (`docs/milestone-4-result.md`) is
deliberately not written yet. M4 has more sub-milestones to
ship; the close-out lands when the reviewer decides M4 is
done, not at M4.9.

This file mirrors `docs/milestone-3-checkpoint.md`'s role:
a single-page snapshot of M4 state as of M4.9 that future
sub-milestones can read for "what does the contract look
like right now?" without piecing together eight per-sub-
milestone notes.

## 1. M4 sub-milestones shipped

```text
M4.1  ProvinceNode data layer (save v11 → v12)
M4.2  first SVG renderer + provinces.svg as 9th artefact
M4.3  per-owner palette (kOwnerPalette + color_for_owner)
M4.4  per-node <text> label
M4.5  HTML wrapper + map.html as 10th artefact
M4.6  minimal viewer CSS (body / svg / svg text)
M4.7  static legend (<ul class="legend">)
M4.8  widened identity surface (data-* on <circle> AND <text>)
M4.9  DOM contract checkpoint (this PR)
```

## 2. Current artefact contract

End-of-run produces ten files. M3 close shipped eight; M4
added two:

```text
save.json                                  (M0.8,  required)
events.jsonl                               (M0.6,  required)
summary.csv                                (M0.10, opt-in)
countries.csv                              (M1.14, opt-in)
factions.csv                               (M1.16, opt-in)
interest_groups.csv                        (M3.5,  unconditional)
interest_group_country_feedback.csv        (M3.6,  unconditional)
interest_group_authority_pressure.csv      (M3.6,  unconditional)
provinces.svg                              (M4.2,  unconditional)
map.html                                   (M4.5,  unconditional)
```

`provinces.svg` and `map.html` share the same `<svg>` body
via the `render_svg_root` helper (M4.5 refactor); the
difference is only the wrapper (XML prolog + standalone SVG
vs HTML5 doc with `<head>` + `<body>`).

## 3. SVG body shape (shared between provinces.svg and map.html)

```text
<svg xmlns="http://www.w3.org/2000/svg"
     viewBox="0 0 1000 1000"
     width="1000" height="1000">

  for each ProvinceNode p in state.provinces (vector order):
    <circle
      cx="<p.x * 1000, fixed 2dp>"
      cy="<p.y * 1000, fixed 2dp>"
      r="8"
      fill="<color_for_owner(p.owner)>"
      data-id="<xml_attr_escape(p.id_code)>"
      data-owner="<p.owner.value()>"
      data-owner-code="<xml_attr_escape(owner_code)>"
      data-name="<xml_attr_escape(p.name)>"/>
    <text
      x="<cx>"
      y="<cy + kLabelYOffset>"            // kLabelYOffset = 22.0
      text-anchor="middle"
      data-id="<xml_attr_escape(p.id_code)>"
      data-owner="<p.owner.value()>"
      data-owner-code="<xml_attr_escape(owner_code)>"
      data-name="<xml_attr_escape(p.name)>"
    ><xml_text_escape(p.name)></text>

</svg>
```

where:

- `color_for_owner` returns
  `kOwnerPalette[owner.value() % kOwnerPaletteSize]` for
  valid owners and `kOwnerFallbackFill` (`#888888`) for
  negative owners.
- `owner_code` is
  `state.countries[owner.value()].id_code` for valid
  owners and the empty string otherwise.
- Insertion order follows `state.provinces` (no sort).
- LF line terminators throughout; fixed 2-space indent.
- `data-*` attribute values use `xml_attr_escape` (`& < >
  " '` → entities); the `<text>` body uses
  `xml_text_escape` (`& < >` only).
- Empty `state.provinces` produces a header-only SVG —
  the `<svg>` element is still written so the artefact
  contract is consistent.

## 4. HTML wrapper shape (map.html)

```text
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Leviathan Map</title>
  <style>
  body      { margin: 0; padding: 20px; background-color: #f0f0f0; }
  svg       { display: block; margin: 0 auto;
              border: 1px solid #888;
              background-color: #ffffff; }
  svg text  { font-family: sans-serif; }
  .legend   { list-style: none; padding: 0;
              margin: 20px auto; max-width: 1000px;
              font-family: sans-serif; }
  .legend li      { display: flex; align-items: center;
                    margin: 4px 0; }
  .legend .swatch { width: 16px; height: 16px;
                    margin-right: 8px; flex-shrink: 0; }
  </style>
</head>
<body>
  <svg ...>...</svg>            // same body as provinces.svg,
                                // no XML prolog
  <ul class="legend">
    for each CountryState c in state.countries (vector order):
      <li data-owner="<i>">
        <svg class="swatch" viewBox="0 0 16 16" width="16" height="16">
          <circle cx="8" cy="8" r="8"
                  fill="<color_for_owner(CountryId{i})>"/>
        </svg>
        <xml_text_escape(c.id_code)> &mdash;
        <xml_text_escape(c.name)>
      </li>
  </ul>
</body>
</html>
```

where:

- `<style>` block carries exactly six selectors (M4.6 three
  + M4.7 three). All are layout rules — no animations, no
  transitions, no media queries, no `@import`, no
  `@font-face`.
- The inline SVG body is byte-identical to
  `provinces.svg` minus the XML prolog.
- The `<ul class="legend">` appears **after** the inline
  SVG. One `<li data-owner="N">` per country in vector
  order; empty `state.countries` produces an empty `<ul>`.
- Per-row swatch is inline SVG (not an HTML element with
  `background-color`) so no element ever carries inline
  `style="..."`.

## 5. The DOM identity contract (load-bearing for future UI)

A future clickable UI / DOM script has these stable lookup
keys, guaranteed across `provinces.svg` and `map.html`:

```text
Province identity:
  - data-id          = ProvinceNode::id_code
  - data-owner       = owner.value() (raw int as string)
  - data-owner-code  = state.countries[owner].id_code (or "")
  - data-name        = ProvinceNode::name

  Present on BOTH <circle> AND <text> for every node.
  XML-attribute-escaped at every site.

Country identity (legend, map.html only):
  - <li data-owner="N">  where N == vector index in state.countries
  - <li> body text       contains the country's id_code

Coordinate space:
  - viewBox 0 0 1000 1000
  - cx = node.x * 1000   (std::fixed setprecision(2))
  - cy = node.y * 1000
  - text x = cx
  - text y = cy + 22.0   (kLabelYOffset)

Owner colour:
  - <circle fill> matches kOwnerPalette[owner.value() %
    kOwnerPaletteSize] for valid owners, kOwnerFallbackFill
    (#888888) for invalid.
  - Legend swatches use the same lookup.
```

## 6. Invariants future M4.x sub-milestones must preserve

```text
both artefacts stay deterministic (same state → same bytes)
both artefacts stay valid (parseable SVG / HTML5)
no removal of an existing data-* attribute
   (additive growth only; renaming = breaking change)
provinces.svg and map.html stay in sync on the SVG body
   (i.e. render_svg_root is the single source of truth)
the per-element no-inline-style invariant continues to hold
   (svg fill via presentation attribute is fine;
    inline style="..." on individual elements is not)
the legend keeps 1:1 correspondence with state.countries
the artefact set stays at 10 (any 11th needs its own
   sub-milestone with the cadence / determinism /
   pre-end_tick contracts documented)
save format stays v12 (any new persistent state bumps)
RunnerOptions::provinces_svg_path / RunnerOptions::map_html_path
   stay opt-in path overrides; no CLI flag is wired
all M4.5 / M4.6 / M4.7 "no" lists (no JS, no <script>,
   no <link>, no inline event attributes, no <meta viewport>,
   no CSS animations / transitions / media queries /
   @import / @font-face) stay enforced
```

## 7. Deferred items

These are intentionally not in M4 yet. M4.9 lists them so a
later sub-milestone has one canonical reference for what is
explicitly out of scope.

```text
JavaScript / clickable UI / event handlers / hover state /
   tooltips / state mutation from the viewer
legend filtering, sorting, or interactive selection
neighbour adjacency edges (SVG <line> / <polyline>)
terrain / resources / population overlays
SVG-side controller-vs-owner distinction
richer node fields (population, terrain, garrison, ...)
multi-province countries with explicit grouping
unowned provinces (current contract assumes owner resolves
   to a valid CountryId; empty data-owner-code is a
   defensive fallback for hand-built bad states, not a
   modelled "neutral" status)
<meta name="viewport"> + media queries for responsive sizing
font-family / font-size on the SVG <text> elements themselves
   (M4.4 contract preserved; only CSS selectors set fonts)
atomic end_tick writes (deferred since M2.9)
M4 close-out / exit report
```

## 8. Integration test coverage at the M4.9 checkpoint

`tests/integration/m4_dom_contract_test.cpp` (3 cases):

1. **Uniform identity surface across both artefacts.** For
   every canonical province (berlin / paris / tokyo), each
   of the four `data-*` attribute substrings appears at
   least twice (once on `<circle>`, once on `<text>`) in
   both `provinces.svg` and `map.html`.
2. **Legend rows correspond 1:1 with `state.countries`.**
   `map.html` carries one `<li data-owner="N">` per
   canonical country, and each row's body contains the
   country's `id_code`. `provinces.svg` has no legend.
3. **No-interactivity invariant.** Both artefacts contain
   no `<script>`, no `<link>`, no common inline event
   attributes (`onclick` / `onmouseover` / `onload` /
   `onmousedown` / `onkeydown`), and no per-element
   `style="..."`. Additionally, `provinces.svg` has no
   `<style>` block and no `font-family` — CSS is HTML-
   wrapper-only.

Per-element / per-rule pinning lives in
`tests/systems/svg_export_test.cpp` and the M4.x sections
of `tests/systems/runner_test.cpp`; the integration tests
above are the end-to-end check against the canonical
fixture.

## 9. What M4.9 does NOT do

```text
no new system
no new formula
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new InterestGroupKind / PlayerCommandKind
no JavaScript / click handler / event handler / hover /
   tooltip / state mutation
no <script>, no <link>, no inline event attributes,
   no per-element inline style="..."
no <meta name="viewport">
no CSS animations / transitions / media queries / @import
no neighbour / adjacency edges
no terrain / resources / population overlays
no events / AI / command integration
no runner CLI flag
no atomic end_tick writes
no M4 close-out
no docs/milestone-4-result.md
no "M4 closed" wording
no change to provinces.svg or map.html bytes (M4.9 only
   adds tests + docs; the renderer is byte-identical with
   M4.8)
```
