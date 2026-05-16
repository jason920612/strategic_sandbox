# M1.3 - Budget and economy baseline

Companion notes for `feature/m1-03-budget-economy-baseline`. Third M1
data-model PR. Adds the per-country budget allocation as a nested
`BudgetState` on `CountryState`. **No economy tick, no budget
application, no GDP formula.** That's M1.5+ territory.

## 1. Field set

```cpp
struct BudgetState {
    double administration  = 0.0;
    double military        = 0.0;
    double education       = 0.0;
    double welfare         = 0.0;
    double intelligence    = 0.0;
    double infrastructure  = 0.0;
    double industry        = 0.0;
};

struct CountryState {
    // ...existing M1.1 fields...
    BudgetState budget;            // M1.3 (embedded, not a pointer)
};
```

Seven categories straight from RFC-010 §2.4 and RFC-060 §3. Each is
a fraction of state spending, in `[0, 1]`.

## 2. Why no "sum to 1" enforcement?

The DataLoader does NOT check that `administration + military + ... + industry == 1.0`.
Reasons:

- **An under-allocated budget is meaningful**: it represents a state
  that holds back a surplus. A future economy system will read the
  shortfall as "uncommitted spending".
- **An over-allocated budget is also meaningful**: deficit spending,
  to be reconciled against `budget_balance` by the economy tick.
- **Enforcing sum at load time would force authors to fudge values**
  to hit exactly 1.0 every time they tweak a category. This is the
  same reason we don't enforce ratio range invariants on save data:
  validation lives where it's actionable.

A future `Diagnostics::sanity_check` rule can flag "extreme deviation
from 1.0" once we have a real economy consumer to define what
"extreme" means. Until then, the only per-category constraint is
`[0, 1]`.

## 3. JSON schema (DataLoader)

```jsonc
"budget": {                        // required JSON object
  "administration":  0.25,         // every category required, [0, 1]
  "military":        0.35,
  "education":       0.10,
  "welfare":         0.10,
  "intelligence":    0.05,
  "infrastructure":  0.10,
  "industry":        0.05
}
```

The "budget" key itself is required (no defaulting to all-zero), and
every category inside it is required. Error messages place the budget
context in the path:

```
country.json: missing required field 'budget'
country.json: 'budget' has wrong type (expected JSON object)
country.json: budget: missing required field 'military'
country.json: budget: 'administration' must be in [0, 1] (got 1.500000)
country.json: budget: 'welfare' must be in [0, 1] (got -0.100000)
```

The "missing required field 'military'" carries the `: budget` prefix
in its source label, so it never collides with the top-level
`military_power` field. Tests pin both shapes.

## 4. Save format change: v3 → v4

`kSaveFormatVersion: 3 → 4`. v3 saves now fail loudly:

```
save.json: unsupported save_version 3 (this binary supports 4)
```

### The bump rule from M1.2 applies again

M1.2's design notes established: "every PR that adds new content to a
previously-absent payload in saves bumps `kSaveFormatVersion`." M1.3
adds a budget block to every country in the saved state; v3 readers
would either (a) silently leave the budget zeroed or (b) crash on
the unknown sub-object depending on implementation. Strict-equality
gating eliminates the ambiguity.

The version-history comment in `save_system.hpp` is updated, and the
new regression test `"deserialize: an old v3 save is rejected
loudly"` pins the behaviour. The previous M1.1 and M1.2 rejection
tests (v1, v2) still pass — they just expect `supports 4` now.

### SaveSystem range validation

Same policy as M1.1 / M1.2: SaveSystem **only type-checks** on load.
Range enforcement is DataLoader's job (untrusted human-authored
input). A future Diagnostics rule could check budget sums once we
have economy code that depends on it.

## 5. Drive-by from PR #13 review

`faction_from_json` was using `FactionId::underlying_type` to
range-check the `country` field. Both types are `int` today so the
check produced the same result, but semantically it should use
`CountryId::underlying_type` because that's the destination type.

