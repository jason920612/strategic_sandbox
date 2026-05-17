# Post-M3 governance follow-up: `CommandGateDiagnostic` on `RejectionRecord`

Companion notes for `feature/m4-02-rejection-record-gate-diagnostic`.

> **RFC alignment note**
>
> This PR is a post-M3 governance follow-up, not RFC-090
> Milestone 4. RFC-090 Milestone 4 remains SVG map and
> UI. RFC-040 diplomacy / world AI remains future RFC-090
> Milestone 8 work. This PR must not be cited as
> implementing RFC-040.
>
> See `docs/rfc-alignment-note-post-m3.md` for the full
> explanation of why earlier docs (PR #61, this PR's
> first revision) labelled this work as "M4.1" / "M4.2"
> and why that labelling is being corrected here. The
> code work itself is unaffected.

This PR wires the post-M3 `commands::CommandGateDiagnostic`
shape (introduced in the previous governance follow-up PR
that was originally labelled "M4.1") into the M2.20
`commands::RejectionRecord`. The apply-time rejection path
(`apply_pending` / `try_apply_pending`) now carries the
same structured gate explanation that the standalone
`diagnose_*_gate` helpers return for a query. Both
surfaces share one source of truth.

No gate formula change. No new command. No new artefact.
No save schema bump. No diplomacy. No event system. No
AI. No UI / CLI / REPL. No SVG map. No new
`PlayerCommandKind`. Existing flat fields on
`RejectionRecord` (`compliance` / `threshold` /
`resistance`) stay byte-identical for back-compat.

## 1. The gap this PR closes

After the previous governance follow-up the project had
two views of a command gate:

1. **Apply-time** — `dispatch_one` builds a
   `RejectionRecord` with flat fields (`compliance`,
   `threshold`, `resistance`) when the M2.18 / M2.19
   gate rejects.
2. **Standalone** — `commands::diagnose_enact_policy_gate`
   / `diagnose_adjust_budget_gate` return a structured
   `CommandGateDiagnostic` for any country, gate, target.

The two views described the same decision in different
shapes. A caller wiring rejection feedback into UI /
structured logs / future event triggers would either:

- duplicate the diagnostic construction in user code
  (and drift from `order_execution`), or
- re-derive a diagnostic from the flat fields (and
  re-derive the field-selection rule wrong).

This PR has the apply path produce the same
`CommandGateDiagnostic` shape directly, so the two
views share construction via the same helpers.

## 2. Public API change

`commands::RejectionRecord` gains one new field, additive:

```cpp
struct RejectionRecord {
    core::PlayerCommandKind kind{};
    std::string policy_id_code;
    std::string budget_category;
    double      compliance = 0.0;
    double      threshold  = 0.0;
    double      resistance = 0.0;

    // Post-M3 follow-up: structured gate explanation.
    CommandGateDiagnostic gate_diagnostic{};
};
```

The flat fields above stay byte-identical for back-compat
— every existing M2.20 / M2.21 / M2.22 caller that reads
`record.compliance` etc. keeps working. The new field is
an extra view sourced from the same data.

### Header reshape

`CommandGateKind` and `CommandGateDiagnostic`
definitions moved from the bottom of `commands.hpp`
(where the previous governance follow-up first
introduced them) to a block above the M2.20
`RejectionRecord` section, so `RejectionRecord` can
carry a `CommandGateDiagnostic` field directly. Only
the type definitions moved; the `diagnose_*_gate` free
function declarations stay where they were, next to
the rest of the governance-follow-up surface.
Behaviour unchanged.

## 3. Implementation

In `dispatch_one`'s two rejection branches (EnactPolicy
and AdjustBudget) `commands.cpp` now also calls the
matching helper and stores the result on the
`RejectionRecord`:

```cpp
auto diag_r = diagnose_enact_policy_gate(
    state, state.player_country, cmd.policy_id_code);
if (!diag_r) {
    return core::Result<DispatchOutcome>::failure(
        ctx + ": EnactPolicy '" + cmd.policy_id_code +
        "': gate diagnostic failed: " +
        std::move(diag_r.error()));
}
rj.gate_diagnostic = std::move(diag_r).value();
```

