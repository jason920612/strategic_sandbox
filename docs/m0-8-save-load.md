# M0.8 - Save / Load design notes

Companion notes for `feature/m0-08-save-load`. Locks in the on-disk
schema, the version-gate policy, and the explicit non-goals
(specifically: M0.8 is *session resume*, not *deterministic replay*).

## 1. What the save file holds

```jsonc
{
  "save_version": 1,              // bumps on incompatible schema change
  "rng_algorithm_version": 1,     // bumps if the RNG mixer changes
  "current_date": "1930-01-06",
  "rng": {
    "seed":    19300101,
    "counter": 0
  },
  "countries": [
    {
      "id": 0,                    // numeric handle (the caller's choice)
      "id_code": "GER",           // on-disk stable identifier
      "name": "Germany",
      "display_name": "Germany",
      "initial_gdp": 100.0,
      "initial_stability": 0.55
    }
  ],
  "provinces": [],                // reserved - schema arrives in M1
  "factions":  [],                // reserved
  "policies":  [],                // reserved
  "events":    [],                // reserved
  "logs": [
    {
      "date": "1930-01-01",
      "category": "lifecycle",
      "severity": "info",
      "source": "main",
      "message": "simulation start",
      "metadata": {}
    }
  ]
}
```

Notes:

- **Pretty-printed.** Indented two spaces. Survives a diff against an
  earlier save cleanly, which is the only debugging tool we have until
  replay infrastructure lands. Cost is ~2x bytes, which we do not care
  about at the file sizes a strategic sim produces.
- **Insertion-order preserved** for every JSON object — the loader and
  the saver both use `nlohmann::ordered_json`. This is the same
  invariant the M0.6 JSONL exporter pins for log metadata.
- **Reserved keys for future entity tables.** `provinces`, `factions`,
  `policies`, `events` are written as empty arrays today and tolerated
  as absent or empty by the loader. When M1 fleshes out their shapes,
  we can populate the arrays without bumping `save_version`.

## 2. Version gates

| Field | Today's value | When to bump |
|---|---|---|
| `save_version` | 1 | Whenever the on-disk schema changes in a way an older binary cannot parse (renamed keys, removed fields, new required field, ...) |
| `rng_algorithm_version` | 1 | Whenever the splitmix64-style mixer in `random_service.cpp` changes. Identical `(seed, counter)` after such a change would produce different draws, so old saves must fail loudly. |

The loader rejects unknown versions with messages like:

```
my-save.json: unsupported save_version 2 (this binary supports 1)
my-save.json: unsupported rng_algorithm_version 99 (this binary supports 1)
```

When we add a v2 in the future, the loader can grow a small migration
path (accept v1, upgrade in-memory, write back v2). M0.8 doesn't
attempt this — strict-equality version gating is the simplest correct
behaviour.

## 3. Error-message format

Same `<source>: <reason>` style as the M0.7 DataLoader:

```
my-save.json: missing required field 'save_version'
my-save.json: 'simulation.start_date' = "1930-02-30" is not a real Gregorian date
my-save.json: countries[3]: 'initial_gdp' has wrong type (expected number)
my-save.json: logs[7]: 'severity': unknown severity 'panic' (expected debug|info|warn|error)
my-save.json: cannot open file for reading
```

Array indices appear in errors so a human can locate the offending
element in the file. The source label propagates through the
sub-helpers via a contextualised string (`"<source>: logs[7]"`) so we
do not have to do brittle substring math.

## 4. M0.8 is session resume, NOT deterministic replay

A "save" captures **just enough state** to resume the game from where
it left off:

- the current date,
- the RNG state (so future draws continue the same stream),
- entity tables,
- the historical log.

That is enough to pause + close + reopen + continue. It is **not**
enough to *replay* a session from start. Real replay needs:

- the per-tick decision log (player commands, AI choices, event
  outcomes) — not yet recorded,
- the system pipeline being re-run from `start_date` rather than
  resumed from `current_date`,
- a way to verify the replay matches by hashing intermediate states.

None of that exists yet. M0.8 only ships the resume path. The PR
description has to state this explicitly so reviewers don't expect
something they aren't getting.

The RNG `counter` *is* preserved by the save, so the foundation for
replay is in place: when replay support lands, we'll know exactly
where in the random stream a given saved session was.

## 5. What's NOT in scope for M0.8

- **Schema migration.** No v2 reader-of-v1 yet. When the first format
  break happens, we'll decide between strict failure and migration.
- **Compression / binary format.** JSON files for a 70-year run are
  estimated at < 100 MB. We can revisit if a profile shows it matters.
- **Concurrent save / load.** Single-threaded only.
- **Crash safety.** No atomic rename-into-place yet; a crash mid-write
  produces a truncated file. Easy to add via a tmp-file-and-rename
  helper later if the runner ever runs unattended.
- **Hash / checksum.** Save integrity is "the JSON parses and the
  version gates accept it". A cryptographic hash isn't needed for
  user-visible saves; it might matter for replay golden files later.
- **Diff-friendly serialisation of doubles.** We rely on nlohmann's
  default double output (~16-17 sig figs). If we ever need *exact*
  round-trip across platforms, we'll switch to `hexfloat` or a fixed
  scaled-integer format. M0 has no such requirement.
