# M3.7 - M3 reaction-loop integration checkpoint

Backfilled per-sub-milestone design note for M3.7. The
checkpoint-time snapshot of M3 dataflow / artefacts /
invariants / deferred items lives in
[`milestone-3-checkpoint.md`](milestone-3-checkpoint.md)
(now annotated as historical now that M3 has closed); the
canonical M3 exit ledger lives in
[`milestone-3-result.md`](milestone-3-result.md). This file
covers the M3.7-specific deliverable in isolation so the
per-sub-milestone naming convention is unbroken across M3.

## 1. Scope

M3.7 is a **checkpoint, not an exit.** It does NOT close
M3 (M3.9 does that). It does NOT add a new gameplay system,
formula, artefact, save schema bump, or fixture. It pins the
M3.1–M3.6 reaction loop at the seam between M3 and any
future milestone via three new integration tests plus a
short status-snapshot doc.

What shipped:

```text
tests/integration/m3_end_to_end_test.cpp    3 new doctest cases
docs/milestone-3-checkpoint.md              status snapshot
README.md / docs/README.md / rfc/README.md  "M3.7 checkpoint shipped"
                                            (M3 stays "in progress")
```

The three integration cases:

1. **One-month `monthly::tick_all_countries` fires every M3
   leg in one call.** Hand-built one-country / one-Bureaucracy-
   group state goes through `tick_all_countries` once.
   Asserts every reverse-direction counter ticked
   (`interest_groups_updated` / `interest_group_countries_updated`
   / `interest_group_authority_countries_updated`), every
   mutable field actually changed (group loyalty / radicalism,
   country stability, country bureaucratic_compliance), and
   each trace vector got one row whose post-mutation value
   matches the live state field.

2. **`runner::run_state` emits all 8 artefacts with actual M3
   data rows.** Same hand-built state through `run_state` for
   31 days (crosses one month boundary). All eight files
   exist on disk; the three M3 files contain data rows (not
   header-only); `RunOutcome` counters report the expected
   non-zero row counts. Covers the data-row path that M1.17 /
   M2.22 canonical-scenario runs leave header-only because
   canonical scenarios authored zero interest groups
   pre-M3.8.

3. **Byte-identical determinism on the M3-mutation path.**
   Two byte-for-byte identical hand-built states through
   `run_state` into independent temp dirs produce
   byte-identical 8 artefacts. Extends the M1.17 / M2.22
   determinism contract to the M3 mutation path, not just
   the canonical-scenario header-only path.

## 2. Why this is a checkpoint, not an exit

The reviewer's M3.7 spec was explicit: pin the loop at the
seam between M3 and any future milestone, but **do not close
M3.** M3 had more sub-milestones to ship (M3.8 canonical
fixtures, M3.9 close-out) before the exit report could be
written honestly.

The 2026-05-17 force-reset history is the recorded reason
(see [`milestone-3-result.md`](milestone-3-result.md) §7):
a previous attempt at M3.7+ jumped straight to M3 close-out
+ M4.X invented numbers and was rolled back; the current
M3.7 (PR #63) is the tightly-scoped redo, and M3.9 (PR #65)
is the proper close-out that comes after M3.8.

## 3. What M3.7 does NOT do

```text
no new system
no new formula
no new artefact (still 8)
no save schema bump (still v11 at this point)
no new state field
no new InterestGroupKind
no new PlayerCommandKind
no events
no logs from interest groups
no AI / UI / REPL / CLI surface
no command-gate formula change
no command-gate diagnostic surface
no runner CLI flag
no atomic end_tick writes
no M3 close-out
no docs/milestone-3-result.md (M3.9 writes that)
no "M3 closed" wording
no M4
no post-M3 governance follow-up
```

## 4. Cross-references

- [`milestone-3-checkpoint.md`](milestone-3-checkpoint.md) —
  M3 status snapshot at the M3.7 moment (now annotated
  historical; M3 has closed).
- [`milestone-3-result.md`](milestone-3-result.md) — canonical
  M3 exit report (M3.1–M3.9 ledger, final dataflow, eight-
  artefact contract, M3+ invariants, deferred items, neutral
  next-milestone candidates).
- [`m3-6-interest-group-feedback-trace-csv.md`](m3-6-interest-group-feedback-trace-csv.md)
  — the trace surface the first integration case exercises.
- [`m3-8-canonical-interest-group-fixtures.md`](m3-8-canonical-interest-group-fixtures.md)
  — added one Bureaucracy interest group per canonical
  country so canonical-scenario runs also exercise the
  data-row path (M3.7 only covered the hand-built path).
