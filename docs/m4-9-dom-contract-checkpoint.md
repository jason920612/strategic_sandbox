# M4.9 - HTML DOM contract checkpoint

Companion notes for `feature/rfc090-m4-09-dom-contract-checkpoint`.

M4.9 is the **checkpoint sub-milestone** for the SVG / HTML
viewer stack built across M4.1–M4.8. Like M3.7 was for the
M3.1–M3.6 reaction loop, M4.9 ships zero new behaviour — it
formally pins the M4.2–M4.8 SVG / HTML DOM contract through
three new integration tests plus a single-page status
snapshot. **M4 is NOT closed**; M4.9 is a checkpoint, not an
exit report.

The canonical M4 status snapshot is
[`milestone-4-checkpoint.md`](milestone-4-checkpoint.md);
this file covers the M4.9-specific deliverable in isolation
so the per-sub-milestone naming convention used through
m4-1 ... m4-8 is unbroken.

## 1. Scope

M4.9 does NOT add a new gameplay system, formula, artefact,
save schema bump, fixture, runner CLI flag, or any
interactivity. It pins the existing contract.

What ships:

```text
tests/integration/m4_dom_contract_test.cpp   3 new doctest cases
docs/milestone-4-checkpoint.md               M4 status snapshot
docs/m4-9-dom-contract-checkpoint.md         this file
README.md / docs/README.md / rfc/README.md   "M4.9 checkpoint
                                              shipped" framing
                                              (M4 stays in progress)
827 → 830 doctest cases
```

The three integration cases:

1. **Uniform identity surface across both artefacts.** For
   every canonical province (berlin / paris / tokyo), each
   of the four `data-*` attribute substrings (`data-id`,
   `data-owner`, `data-owner-code`, `data-name`) appears at
   least twice in both `provinces.svg` and `map.html` —
   once on `<circle>`, once on `<text>`. This is the
   end-to-end check that M4.8's "widened identity surface"
   actually reaches both artefacts.
2. **Legend rows 1:1 with `state.countries`.** `map.html`
   carries one `<li data-owner="N">` per canonical country,
   and each row's body contains the country's `id_code`.
   `provinces.svg` has no legend (`<li>` / `class="legend"`
   both absent).
3. **No-interactivity invariant.** Both artefacts contain
   no `<script>`, no `</script>`, no `<link>`, no common
   inline event attributes (`onclick` / `onmouseover` /
   `onload` / `onmousedown` / `onkeydown`), and no
   per-element `style="..."`. `provinces.svg` additionally
   has no `<style>` block and no `font-family` — CSS is
   HTML-wrapper-only.

Per-element / per-rule pinning already lives in
`tests/systems/svg_export_test.cpp` and the M4.x sections
of `tests/systems/runner_test.cpp`; these three integration
cases are the **end-to-end** check at the canonical
fixture.

## 2. Why a checkpoint, not an exit

The reviewer's spec said: "**正式釘住 M4.2–M4.8 的 SVG/HTML
DOM contract，作為未來 clickable UI 前的 checkpoint**".
M4 has clear future sub-milestones (clickable UI, hover
state, tooltips, neighbour adjacency, terrain, etc.) that a
close-out at M4.9 would prematurely freeze.

The same lesson the 2026-05-17 force-reset taught for M3
applies: don't write the exit report until the milestone is
actually exiting. M3.7 took the same pattern (checkpoint
without close-out). M4.9 follows it.

## 3. The DOM contract being pinned

See `milestone-4-checkpoint.md` for the full spec. Summary:

```text
provinces.svg + map.html share one <svg> body via
   render_svg_root(state)
viewBox 0 0 1000 1000; cx = node.x * 1000; cy = node.y * 1000
one <circle> + one <text> per ProvinceNode, interleaved
each <circle> carries: cx, cy, r=8, fill, +four data-*
each <text>   carries: x, y, text-anchor, +four data-*, body
the four data-* attributes:
  data-id          = ProvinceNode::id_code
  data-owner       = owner.value() raw int
  data-owner-code  = state.countries[owner].id_code (or "")
  data-name        = ProvinceNode::name
all data-* values XML-attribute-escaped; <text> body XML-text-escaped
map.html wraps the SVG body in HTML5 with:
  <style>: six rules (body / svg / svg text / .legend /
           .legend li / .legend .swatch)
  <body>:  inline SVG (no XML prolog) + <ul class="legend">
            with one <li data-owner="N"> per state.countries[i]
no JavaScript / <script> / <link> / inline event attrs /
   inline style="..." per element / <meta viewport> /
   CSS animations / transitions / media queries / @import /
   @font-face
provinces.svg stays CSS-free / legend-free
artefact set = 10 files; save format = v12
```

The contract is documented load-bearing: future M4.x sub-
milestones can extend (e.g. add new `data-*` attributes,
add a new CSS rule, add a new HTML element after the
legend) but should not silently break the keys downstream
code may already grep against.

## 4. What M4.9 does NOT do

```text
no new system
no new formula
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new fixture
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
no change to provinces.svg or map.html bytes
```

The renderer is byte-identical with M4.8 — M4.9 only adds
tests and docs.

## 5. Cross-references

- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot M4.9 publishes (this is the
  canonical place to read "what does the M4 contract look
  like right now?").
- [`milestone-3-checkpoint.md`](milestone-3-checkpoint.md) +
  [`m3-7-reaction-loop-integration-checkpoint.md`](m3-7-reaction-loop-integration-checkpoint.md)
  — the M3.7 checkpoint precedent M4.9 follows in shape.
- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 `data-*` widening whose end-to-end behaviour
  test #1 pins.
- [`m4-7-html-legend-skeleton.md`](m4-7-html-legend-skeleton.md)
  — the M4.7 legend whose 1:1 country correspondence test
  #2 pins.
- [`m4-5-html-viewer-skeleton.md`](m4-5-html-viewer-skeleton.md)
  + [`m4-6-html-viewer-css-skeleton.md`](m4-6-html-viewer-css-skeleton.md)
  — the M4.5 / M4.6 "no JS / no `<link>` / no inline event
  / no per-element inline style" invariants test #3 pins.
- [`milestone-3-result.md`](milestone-3-result.md) §5 — the
  M3 invariants every future milestone must preserve (M4.9
  is consistent: no schema change, no new artefact, no
  command-gate change, no logs / events from interest
  groups; M4 contract is documented load-bearing in the
  same shape).
