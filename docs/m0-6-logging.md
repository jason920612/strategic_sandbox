# M0.6 - LoggingSystem design notes

Companion notes for `feature/m0-06-logging`. Locks in the JSONL format,
the "no side-effect logging" rule, and the deterministic-metadata
ordering invariant that downstream tools (M0.10 diagnostics, M0.8
save round-trips) will rely on.

## 1. The shape of a LogEntry

```cpp
struct LogEntry {
    GameDate    date;
    std::string category;
    LogSeverity severity;     // Debug | Info | Warn | Error
    std::string source;
    std::string message;
    LogMetadata metadata;     // vector<pair<string, string>>
};
```

Field-by-field:

- **`date`**: stamped from `state.current_date` at the moment `log()`
  is called. There is no "post-dated" or "scheduled" log path.
- **`category`**: free-form bucket. Conventional values: `"time"`,
  `"economy"`, `"lifecycle"`, `"policy"`, `"event"`. Treat it as
  a coarse filter knob, not a structured tag system.
- **`severity`**: `enum class`, four levels. `Debug` is for fine-grained
  per-tick tracing; `Error` is for "something invariant-breaking just
  happened". Default in convenience APIs is `Info`.
- **`source`**: who emitted this log. Conventional values: the system
  name (`"TimeSystem"`, `"PolicySystem"`) or `"main"` / `"DataLoader"`.
- **`message`**: human-readable description.
- **`metadata`**: ordered string-keyed string values. Order is
  **deliberately** preserved, because JSONL output keys are emitted in
  insertion order and we want bit-stable output across runs.

Why `vector<pair<>>` instead of `unordered_map`? Because
`unordered_map` iteration order is implementation-defined and would
make JSONL output non-deterministic across compilers. The cost of
losing O(1) key lookup is fine: metadata is small (typically < 5
entries) and we only ever iterate it, never look up by key.

## 2. The "logging is always explicit" rule

`leviathan::systems::logging::log()` is the **only** function in the
project that may push to `state.logs`. No other system writes to the
log container as a side effect of doing its real job.

Enforcement:

- **TimeSystem** has a regression test (extended in this PR) that
  advances 100 days and asserts `state.logs.empty()` afterwards.
- **RandomService** likewise never touches `state.logs`. Reviewers
  should reject any PR that adds a side-effect log inside a system's
  hot path.
- If a system *wants* to be loggable, the caller adds the log on its
  own line, e.g.:

  ```cpp
  const auto r = lt::advance_one_day(state);
  if (r.month_changed) {
      lg::log_info(state, "time", "TimeSystem", "Month rolled over");
  }
  ```

This keeps logging policy in the calling code, where it belongs.
TimeSystem doesn't decide whether to log; the caller does.

## 3. JSONL format - byte stable

One entry, one line, terminated by `\n`. Field order is fixed:

```json
{"date":"YYYY-MM-DD","category":"...","severity":"...","source":"...","message":"...","metadata":{"k":"v"}}
```

- All values are JSON strings (severity is rendered as one of `"debug"`,
  `"info"`, `"warn"`, `"error"`).
- `metadata` is a JSON object whose keys are emitted in insertion
  order, not sorted alphabetically. Two runs of the same simulation
  with the same seed produce byte-identical JSONL.
- Escapes: `"` → `\"`, `\` → `\\`, `\b \f \n \r \t` use the named
  escapes; other C0 controls (0x00–0x1F) become `\u00XX`. Non-ASCII
  bytes pass through unchanged (we assume UTF-8 input).

Snapshot test pins this exactly:

```cpp
const std::string expected =
    "{\"date\":\"1930-01-01\","
    "\"category\":\"time\","
    "\"severity\":\"info\","
    "\"source\":\"TimeSystem\","
    "\"message\":\"Advanced to next day\","
    "\"metadata\":{}}\n";
```

If you need to change this format, expect to break replay diff tools
that downstream PRs will build. Discuss before doing so.

## 4. Why no third-party JSON library?

For LogEntry serialisation we control both ends: a small finite set of
fields, all strings or known enums, no nesting beyond the metadata
object. Hand-rolling the escape and emit is a few dozen lines and
avoids:

- Adding another FetchContent dependency.
- Risk that the library re-orders keys.
- Risk that a future major-version bump changes escape rules.

M0.7 (DataLoader) **will** introduce a JSON parser as a third-party
dep. But parsing is much harder than emitting; logging exporter
remains self-contained.

## 5. recent(N) semantics

`recent(state, n)` returns up to the last `n` entries in insertion
order (oldest first within the returned slice). It copies the entries,
because handing out a non-owning view of a `std::vector<LogEntry>`
that the caller could invalidate on the next `log()` call is a
footgun.

If `state.logs.size() < n`, the function returns all entries. This is
intentional: the natural use case is "show me the last 10 logs" and we
don't want callers to special-case the early-game empty-log situation.

## 6. What's still NOT in scope

- **In-place filtering by severity / category.** Easy to bolt on later
  (`recent_with_severity_at_least(...)`) but no caller needs it yet.
- **Real-time emit to disk or stdout.** Logs accumulate in
  `state.logs`; M0.9 (headless runner) decides when to dump them.
  Always-on stdout logging would slow tests and make output
  comparisons painful.
- **Log rotation / truncation.** A 70-year sim at 1 log/day is ~26k
  entries — well within memory. If we ever ship a multi-decade run
  with per-second logging, we'll revisit.
- **Multi-thread safety.** Same caveat as the rest of the simulation:
  one `GameState` per thread.
