# M4.22 - close-out readiness checkpoint

Companion notes for `feature/rfc090-m4-22-closeout-readiness-checkpoint`.

M4.22 is the **close-out readiness checkpoint** for M4 —
NOT the close-out itself. After M4.21 the M4 viewer
stack is structurally complete; this PR formally
assesses readiness, locks the current contract with one
consolidated end-to-end integration test, fixes a small
math wording the PR #87 reviewer flagged, and leaves
the M4 exit report (`docs/milestone-4-result.md`) for a
deliberate follow-up PR once the reviewer gives the
"do M4 close-out" green light.

**This PR mirrors M3.7's role for the M3 reaction loop**:
docs + 1 integration test, no renderer behaviour change,
explicitly NOT writing the exit report.

## 1. Scope

What ships:

```text
docs/milestone-4-checkpoint.md
  - Sub-milestones list adds M4.22.
  - "Refreshed at" stamp adds the M4.22 stage.
  - NEW section 9 "Close-out readiness assessment" with
    four subsections:
      9.1  one-line summary of what M4 shipped
      9.2  still-deferred items, categorised as
           A (defer-to-M5+ gameplay-domain),
           B (recommended post-M4 follow-up polish),
           C (not-needed-for-close nice-to-haves)
      9.3  close-out readiness verdict — M4 is
           structurally ready; the exit report stays
           deferred until reviewer green-light
      9.4  fix-up note for the PR #87 wording flag

tests/integration/m4_dom_contract_test.cpp
  - NEW test G "M4 viewer contract complete" —
    canonical map.html carries EVERY M4.x surface
    marker (viewBox, circle, text, 5 data-* attrs,
    tabindex, role, aria-label, details panel, legend,
    addEventListener click + keydown + mouseover +
    mouseout, hover-status, .selected CSS,
    :focus-visible, :hover, fields-array labels,
    viewport meta, @media block). provinces.svg
    carries the SVG-body subset and NONE of the
    HTML-wrapper surfaces. Pin save_version: 12 and
    all 7 unconditional artefacts present.

src/leviathan/systems/svg_export.cpp
  - Fix the 1040px math comment per PR #87 reviewer
    flag: "1000 (SVG) + 2 * 20 (body padding) = 1040"
    (the SVG's 1px border lives INSIDE the padded
    column, not adding to layout width).

include/leviathan/systems/svg_export.hpp
  - Same math-wording fix in the M4.21 intro
    paragraph.

README.md / docs/README.md / rfc/README.md
  - Same math-wording fix in the M4.21 ledger entries.

docs/m4-21-responsive-viewport-skeleton.md
  - Same math-wording fix in the design note; added a
    short "Fixed in M4.22" note acknowledging the
    earlier drift.

docs/m4-22-closeout-readiness-checkpoint.md
  - this file.
```

## 2. The reviewer's PR #87 wording flag

PR #87 reviewer caught:

> 1040px 的計算在 PR body 裡寫成 `1000 + 2*20 + 2*1
> = 1042` 其實數學上會是 1042，不過實作和測試採用
> 1040px，也就是 SVG width + body padding，不把
> border 算進去。

M4.22 propagates the corrected wording across the
sources where the math appeared (svg_export.cpp,
svg_export.hpp, root + docs + rfc READMEs, M4.21
design note). The implementation has always been
correct at 1040px — only the explanatory text drifted.
The fix is a one-line acknowledgement in
`m4-21-responsive-viewport-skeleton.md`'s "Why 1040px"
section so a future archaeologist sees the drift +
the correction.

## 3. The new consolidated integration test (test G)

`tests/integration/m4_dom_contract_test.cpp` already
contains six tests (A–F) added across M4.9, M4.14,
M4.18 covering individual contract clauses (data-*
identity surface, legend rows, asymmetric one-script
invariant, fields-list contract, tabindex + focus
surface, ARIA labels + focus-visible).

M4.22 adds test G — a single catch-all that touches
every M4.x surface marker in one runner-driven check.
It's the "did anything regress holistically?" test;
A–F catch per-clause regressions, G catches whole-
file regressions (e.g. a future PR that
short-circuits `render_map_html` under some condition
or drops an entire surface block).