The fix is a one-character rename plus splitting the constant into
`kFacMaxId` and `kCtyMaxId`, with a comment pointing at the review
that flagged it. Latent bug if the underlying types ever diverge.

## 6. Fixtures

Every country JSON gains a `budget` block:

| Country | admin | military | education | welfare | intel | infra | industry | sum |
|---|---|---|---|---|---|---|---|---|
| GER | 0.25 | 0.35 | 0.10 | 0.10 | 0.05 | 0.10 | 0.05 | 1.00 |
| FRA | 0.25 | 0.30 | 0.15 | 0.15 | 0.04 | 0.06 | 0.05 | 1.00 |
| JPN | 0.20 | 0.40 | 0.10 | 0.05 | 0.08 | 0.07 | 0.10 | 1.00 |

All three sum to 1.0 for clarity, but the loader doesn't enforce
that — see §2.

## 7. What M1.3 deliberately does NOT do

- **No economy tick / monthly budget application.** That's M1.13–1.15.
- **No `tax_revenue` derivation from budget × GDP.** Tax revenue is
  still a runtime-only field that starts at 0.
- **No `budget_balance` adjustment from spending.** Same as above.
- **No faction reaction to budget changes.** M1.5+.
- **No `Diagnostics::sanity_check` rule for budget sums.** When the
  economy tick lands and there's a consumer, we can add one.
- **No JSON file structure change for budget**. It's nested inside
  the country JSON, not a separate `data/budgets/*.json` collection.
  Country and its budget are tightly coupled — splitting them would
  invite drift.

## 8. Migration impact

| File | Δ |
|---|---|
| `include/leviathan/core/entities.hpp` | +`BudgetState`, +`budget` member |
| `include/leviathan/systems/data_loader.hpp` | schema doc updated |
| `src/leviathan/systems/data_loader.cpp` | +`budget` parsing block in `parse_country` |
| `include/leviathan/systems/save_system.hpp` | `kSaveFormatVersion` 3 → 4, version-history comment |
| `src/leviathan/systems/save_system.cpp` | `country_to_json` and `country_from_json` extended; drive-by `CountryId::underlying_type` fix in `faction_from_json` |
| `data/countries/*.json` | +`budget` block in all three |
| `tests/systems/data_loader_test.cpp` | +6 budget-specific cases; existing happy path checks budget |
| `tests/systems/save_system_test.cpp` | +3 cases (v3-rejected, budget round-trip via file, missing-budget in save); all inline `save_version: 3` literals swept to 4; `build_seeded_state` populates budgets |
| `tests/integration/m0_end_to_end_test.cpp` | unchanged (consumes the updated germany.json transparently) |

Total tests: **205 → 214** (+9).

## 9. Risks / things to watch

- **Save format v4 is now load-bearing.** Same rule as v3 from M1.2:
  any future change to country / budget shape must either extend
  forward-compatibly or bump to v5.
- **DataLoader / SaveSystem are accumulating JSON-validation
  duplication.** `require_string` / `require_number` / `require_ratio`
  / `require_u64` live in `data_loader.cpp`; near-identical helpers
  for the save schema live in `save_system.cpp`. The
  `country_from_json` `load_num` lambda is now triplicated (country,
  faction, country.budget). M1.4 (PolicyData loader) will be the
  third real consumer; extracting `src/.../json_helpers.hpp` is a
  natural follow-up PR.
- **`budget` is the first nested object inside a top-level entity.**
  The parser uses a contextualised source label
  (`<src>: budget: <reason>`) so error messages disambiguate
  "`budget.military`" from "top-level `military_power`". The shape
  is pinned by tests; reviewers should reject any PR that loses
  this prefix.

## 10. Next sub-milestone

**M1.4 — PolicyData schema.** Per the user's M1 plan: policy data
format and ~10 policy fixtures, **no effect application**. Same
template — shape + loader + save (likely v4 → v5) + tests + docs.
M1.5 is where the first real effect lands.