`apply_pending` already validates `state.player_country`
at the top so the helper can't realistically fail in
this path — but the diagnostic's `Result` is propagated
verbatim if a future refactor moves the precondition
around.

The `AdjustBudget` branch mirrors the same pattern with
`diagnose_adjust_budget_gate`.

### `format_rejection_message` untouched

The legacy string used by `apply_pending`'s
`Result::failure` return is built by
`format_rejection_message`, which reads the flat fields
only. This PR does not touch it — the M2.18 / M2.19
error-text substring contract is preserved
byte-identical. A test in `commands_test.cpp` (the
post-M3 section near the end) pins that contract.

## 4. Why keep the flat fields

Two-views-of-one-decision is a real cost: a future PR
can diverge `record.compliance` from
`record.gate_diagnostic.authority_value` if it touches
one without the other. This PR accepts that cost in
exchange for preserving every M2.20 / M2.21 / M2.22
caller (and every test reading the flat fields) without
churn.

A `gate_diagnostic-only-no-flat-fields` redesign would
be a separate, larger PR removing public fields —
outside this PR's scope. The agreement test in
`commands_test.cpp` pins that the two views agree by
construction on real rejection cases. A future "remove
redundant flat fields" PR can lean on that test to know
it's safe.

## 5. What is NOT in scope

- no command formula change
- no threshold change
- no new `PlayerCommandKind`
- no new gameplay command
- no new artefact (still 9)
- no save schema bump (still v11)
- no event system / persistent event log / trigger
- no AI / UI / CLI / REPL
- no diplomacy (NOT RFC-040 work)
- no SVG / map / UI (NOT RFC-090 Milestone 4 work)
- no new interest-group channel
- no command-gate integration with interest-group
  aggregates (still deferred to a future
  governance-follow-up PR)
- no removal of the flat `RejectionRecord` fields
- no M3 work
- no RFC roadmap amendment (the alignment note doc is
  observational, not a re-plan)

## 6. Test surface

6 new doctest cases in `commands_test.cpp` post-M3
section:

- EnactPolicy rejection: flat fields populated AND
  `gate_diagnostic` populated; both views agree on
  `compliance == authority_value`, `threshold`,
  `allowed == false`, `target == "policy:..."`,
  `authority_field == "bureaucratic_compliance"`.
- Military AdjustBudget rejection: `gate_diagnostic`
  uses `military_loyalty` per the M2.19 routing rule.
- Non-military AdjustBudget rejection:
  `gate_diagnostic` uses `bureaucratic_compliance`
  (even when `military_loyalty` would have accepted).
- Boundary `0.299` reject: `gate_diagnostic.allowed`
  is `false`.
- `format_rejection_message` legacy string preserved
  byte-identical (substrings `order_execution`,
  `rejected`, `bureaucratic_compliance=`,
  `threshold=` all still present).
- The diagnostic on a rejection matches what
  `diagnose_enact_policy_gate` returns for the same
  state + target standalone — proves the two views
  share one source of truth.

Total doctest count: 794 → 800 (+6).

## 7. Future post-M3 governance follow-up candidates

These remain candidates for further post-M3 work
within the same governance scope. None committed; the
M-pacing rule still requires a reviewer-supplied spec
before any of them ship. **None of them count as
RFC-090 Milestone 4** (which remains SVG map and UI)
or as RFC-040 work (which remains future RFC-090
Milestone 8 work).

- Plumb a `RejectionRecord` (or its
  `gate_diagnostic`) through to `RunOutcome` so a CLI
  run prints why a command was rejected, not just the
  count.
- A 10th unconditional CSV artefact —
  `command_rejections.csv` — recording every gate
  rejection across a run with the diagnostic shape as
  the row.
- Remove the redundant flat fields on
  `RejectionRecord` once enough callers have migrated
  to `gate_diagnostic`. Behaviour-preserving cleanup,
  no formula change.
- Interest-group aggregates as additional gate inputs
  (the long-deferred M3 → M2 integration).
