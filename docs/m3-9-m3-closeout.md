# M3.9 - M3 close-out

Backfilled per-sub-milestone design note for M3.9. The
canonical M3 exit ledger lives in
[`milestone-3-result.md`](milestone-3-result.md); this file
covers the M3.9-specific deliverable in isolation so the
per-sub-milestone naming convention is unbroken across M3.

## 1. Scope

M3.9 is the **exit / close-out sub-milestone** for M3.
Mirrors M1.17 / M2.22's role: no new gameplay system, no
new formula, no new artefact, no save schema bump — only
the exit-doc deliverables that flip M3 from "in progress"
to "closed".

What shipped:

```text
docs/milestone-3-result.md                  new M3 exit report
docs/milestone-3-checkpoint.md              annotated "historical"
README.md / docs/README.md / rfc/README.md  flipped to "M3 closed"
                                            next milestone TBD
```

The exit report covers seven sections per the M3.9 spec:

1. **What M3 shipped** — table of M3.1–M3.9 with one-line
   highlights each.
2. **Final M3 dataflow** — country.stability → M3.2 react
   → group state → M3.3 country_feedback → country.stability;
   Bureaucracy loyalty → M3.4 authority_pressure →
   bureaucratic_compliance; state / outcomes → M3.5 /
   M3.6 CSVs; canonical fixtures → M3.8.
3. **Final artefact contract** — eight files, with the
   sequential-not-transactional `end_tick` caveat preserved
   from M3.6 and the pre-`end_tick` no-artefact contract
   from M2.9 still in force.
4. **Save schema** — remains v11 after M3 (M3.1 was the
   only M3 schema bump; M3.2–M3.8 were schema-neutral).
5. **Architectural invariants every future milestone must
   preserve** — M3 systems deterministic + RNG-free; no logs
   / events from interest groups; rate ladder 0.05 → 0.02
   → 0.01 load-bearing; canonical `tick_all_countries`
   order; M2 command gates byte-identical with M2.22;
   `bureaucratic_compliance` is a downstream input to M2
   gates via M3.4; canonical scenarios author minimal
   Bureaucracy groups (M3.8); 8-artefact set; v11 save
   floor.
6. **Deferred items** — Military / Intelligence / Media
   pressure channels; richer political maps; policy
   preferences; event triggers from thresholds; strike /
   protest / coup / civil-war; cross-border influence;
   per-kind formulas; command-gate diagnostic / UI surface;
   atomic `end_tick` writes; etc.
7. **Recommended next milestone candidates** — neutral list:
   RFC-090 M4 (SVG map + UI), RFC-090 M5 (event engine),
   non-RFC-numbered post-M3 governance follow-up. M3.9
   does NOT open or claim any candidate.

## 2. Why this is its own sub-milestone

M3.7 was the integration checkpoint; M3.8 was the canonical
fixture; M3.9 is the dedicated close-out PR that publishes
the exit report and flips the wording everywhere. Splitting
close-out from M3.8 keeps each PR scoped to one concern
(matches the per-PR-pacing rule used throughout M1 / M2 / M3)
and keeps the exit doc reviewable separately from any
code or fixture change.

The 2026-05-17 force-reset history is the recorded reason
this care is warranted (see
[`milestone-3-result.md`](milestone-3-result.md) §7): a
previous attempt at M3 close-out bundled premature exit +
invented M4.X numbers + a 9th artefact and was rolled back.
The current M3.9 is tightly scoped to doc-only.

## 3. What M3.9 does NOT do

```text
no new system
no new formula
no new artefact (still 8)
no save schema bump (still v11)
no new state field
no new InterestGroupKind
no new fixture (canonical M3 fixtures are M3.8's)
no new test (M3.7 / M3.8 cover the loop / 8-artefact /
              canonical-data-row paths)
no PlayerCommandKind
no event
no log from interest groups
no AI / UI / REPL / CLI surface
no command-gate formula change
no command-gate diagnostic surface
no runner CLI flag
no atomic end_tick writes
no M4
no post-M3 governance follow-up wording
no claim about which milestone is next
```

745 doctest cases unchanged.

## 4. Cross-references

- [`milestone-3-result.md`](milestone-3-result.md) — canonical
  M3 exit report (the deliverable M3.9 publishes).
- [`milestone-3-checkpoint.md`](milestone-3-checkpoint.md) —
  M3 status snapshot at the M3.7 moment, kept for archaeology
  with a "historical" annotation added by M3.9.
- [`m3-7-reaction-loop-integration-checkpoint.md`](m3-7-reaction-loop-integration-checkpoint.md)
  — the integration tests M3.9 inherits without modification.
- [`m3-8-canonical-interest-group-fixtures.md`](m3-8-canonical-interest-group-fixtures.md)
  — the canonical fixtures M3.9 inherits without modification.
