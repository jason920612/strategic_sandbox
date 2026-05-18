# M4.18 - accessibility checkpoint refresh

Companion notes for `feature/rfc090-m4-18-accessibility-checkpoint-refresh`.

M4.18 refreshes the M4 status snapshot
(`docs/milestone-4-checkpoint.md`) to cover the three a11y
surfaces that landed in M4.15–M4.17:

- **M4.15** — keyboard focus skeleton (`tabindex="0"` on
  `<circle>` + `<text>` + keydown listener for Enter/Space).
- **M4.16** — focus-visible CSS skeleton (blue `#1976d2`
  `:focus-visible` rings visually distinct from M4.12 black
  `.selected` stroke).
- **M4.17** — ARIA labels skeleton (`role="button"` +
  `aria-label="<name>, <owner_name>"` on `<circle>` +
  `<text>`).

M4.18 mirrors M4.14's role exactly: a checkpoint refresh,
**not** an exit report. M4 remains in progress; this
sub-milestone only brings the canonical snapshot up to date
and adds one new integration assertion. Zero new behaviour.

## 1. Scope

What ships:

```text
docs/milestone-4-checkpoint.md
  - Full refresh covering M4.15–M4.17 alongside the prior
    M4.2–M4.14 surfaces.
  - Sub-milestones list extends 4.1–4.14 → 4.1–4.18.
  - SVG body shape: <circle> + <text> attribute list now
    includes tabindex="0" (M4.15), role="button" + aria-label
    (M4.17) alongside the M4.8/M4.13 data-* attrs.
  - HTML wrapper shape: <style> block grows from 13
    selectors at M4.14 to 17 at M4.18 (four new M4.16
    :focus / :focus-visible rules); listener loop now wires
    both click + keydown via shared activate() closure.
  - DOM identity + interactivity contract: new
    "Accessibility surface (M4.15–M4.17)" section enumerating
    tabindex, keydown, :focus-visible CSS, role=button,
    aria-label, decorative-legend-swatch rule.
  - Invariants future M4.x must preserve: new bullets for
    the asymmetric ARIA surface (only role=button +
    aria-label on circle/text), the shared activate()
    closure, :focus-visible vs bare :focus, and the
    keyboard interactivity surface staying separable from
    the existing M4.12 selection state.
  - Deferred items: rebucketed. KEYBOARD+FOCUS surface
    shipped; BROADER ARIA explicitly still deferred
    (aria-selected / aria-current / aria-pressed /
    aria-live / aria-describedby / aria-labelledby);
    KEYBOARD POLISH (arrow-key nav, Escape-to-clear,
    Tab-within-panel) still deferred.
  - Integration test count: was 5 at M4.14; now 6 at M4.18.
  - What M4.18 does NOT do: refreshed for this PR.

tests/integration/m4_dom_contract_test.cpp
  - New TEST_CASE F: "M4 DOM contract: M4.17 role=button +
    aria-label end-to-end; M4.16 focus-visible CSS
    asymmetric".
  - Pins canonical scenario: 6 role="button" occurrences
    per artefact (3 provinces × circle+text); per-province
    aria-label="<name>, <owner_name>" appears twice per
    artefact; 3 legend swatches all carry NO role /
    aria-label / tabindex; M4.16 :focus-visible CSS
    appears in map.html but NOT in provinces.svg; the
    still-deferred ARIA surface (aria-selected /
    aria-current / aria-pressed / aria-live /
    aria-describedby / aria-labelledby) stays absent in
    both artefacts.

README.md / docs/README.md / rfc/README.md
  - M4.18 entry on each.

docs/m4-18-accessibility-checkpoint-refresh.md
  - this file.
```

## 2. Why a checkpoint refresh, not an M4 close-out

Same reasoning as M4.14. The reviewer's spec for M4.18 was
explicit:

> 只更新 M4 checkpoint docs + integration test，反映
> M4.15–M4.17 的 keyboard focus、focus-visible、ARIA
> label surface；不要新增功能、不要 close M4、不要新增
> artifact/schema/gameplay.

M4 has more sub-milestones to ship (broader ARIA, arrow-key
nav, Escape-to-clear, hover state, tooltips, selection
persistence, neighbour adjacency, terrain, responsive
sizing, ...). M4.18 only keeps the contract-snapshot doc
current so the next one (M4.19 or later) has an accurate
read.

The 2026-05-17 force-reset history
(`milestone-3-result.md` §7) is the documented reason for
"don't write the exit report until the milestone is
actually exiting". M3.7 followed it; M4.9 followed it;
M4.14 followed it; M4.18 follows it.

