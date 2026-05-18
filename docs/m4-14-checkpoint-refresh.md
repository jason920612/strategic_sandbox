# M4.14 - DOM contract checkpoint refresh

Companion notes for `feature/rfc090-m4-14-checkpoint-refresh`.

M4.14 refreshes the M4.9 checkpoint (`docs/milestone-4-checkpoint.md`)
to cover the four surfaces that landed in M4.10–M4.13:

- **M4.10** — first inline `<script>` in `map.html`
  (asymmetric JS boundary: `provinces.svg` stays
  script-free; `map.html` carries exactly one inline
  script).
- **M4.11** — details panel `<dt>` labels decoupled
  from raw `data-*` keys (`Province ID` / `Owner Index`
  / `Owner Code` / `Province Name`).
- **M4.12** — transient `.selected` class + CSS
  highlight + `selectProvince(el)` helper (purely
  DOM-level; lost on reload).
- **M4.13** — fifth `data-owner-name` attribute on
  every `<circle>` and `<text>`; details panel
  `fields` array grew 4 → 5 entries with the new
  `Owner Name` row.

M4.14 mirrors M4.9's role: a checkpoint, **not** an exit
report. M4 remains in progress; this sub-milestone only
brings the canonical snapshot up to date and adds one new
integration assertion. Zero new behaviour.

## 1. Scope

What ships:

```text
docs/milestone-4-checkpoint.md
  - Full refresh from M4.9 (M4.2–M4.8 only) to M4.14
    (M4.2–M4.13).
  - Sub-milestones-shipped list extends 4.1–4.9 → 4.1–4.14.
  - SVG body shape: 4 data-* attrs → 5 (added
    data-owner-name with the M4.13 shared-bounds-check note).
  - HTML wrapper shape: details panel + .selected CSS +
    inline <script> + 5-entry fields array now documented
    inline alongside the existing M4.6/M4.7 CSS + legend.
  - DOM identity contract: explicit M4.13 fifth-attr line;
    new "interactivity surface (map.html only, M4.10–M4.12)"
    section enumerating <div id="details">, .selected,
    selectProvince, showDetails, fields array.
  - Invariants future M4.x must preserve: rewritten with
    the M4.10 asymmetric one-script invariant, the M4.13
    additive-only widening rule, the XSS-safe + read-only
    + no-persistence rules for the click handler.
  - Deferred items: bucketed into HOVER+TOOLTIPS, KEYBOARD+A11Y,
    PERSISTENT SELECTION, DOM EXTENSIONS, VISUAL POLISH,
    INFRASTRUCTURE. JavaScript / clickable UI / selection
    class are no longer deferred — they shipped in
    M4.10/M4.12.
  - Integration test coverage: now 4 tests (was 3); test D
    is new in M4.14.
  - What M4.14 does NOT do: refreshed for this PR.
tests/integration/m4_dom_contract_test.cpp
  - New TEST_CASE: "M4 DOM contract: map.html
    click-handler script carries the M4.13 five-entry
    fields list with the canonical labels".
  - Pins the five data-* attribute names AND the five
    human-readable labels appearing inside the inline
    <script> on the canonical scenario's map.html.
  - Confirms provinces.svg carries none of the JS-literal
    forms.
README.md / docs/README.md / rfc/README.md
  - M4.14 entry on each.
docs/m4-14-checkpoint-refresh.md
  - this file.
```

## 2. Why a checkpoint refresh, not a M4 close-out

M3.7 set the precedent: a checkpoint pins the contract
**without** writing the exit report. The reviewer's
spec for M4.14 was explicit:

> 只更新 M4.9 checkpoint 後已變更的 contract (first JS,
> selected class, owner-name fifth data attr), 補一個
> 小型 integration assertion 或 docs refresh; 不要新增
> 功能、不要做 M4 close-out、不要新增 artifact / schema /
> gameplay.

M4 has more sub-milestones to ship (hover state,
tooltips, keyboard nav / `aria-*` polish, selection
persistence, neighbour adjacency, terrain, responsive
sizing, ...). Each of those is a future sub-milestone.
M4.14 only keeps the contract-snapshot doc current so the
**next** one (M4.15 or later) has an accurate read.