Test G also pins:
- All 7 unconditional artefacts present on the
  canonical fixture (`save.json`, `events.jsonl`,
  the 3 M3 CSVs, `provinces.svg`, `map.html`).
- `save.json` carries `"save_version": 12` (the M4.1
  schema bump that M4 stays at).

## 4. Close-out readiness verdict

**M4 is structurally ready for close-out.** Concrete
evidence:

- Every shipped surface has unit-test coverage in
  `tests/systems/svg_export_test.cpp` (over 600
  cases / 60000+ assertions just in the M4.x section).
- Every shipped surface has end-to-end coverage in
  `tests/integration/m4_dom_contract_test.cpp` (tests
  A through G — 7 cases / many sub-checks).
- Every sub-milestone has its own per-PR design note
  in `docs/m4-NN-*.md` (22 files: m4-1 through m4-22).
- The renderer is byte-deterministic across all
  M1.17 / M2.22 / M3.7 byte-identical determinism
  contracts; M4.1's save format v12 has held since.
- The artefact set has stayed at 10 since M4.5;
  hover-status / focus rings / role+aria-label /
  viewport meta were all narrowly-scoped renderer
  changes inside the existing artefacts.

What remains undone is **by design** — the M4.22
checkpoint section 9.2 categorises the deferred
items. None are M4-close-out blockers.

## 5. What M4.22 does NOT do

```text
no M4 close-out (no docs/milestone-4-result.md;
   no "M4 closed" wording in any README)
no new feature surface (M4.22 is docs + 1 integration
   test + 1 math-wording fix)
no renderer behaviour change (svg_export.cpp change
   is comment-only, not behavioural)
no new system / formula / artefact / state field /
   fixture / InterestGroupKind / PlayerCommandKind
no save schema bump (still v12)
no rename of any data-* attribute
no broader ARIA / pair-hover / position-aware tooltip
   / keyboard polish beyond M4.15 / selection
   persistence / hover delay / dark-mode / etc. —
   those are all explicitly deferred per checkpoint
   section 9.2
no change to provinces.svg or map.html bytes
   (M4.22 only ships docs + 1 test + comment + doc
   wording fix; the rendered artefacts are byte-
   identical with M4.21)
```

## 6. Recommendation to the reviewer

The next sub-milestone after M4.22 should be one of:

1. **M4 close-out** — publish
   `docs/milestone-4-result.md` (mirror
   `milestone-3-result.md`'s shape: M4.1–M4.22
   ledger, dataflow summary, 10-artefact contract,
   v12 save floor, architectural invariants future
   milestones must preserve, deferred items
   categorised per checkpoint section 9.2, neutral
   next-milestone candidates). Flip the three
   READMEs to "M4 closed". This would be M4.23 (or
   whatever number the reviewer wants).

2. **One more polish PR** — if any of the
   category-B items (broader ARIA, pair-hover,
   etc.) feels worth landing inside M4 rather than
   deferring, that's a one-PR scope each. Reviewer
   chooses.

3. **Stop and move to M5** — defer M4 close-out
   until a sensible moment alongside M5 work
   beginning.

M4.22 does NOT pick one of these — that's the
reviewer's call. M4.22 only confirms readiness and
documents the choice space.

## 7. Cross-references

- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the canonical M4 status snapshot;
  M4.22-refreshed with the close-out readiness
  assessment section.
- [`m3-7-reaction-loop-integration-checkpoint.md`](m3-7-reaction-loop-integration-checkpoint.md)
  — the M3 precedent for "checkpoint, not exit".
  M4.9 / M4.14 / M4.18 / M4.22 all follow that
  pattern; the M3 close-out was M3.9, which is the
  precedent the eventual M4 close-out will follow.
- [`milestone-3-result.md`](milestone-3-result.md)
  — the M3 exit report shape the eventual M4 exit
  report will mirror.
- [`m4-21-responsive-viewport-skeleton.md`](m4-21-responsive-viewport-skeleton.md)
  — the M4.21 design note that contained the
  doc-only `1042` math drift M4.22 corrects (the
  implementation always used 1040 correctly).
- All `m4-NN-*.md` design notes (m4-1 through
  m4-21) — together with `milestone-4-checkpoint.md`
  these form the complete record of what M4
  shipped. M4.22 doesn't change any of them
  (except the m4-21 wording fix).