## 3. The new integration test (test F)

```cpp
TEST_CASE("M4 DOM contract: M4.17 role=button + aria-label
          end-to-end; M4.16 focus-visible CSS asymmetric") {
    // Runs the canonical scenario for 1 day, reads
    // map.html + provinces.svg, asserts:
    //   - 6 role="button" per artefact (3 provinces ×
    //     circle+text)
    //   - 2 aria-label="<name>, <owner_name>" per
    //     province per artefact, with the canonical
    //     composed values ("Berlin, Germany",
    //     "Paris, France", "Tokyo, Japan")
    //   - All 3 legend swatches carry NO role /
    //     aria-label / tabindex (decorative invariant)
    //   - map.html <style> block carries
    //     :focus-visible + svg circle:focus-visible +
    //     svg text:focus-visible + #1976d2
    //   - provinces.svg carries NO :focus-visible /
    //     :focus / #1976d2 (CSS is HTML-wrapper-only)
    //   - Still-deferred ARIA (aria-selected /
    //     aria-current / aria-pressed / aria-live /
    //     aria-describedby / aria-labelledby) absent
    //     in BOTH artefacts.
}
```

Why this test specifically:

1. It is the **end-to-end** mirror of the M4.15/M4.16/M4.17
   `svg_export_test` unit cases that already pin the
   tabindex, focus-visible CSS, role, and aria-label. The
   unit cases catch `render_svg_root` / `render_map_html`
   regressions in isolation; this test catches them through
   the actual runner / canonical fixture path.
2. It pins the **decorative legend swatch invariant** at
   the integration layer too: a future refactor that
   accidentally adds `role` / `aria-label` / `tabindex` to
   the legend swatch circles (e.g. by sharing the
   `render_svg_root` SVG body for the swatch) trips this
   gate.
3. It pins the **still-deferred ARIA surface** absent
   end-to-end so a future PR that mistakenly adds
   `aria-selected` (or similar) without an explicit M4.x
   sub-milestone for the broader ARIA model trips this
   gate.

## 4. What M4.18 does NOT do

```text
no new system
no new formula
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new fixture
no new InterestGroupKind / PlayerCommandKind
no new feature surface (M4.18 is docs + 1 integration test)
no rename of any data-* attribute
no change to render_svg_root / render_map_html bytes
   (M4.15–M4.17 shapes byte-identical)
no broader ARIA (aria-selected / aria-current /
   aria-pressed / aria-live / aria-describedby /
   aria-labelledby) — still deferred
no keyboard polish beyond M4.15 (no arrow-key nav,
   no Escape-to-clear, no Tab-within-panel) — still
   deferred
no hover state / tooltip
no animation / transition
no selection persistence across reloads
no runner CLI flag
no atomic end_tick writes
no M4 close-out
no docs/milestone-4-result.md
no "M4 closed" wording
no change to provinces.svg or map.html bytes
   (M4.18 only adds tests + docs; the renderer is
   byte-identical with M4.17)
```

## 5. Cross-references

- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the canonical M4 status snapshot. M4.18 is the
  refresh; future M4.x sub-milestones read this file
  for "what does the contract look like right now?".
- [`m4-14-checkpoint-refresh.md`](m4-14-checkpoint-refresh.md)
  — the M4.14 PR M4.18 mirrors. M4.14 refreshed for
  M4.10–M4.13; M4.18 refreshes for M4.15–M4.17.
- [`m3-7-reaction-loop-integration-checkpoint.md`](m3-7-reaction-loop-integration-checkpoint.md)
  — the M3 precedent for "checkpoint, not exit". M4.9
  followed it; M4.14 followed it; M4.18 follows it.
- [`m4-15-keyboard-focus-skeleton.md`](m4-15-keyboard-focus-skeleton.md)
  — the M4.15 keyboard-focus surface test F pins
  end-to-end (via the existing integration test E +
  the new F).
- [`m4-16-focus-visible-skeleton.md`](m4-16-focus-visible-skeleton.md)
  — the M4.16 focus-visible CSS test F pins end-to-end.
- [`m4-17-aria-labels-skeleton.md`](m4-17-aria-labels-skeleton.md)
  — the M4.17 ARIA surface test F pins end-to-end. Also
  the source of the explicit "still-deferred ARIA"
  list test F enforces.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  the M3 invariants every future milestone must
  preserve. M4.18 is consistent: no schema bump, no new
  artefact, no command-gate change, no events / logs
  from the viewer, no state mutation.