The 2026-05-17 force-reset history (`milestone-3-result.md`
§7) is the documented reason for the "don't write the exit
report until the milestone is actually exiting" rule. M3.7
followed it; M4.9 followed it; M4.14 follows it.

## 3. The new integration test (test D)

```cpp
TEST_CASE("M4 DOM contract: map.html click-handler script
          carries the M4.13 five-entry fields list with
          the canonical labels") {
    // Runs the canonical scenario for 1 day, reads
    // map.html + provinces.svg, asserts:
    //   - five data-* attribute JS literals appear in
    //     map.html: "data-id", "data-owner",
    //     "data-owner-code", "data-owner-name", "data-name"
    //   - five human-readable label literals appear:
    //     "Province ID", "Owner Index", "Owner Code",
    //     "Owner Name", "Province Name"
    //   - provinces.svg carries none of the JS literal
    //     forms (the bare SVG attribute form
    //     `data-owner-name="..."` still does — that's
    //     M4.13's contract, not this test's concern)
}
```

Why this test specifically:

1. It is the **end-to-end** mirror of the M4.11/M4.13
   `svg_export_test` unit cases that already pin the
   labels and attribute names. The unit cases catch
   `render_map_html` regressions in isolation; this test
   catches them through the actual runner / canonical
   fixture path.
2. It tells future-me what shape "shipped" the M4.10–M4.13
   click handler. If a future M4.x refactor accidentally
   shrinks the fields array back to four entries (e.g.
   collapsing `data-owner-code` and `data-owner-name`
   into a single "owner" cell), this gate trips.
3. It does NOT pin handler body details (the closure
   shape, the helper function names, the selector
   strings) — those live in the per-element unit tests.
   Test D is intentionally coarse to match the
   integration-test cadence M4.9 established.

## 4. What M4.14 does NOT do

```text
no new system
no new formula
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new fixture
no new InterestGroupKind / PlayerCommandKind
no new feature surface (M4.14 is docs + 1 integration test)
no rename of any data-* attribute
no change to the click handler / details panel /
   .selected CSS / fields array bytes
no <meta name="viewport">
no CSS animations / transitions / media queries
no neighbour / adjacency edges / terrain / overlays
no events / AI / command integration
no hover state / tooltip / keyboard nav / aria-* polish
no selection persistence across reloads
no runner CLI flag
no atomic end_tick writes
no M4 close-out
no docs/milestone-4-result.md
no "M4 closed" wording
no change to provinces.svg or map.html bytes
   (the renderer is byte-identical with M4.13)
```

## 5. Cross-references

- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the canonical M4 status snapshot. M4.14 is the
  refresh; future M4.x sub-milestones read this file
  for "what does the contract look like right now?".
- [`m3-7-reaction-loop-integration-checkpoint.md`](m3-7-reaction-loop-integration-checkpoint.md)
  — the M3 precedent for "checkpoint, not exit". M4.9
  followed it; M4.14 follows both.
- [`m4-9-dom-contract-checkpoint.md`](m4-9-dom-contract-checkpoint.md)
  — the original M4.9 checkpoint PR's design note. M4.14
  is a strict superset (adds M4.10–M4.13 surfaces;
  preserves the M4.9 three-test integration coverage as
  tests A/B/C and adds test D).
- [`m4-10-clickable-ui-skeleton.md`](m4-10-clickable-ui-skeleton.md)
  — the asymmetric one-script invariant test D
  implicitly relies on (M4.10 invariant, still pinned
  by test C).
- [`m4-11-details-labels-polish.md`](m4-11-details-labels-polish.md)
  — the source of the four human-readable labels test D
  pins (`Province ID` / `Owner Index` / `Owner Code` /
  `Province Name`).
- [`m4-12-selected-state-css-skeleton.md`](m4-12-selected-state-css-skeleton.md)
  — the transient `.selected` class M4.14 documents in
  the refreshed checkpoint but does not unit-test in this
  PR (M4.12's own svg_export_test cases already cover
  the surface).
- [`m4-13-details-owner-name-polish.md`](m4-13-details-owner-name-polish.md)
  — the source of the fifth attribute (`data-owner-name`)
  and the fifth label (`Owner Name`) test D pins.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must
  preserve. M4.14 is consistent: no schema bump, no new
  artefact, no command-gate change, no events / logs
  from the viewer, no state mutation.
