# RFC alignment note — post-M3 milestone drift

This note documents a divergence between the implementation
milestones shipped on `main` and the milestone numbering
defined in `rfc/RFC-090-roadmap.md`. It is **observational**,
not a re-plan of the roadmap. No RFC text is amended here.

The intent of this document is to **freeze the drift**:
future PRs must either follow RFC-090 numbering or first land
an explicit RFC roadmap amendment PR (separate from any
implementation work).

## 1. What RFC-090 says

`rfc/RFC-090-roadmap.md` lists twelve milestones. The
relevant ones for this note:

| RFC-090 milestone | Title | Key tasks |
|---|---|---|
| M0 | 技術骨架 | C++ skeleton, GameDate, RNG, GameState, JSON loader, logging, save, CSV, headless mode |
| M1 | 單國內政原型 | CountryState / FactionState / BudgetState / PolicyData, policy effects, faction support, stability, GDP / tax / budget monthly, monthly stats output |
| M2 | 玩家操作原型 | Player country selection, pause/resume, speed, budget command, policy command, command log, error messages, playback log |
| **M3** | **多國模擬** | **CountryData schema, 10 / 20–30 country data, multi-country GameState, AI policy selection, relationship value, threat value, simple military strength, annual world stats, 1930–2000 auto test** |
| **M4** | **SVG 地圖與 UI** | **ProvinceNode, province JSON, SVG exporter, HTML viewer, map + country panel, map + event log, clickable provinces** |
| M5 | 事件引擎 | EventData, TriggerCondition, WeightModifier, EventOption, EventEffect, event chain |
| M6 | 隱藏真相與資訊失真 | true_cause, visible_report, information_accuracy, bias / noise, intelligence budget impact |
| **M7** | **派系深化** | **Faction demands, radicalism events, influence weights, faction conflict, military pressure, media scandal, intelligence expansion, local tax revolts, student protest, technical-elite emigration** |
| M8 | 外交與世界 AI | Relationship change, threat, alliances, sanctions, declaration of war, peace, AI utility, misperception, 70-year diplomacy test |
| M9 | 自動戰爭 | WarGoal, WarState, Front, military combat, province control, war weariness |
| M10 | 崩潰、政變與內戰 | Coup risk, civil war, faction defection, exile / prison states |
| M11 | 經濟擴展 | Unemployment, debt, inflation, resources, industry, trade, black market |
| M12 | 內容擴充 | Country data expansion, factions templates, 100 events, 50 policies, replays |

(Note: `rfc/RFC-001-development-contract.md` §2.1 lists
a different, shorter milestone numbering that predates
RFC-090. The two documents are themselves inconsistent.
RFC-090 is the more detailed and more recent roadmap and
should be treated as authoritative for milestone
numbering until an explicit RFC roadmap amendment lands.)

## 2. What the implementation actually shipped

`main` currently has these milestone exit reports:

- `docs/milestone-0-result.md` — M0 close-out.
- `docs/milestone-1-result.md` — M1 close-out.
- `docs/milestone-2-result.md` — M2 close-out (includes
  M2.16 GovernmentAuthorityState, M2.17–M2.20
  OrderExecution + rejection reporting, M2.21 script
  driver, M2.22 exit tests — these are M2 extensions
  that exceed RFC-090 M2's nominal §2.1–2.8 scope).
- `docs/milestone-3-result.md` — "M3" close-out. **Ships
  interest-group reaction layer** (InterestGroupState,
  three reaction systems with rate ladder
  `0.05 → 0.02 → 0.01 → 0.01`, sibling Military
  channel, four-CSV observability surface).

And on the current feature branch, two more sub-milestones
are labelled M4.X (the previous governance follow-up that
was labelled "M4.1" command gate diagnostics, and this PR
originally labelled "M4.2" `CommandGateDiagnostic` on
`RejectionRecord`).

## 3. Mapping implementation to RFC-090

| Implementation label | What it shipped | Closest RFC-090 milestone |
|---|---|---|
| M0 (closed) | Tech skeleton | **M0 (matches)** |
| M1 (closed) | Single-country internal politics | **M1 (matches)** |
| M2 (closed) | Player command prototype + extensions (M2.16–M2.22) | M2 plus M2-shaped extensions; **broadly aligned** |
| **M3 (closed)** | **Interest-group reaction layer (10 InterestGroupKind variants, three reaction systems, authority pressure, four observability CSVs)** | **Closer to RFC-090 M7 派系深化 than RFC-090 M3 多國模擬.** RFC-090 M3 is multi-country simulation (20–30 country data, AI policy selection, relationship/threat values) — none of which is in the shipped M3. |
| **"M4.1" (merged, PR #61)** | `commands::CommandGateDiagnostic` helper + `diagnose_*_gate` free functions | **No RFC-090 milestone matches.** This is a post-M2 governance follow-up. RFC-090 M4 is SVG map and UI; M8 is diplomacy / world AI. |
| **"M4.2" (this PR, before alignment fix)** | `CommandGateDiagnostic` field on `RejectionRecord` | **No RFC-090 milestone matches.** Same as above. |

## 4. Why this matters

- **RFC-090 is the contract for what the project is
  building**, and the milestone numbers are how PR review
  + tests + docs refer to that contract. When the
  implementation invents its own M3 / M4 numbers without
  reflecting RFC-090, the contract erodes.
- **Outside readers** (future contributors, automated
  agents, the project author themselves after a long
  break) cannot tell what's in scope or out of scope from
  the milestone label alone — they have to read the
  implementation history to find out.
- **RFC-040 (diplomacy / world AI)** is RFC-090 Milestone
  8 work, not M4 work. Citing "M4" as a diplomacy
  milestone in any future doc or PR description would
  mis-cite RFC-040.

## 5. What this means going forward

The drift is **frozen** as of this PR. No retroactive
renaming of M0 / M1 / M2 / M3 exit reports on main:

- `docs/milestone-0-result.md`, `-1-result.md`,
  `-2-result.md`, `-3-result.md` stay as they are. They
  remain valid records of what shipped under those
  labels.
- The previous governance follow-up that was labelled
  "M4.1" stays merged with that name on main (commit
  `1958083`). Removing the label retroactively would
  require a separate cleanup PR.
- This PR (originally labelled "M4.2") drops the M4
  labelling: doc renamed to
  `docs/post-m3-command-gate-diagnostic-on-rejection.md`,
  READMEs reframed.

For future work:

- **Default**: follow RFC-090 numbering. RFC-090 M4 is
  SVG map + UI; M5 is event engine; M7 is faction
  deepening; M8 is diplomacy / world AI; etc. A PR doing
  any of those should use the corresponding RFC-090
  number, not invent a new one.
- **Otherwise**: a PR may use a non-RFC-090 label
  (`post-m3-governance-followup-*`, etc.) but the
  design note must explicitly disclaim the milestone
  mapping (as this PR's renamed design note does), and
  the three READMEs must not present the work as
  RFC-090 M4 / M5 / M7 / M8 work.
- **If the project wants to renumber the implementation
  milestones to match RFC-090**, that is an explicit
  RFC roadmap amendment PR. It must:
  1. amend `rfc/RFC-090-roadmap.md` (and / or
     `rfc/RFC-001-development-contract.md` §2.1 to
     resolve the cross-document inconsistency), AND
  2. rename `docs/milestone-3-result.md` (and any
     others) to reflect the new mapping, AND
  3. not bundle implementation code changes.

No such amendment is proposed by this PR.

### Hard rule for future PRs

A PR title, branch name, design note filename, README
status line, or `rfc/README.md` progress entry must
**not** use an RFC-090 milestone number unless the work
directly implements that RFC-090 milestone's defined
scope, **or** the PR is an explicit RFC roadmap
amendment.

This rule is binding. The reviewer flagged the
governance issue during PR #62 review on 2026-05-17 and
made clear that if RFC-090 milestone numbers can be
reused loosely, the RFCs themselves become meaningless.
The drift inherited from PR #50 / PR #54 / PR #61 is
frozen but **not extended**. Work that wants to use
"M4.X" must be SVG / map / UI work per RFC-090 §M4.
Work that wants to use "M5.X" must be event-engine work
per RFC-090 §M5. And so on. Otherwise the PR uses a
`post-m{N}-{slug}` label and the design note disclaims
the mapping at the top, the way this PR's renamed
design note (`docs/post-m3-command-gate-diagnostic-on-rejection.md`)
does.

## 6. Strict non-claims this note makes

This note explicitly does **not**:

- amend any RFC text,
- retroactively rename any merged milestone,
- close M3 again (M3 stayed closed at commit
  `3c1096a`),
- open RFC-090 M4 (SVG map and UI — still future
  work),
- open RFC-090 M8 (diplomacy / world AI — still
  future work),
- introduce a new gameplay system,
- change any save schema,
- change any artefact set.
