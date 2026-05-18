# M5.10 - M5 close-out

Companion notes for `feature/rfc090-m5-10-m5-closeout`.

M5.10 is the **M5 close-out** sub-milestone. It mirrors
M1.17 / M2.22 / M3.9 / M4.23 in shape: a docs-only PR that
publishes the milestone exit report, annotates the
in-progress checkpoint as historical, and flips the three
READMEs to "M5 closed". No code, no formula, no fixture, no
test change.

The canonical M5 record from this PR forward is
[`milestone-5-result.md`](milestone-5-result.md).

## 1. Scope

What ships:

```text
docs/milestone-5-result.md                     NEW — the M5
                                               exit report.
                                               7 sections:
                                                 1. M5.1–M5.10
                                                    ledger table
                                                 2. final M5 dataflow
                                                    (incl. monthly
                                                     step 7 wiring
                                                     + inner-loop)
                                                 3. final 10-artefact
                                                    contract (M5
                                                    added zero)
                                                 4. save schema
                                                    (v14 floor)
                                                 5. architectural
                                                    invariants every
                                                    future milestone
                                                    must preserve
                                                    (organised into
                                                     5.1 schema, 5.2
                                                     evaluator, 5.3
                                                     firer, 5.4
                                                     effects, 5.5
                                                     composition,
                                                     5.6 monthly
                                                     wiring, 5.7
                                                     canonical-non-
                                                     fire, 5.8 no
                                                     new artefact,
                                                     5.9 RNG-free)
                                                 6. deferred items
                                                    (A defer-to-
                                                     M6+ gameplay-
                                                     domain,
                                                     B post-M5
                                                     polish,
                                                     C not-needed-
                                                     for-close)
                                                 7. recommended next
                                                    milestone
                                                    candidates
                                               Closes with the
                                               literal "M5 closes
                                               here."

docs/milestone-5-checkpoint.md                 ANNOTATED as
                                               historical. Top-of-
                                               file note points at
                                               milestone-5-result.md
                                               as the new
                                               authoritative M5
                                               record. Body kept
                                               verbatim for
                                               archaeology (records
                                               the M5.9 in-progress
                                               snapshot).

docs/m5-10-m5-closeout.md                      NEW — this
                                               companion design
                                               note.

README.md / docs/README.md / rfc/README.md     Flipped to
                                                 - Phase: M5
                                                   CLOSED
                                                 - Latest = M5.10
                                                 - milestone-5-
                                                   checkpoint
                                                   annotated as
                                                   historical
                                                 - canonical M5
                                                   record is
                                                   milestone-5-
                                                   result.md
```

What does NOT change:

```text
no code change (zero files under src/ or include/)
no test change (zero files under tests/)
no canonical fixture change (zero files under data/)
no new artefact (still 10)
no save format bump (still v14)
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind / new state field
no new event-module / policy_system / monthly_pipeline
   / save_system / scenario_loader / runner code change
no events.jsonl semantic change
no UI surface change
no balance pass
no M1/M2/M3/M4 systems' external behaviour change
no rebake of M1.17 / M2 / M3 / M4 byte-identical
   determinism baselines
no "M6 in progress" wording — close-out makes no claim
   about which milestone is next (see §3 below)
```

## 2. Why a docs-only PR

Mirrors the M1.17 / M2.22 / M3.9 / M4.23 pattern: every prior
milestone closed with a dedicated docs-only PR that publishes
the exit report, annotates the in-progress checkpoint as
historical, and flips the READMEs. No code or test ships in
the close-out PR itself; all the load-bearing work shipped in
prior sub-milestones (M5.1–M5.9 for this milestone).

This pattern came out of the 2026-05-17 force-reset (see the
M3.9 / M4.23 design notes for full history): the original
attempt to close M3 mid-flight while shipping new gameplay
drifted into premature close-out + invented sub-milestone
numbers + an unintended 9th artefact and had to be reset.
The fix is: **one dedicated final sub-milestone per
milestone, docs only.** M5.10 follows that template.

## 3. Why no "M6 in progress" wording

The 2026-05-17 force-reset lesson cuts both ways: don't
pre-open the next milestone in a close-out PR. The PR #98
reviewer explicitly recommended *"直接 close M5，不要在 M5
繼續加 events.jsonl / cooldown / CLI，避免把事件系統 scope
拉大"* — i.e. close M5 cleanly without inventing M6.

§7 of `milestone-5-result.md` lists candidates for the next
milestone (stop M5 cleanly and move to whatever the reviewer
picks; or post-M5 follow-up polish; or Category A gameplay-
domain items in their own milestones), but M5.10 itself does
not pick. The next milestone starts when the reviewer says
so, in its own deliberate first sub-milestone PR.

## 4. The canonical M5 record from this PR forward

After this PR merges, the canonical reading source for M5 is
`docs/milestone-5-result.md`. The other M5 docs remain in
the tree:

- `docs/m5-1-event-definition-schema-foundation.md` …
  `docs/m5-9-event-observability-checkpoint.md` — the
  per-sub-milestone design notes, kept as historical
  per-PR rationale.
- `docs/milestone-5-checkpoint.md` — the M5.9 in-progress
  snapshot, now annotated as historical (top-of-file note
  added by this PR points at the exit report).
- `docs/m5-10-m5-closeout.md` — this companion note, kept
  in the per-sub-milestone style for symmetry with the M5.1
  – M5.9 design notes.

Future readers wanting "what does the M5 contract look like
right now?" should read `milestone-5-result.md`, not the
checkpoint. Future readers wanting "how did the M5 contract
evolve sub-milestone by sub-milestone?" can read the per-PR
design notes `m5-NN-*.md` alongside the exit report's
§1 ledger.

## 5. What M5.10 explicitly does NOT do

For symmetry with the M5.1–M5.9 design notes:

```text
no new system / formula / artefact (still 10)
no save format bump (still v14)
no new state field
no new RunnerOptions field / new CLI flag
no new PlayerCommandKind
no new event-module / policy_system / monthly_pipeline
   / save_system / scenario_loader / runner code change
no canonical fixture change
no new test / no removed test
no test rename / no test renumbering
no rebake of M1.17 / M2 / M3 / M4 byte-identical
   determinism baselines (no code change = no behavioural
   drift)
no events.jsonl semantic change
no UI surface change
no balance pass
no "M5 closed" annotation on per-sub-milestone notes
   (they each describe M5 *at the moment that PR landed*;
   updating them to past-tense would obscure that)
no claim about which milestone is next
   ("M6 in progress" wording deliberately absent)
no events / matches / fires recorded against the canonical
   scenario that weren't already recorded at M5.9 close
```

**M5 closes here.**
