# M4.23 - M4 close-out

Companion notes for `feature/rfc090-m4-23-m4-closeout`.

M4.23 is the **M4 close-out** sub-milestone. It mirrors
M1.17 / M2.22 / M3.9 in shape: a docs-only PR that
publishes the milestone exit report, annotates the
in-progress checkpoint as historical, and flips the three
READMEs to "M4 closed". No code, no formula, no fixture,
no test change.

The canonical M4 record from this PR forward is
[`milestone-4-result.md`](milestone-4-result.md).

## 1. Scope

What ships:

```text
docs/milestone-4-result.md                     NEW — the M4
                                               exit report.
                                               7 sections:
                                                 1. M4.1–M4.23
                                                    ledger table
                                                 2. final M4 dataflow
                                                 3. final 10-artefact
                                                    contract
                                                 4. save schema (v12
                                                    floor)
                                                 5. architectural
                                                    invariants every
                                                    future milestone
                                                    must preserve
                                                 6. deferred items
                                                    (A defer-to-M5+,
                                                     B post-M4 polish,
                                                     C not-needed-for-
                                                       close)
                                                 7. recommended next
                                                    milestone
                                                    candidates
                                               Closes with the
                                               literal "M4 closes
                                               here."
docs/milestone-4-checkpoint.md                 ANNOTATED as
                                               historical. Top-of-
                                               file note points at
                                               milestone-4-result.md
                                               as the new
                                               authoritative
                                               record; status
                                               changes from
                                               "in progress" to
                                               "historical (M4
                                               closed at M4.23)".
                                               Body kept verbatim
                                               for archaeology.
README.md                                      Status section
                                               flipped to "M4
                                               closed"; latest
                                               shipped becomes
                                               M4.23; M3-style
                                               ledger paragraph
                                               for M4.1–M4.23;
                                               next-candidate
                                               line: "TBD —
                                               awaits explicit
                                               reviewer
                                               direction".
docs/README.md                                 Table row for
                                               M4.23; reading-
                                               order chain ends
                                               at M4.23 with
                                               pointer to
                                               milestone-4-result.md;
                                               what's-next
                                               section flipped
                                               to "M4 closed".
rfc/README.md                                  Chinese paragraph
                                               for M4.23; M4
                                               marked closed.
docs/m4-23-m4-closeout.md                      this file.
```

## 2. Why a dedicated close-out PR

The 2026-05-17 force-reset lesson (see
`milestone-3-result.md` §7) is the documented reason for
"don't write the exit report until the milestone is
actually exiting". A previous attempt at M3.7+ drifted
into premature close-out + invented M4.X numbers + a 9th
artefact, and was force-reset. The recovery pattern that
worked: each milestone gets a dedicated final close-out
PR that does nothing else.

M1.17, M2.22, M3.9 all followed that pattern. M4.23
follows it for M4.

The M4.22 close-out readiness checkpoint explicitly left
the choice to the reviewer (one of: M4 close-out / one
more polish PR / stop M4 and move to M5). The reviewer
chose M4 close-out, so M4.23 is the close-out PR.

## 3. What this PR does NOT do

```text
no new system
no new formula
no new artefact (still 10)
no save schema bump (still v12)
no new state field
no new fixture
no new test
no new InterestGroupKind / PlayerCommandKind
no AI / events / commands / state mutation
no runner CLI flag
no renderer behaviour change (svg_export.cpp / .hpp
   untouched in M4.23)
no change to provinces.svg or map.html bytes
   (renderer is byte-identical with M4.22)
no "M5 in progress" wording (M5 starts in its own
   deliberate first sub-milestone PR; M4.23 makes
   no claim about which milestone is next)
no claim about which of the deferred items will land
   when (the M4 exit report lists them but leaves
   prioritisation to the reviewer / future milestones)
no test-suite changes (892 doctest cases / 61742
   assertions identical with M4.22)
```

## 4. Test plan

No new tests in M4.23 itself (close-out PRs ship docs
only — the M4.22 close-out readiness PR already added
integration test G as the consolidated "M4 viewer
contract complete" pin). Verification:

- `./build/bin/Debug/leviathan_tests.exe` → 892 cases /
  61742 assertions / 0 failed (unchanged from M4.22).
- M3.7 / M2.22 / M1.17 byte-identical determinism
  contracts continue to pass by construction (no
  renderer change).

## 5. Cross-references

- [`milestone-4-result.md`](milestone-4-result.md) — the
  new authoritative M4 record. Read this for the
  ledger, dataflow, contract, invariants, deferred
  items, and next-milestone candidates.
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — frozen-as-historical by this PR. Future readers
  wanting "what does the M4 contract look like right
  now?" should read `milestone-4-result.md` instead.
- [`m3-9-m3-closeout.md`](m3-9-m3-closeout.md) — the M3
  precedent M4.23 mirrors in shape.
- [`m2-22-end-to-end-tests.md`](m2-22-end-to-end-tests.md)
  — the M2 close-out precedent.
- All `m4-NN-*.md` design notes (m4-1 through m4-22) —
  the per-sub-milestone records. Together with
  `milestone-4-result.md` these form the complete M4
  archive.

**M4 closes here.**
