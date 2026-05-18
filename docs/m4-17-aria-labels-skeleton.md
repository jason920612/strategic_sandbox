# M4.17 - ARIA labels skeleton

Companion notes for `feature/rfc090-m4-17-aria-labels-skeleton`.

M4.17 makes the M4.15-focusable / M4.10-clickable province
markers **screen-reader-readable**. Two new attributes per
element:

- `role="button"` — tells assistive tech that the marker
  is an interactive control (matches the click + Enter/Space
  activation).
- `aria-label="<name>, <owner_name>"` — gives the otherwise
  nameless `<circle>` a readable name; gives the `<text>` a
  consistent name so the announcement is the same regardless
  of which sibling has focus.

This is the **screen-reader-name skeleton**, not a full
ARIA pass. State-of-control attributes (`aria-selected`,
`aria-current`, `aria-pressed`), live regions
(`aria-live`), and description / labelling indirection
(`aria-describedby`, `aria-labelledby`) are explicitly
deferred to a future broader A11Y sub-milestone.

## 1. Scope

What ships:

```text
src/leviathan/systems/svg_export.cpp
  - render_svg_root: composes aria_label_raw =
      owner_name.empty() ? p.name : (p.name + ", " + owner_name)
    then xml_attr_escape's it. Emits role="button" +
    aria-label="..." on both <circle> and <text>.
  - Reuses the existing M4.8/M4.13 single bounds check so
    `owner_name.empty()` exactly matches the fallback
    branch for `data-owner-name` (and `data-owner-code`).

include/leviathan/systems/svg_export.hpp
  - Intro paragraph mentions M4.17 reverses the M4.15/M4.16
    "no ARIA" non-goal in a narrowly-scoped way.
  - Output-shape <circle> + <text> attribute lists grow by
    role="button" and aria-label.
  - "What ... deliberately do NOT do" heading extends to M4.17.

tests/systems/svg_export_test.cpp
  - Existing M4.15/M4.16 "NO ARIA polish" tests retuned —
    role= and aria-label= are no longer absent; the
    narrower still-deferred ARIA surface (aria-selected /
    aria-current / aria-pressed / aria-live /
    aria-describedby / aria-labelledby) IS still absent.
  - +7 new M4.17 doctest cases (role on circle+text;
    composed label for valid owner; label inside <text>
    opening tag; fallback to <name> alone for invalid
    owner; XML-attribute-escaped against the five
    metacharacters; legend swatch carries neither role
    nor aria-label; map.html propagates both attrs).

README.md / docs/README.md / rfc/README.md         M4.17 entry,
                                                   M4 still in
                                                   progress
docs/m4-17-aria-labels-skeleton.md                 this file
```

## 2. The composed label

```cpp
const std::string aria_label_raw =
    owner_name.empty() ? p.name : (p.name + ", " + owner_name);
const std::string aria_label_attr =
    xml_attr_escape(aria_label_raw);
```

Two cases:

| owner state                         | aria-label                |
|-------------------------------------|---------------------------|
| valid (within `state.countries`)    | `<name>, <owner_name>`    |
| invalid (negative or out-of-range)  | `<name>` (no comma)       |

The "no comma" fallback is deliberate: a label like
`"Ghost, "` would read as "Ghost, comma" on some screen
readers; `"Ghost"` reads cleanly.

The composed string is escaped as a single value via the
M4.2 helper, so a name containing `& < > " '` cannot break
the attribute syntax. Example: a province named `"A&B<C>"`
owned by `"X&Y\"Z"` produces
`aria-label="A&amp;B&lt;C&gt;, X&amp;Y&quot;Z"`.

The label values match what the M4.10/M4.11 details panel
renders for the `Province Name` and `Owner Name` rows, so
a screen-reader user hears the same node identity sighted
users see in the panel. (The label does NOT include the
owner code or owner index — those are programmatic
identifiers, not user-facing names.)

## 3. Why role="button" specifically

The M4 click handler treats the marker as a button:
- Mouse click → activate
- Keyboard Tab → focus (M4.15)
- Keyboard Enter / Space → activate (M4.15)
- Visual focus ring → visible (M4.16)

`role="button"` matches that interaction model exactly.
Alternatives considered and rejected:

- `role="link"` — would imply "navigates to another page";
  the click handler doesn't navigate, it repaints the
  details panel.
- `role="option"` (with `role="listbox"` on the SVG) —
  would imply the province is part of a single-selection
  list; the .selected class IS exclusive, but treating the
  whole SVG as a listbox brings expectations (arrow-key
  navigation between options, `aria-activedescendant`)
  that the M4 viewer doesn't fulfil yet. Deferred to a
  future broader A11Y sub-milestone.
- `role="checkbox"` — would imply binary selectable state;
  the M4.12 .selected isn't a checkbox.
- No role — would make screen readers announce the
  focused element as a plain graphic ("circle" or
  "Berlin"), losing the "interactive" cue.

## 4. Why this reverses an explicit non-goal

The M4.15 and M4.16 design notes both said "no ARIA
polish" explicitly. M4.17 is the narrowly-scoped reversal:

- **What lands:** `role="button"` + `aria-label`. These are
  the two attributes a focusable interactive SVG element
  needs to be screen-reader-usable at all.
- **What stays deferred:** `aria-selected`, `aria-current`,
  `aria-pressed`, `aria-live`, `aria-describedby`,
  `aria-labelledby`. These all encode state or relationships
  the M4 viewer doesn't yet have a coherent model for.

Splitting the ARIA work this way keeps each PR's contract
clear. M4.17's contract: "every province marker has a
screen-reader name and is announced as a button". A future
A11Y PR can extend to "and announces its selected state",
"and announces panel updates via a live region", etc.

## 5. What M4.17 does NOT do

```text
no aria-selected= (selection state announcement deferred)
no aria-current= (URL-fragment / route-current marker deferred)
no aria-pressed= (toggle-button semantics deferred)
no aria-live= (panel-update announcement deferred)
no aria-describedby= / aria-labelledby= (description /
   labelling indirection deferred — direct aria-label is enough)
no role= other than "button" (no listbox, no option, no
   tablist, no group, no presentation override)
no <title> child element on <circle> / <text> (SVG-native
   tooltip mechanism — would compete with aria-label)
no <desc> child element
no state mutation, no commands, no AI integration
no events emitted by the announcement
no selection persistence
no tooltip / hover / mouseover
no animation / transition
no keyboard shortcut for the panel
no second <script>, no <script src=>, no <script type=>
no <link>, no external CSS, no external font
no fetch / XHR / storage / history / navigation APIs
no innerHTML / outerHTML / document.write / eval / Function
no inline event attributes
no per-element inline style="..."
no <meta name="viewport">
no save schema bump (still v12) — aria-label is composed
   from existing ProvinceNode + state.countries fields,
   not a new persistent state field
no new state field
no new artefact (still 10)
no new fixture
no new InterestGroupKind / PlayerCommandKind
no runner CLI flag
no neighbour / adjacency edges
no terrain / resources / population overlays
no M4 close-out
no "M4 closed" wording
```

`provinces.svg` AND `map.html` bytes BOTH changed (the
two new attributes on every `<circle>` + `<text>`).
Additive only — no removed attributes, no rendered-pixel
movement.

## 6. Test coverage

Unit (`tests/systems/svg_export_test.cpp`):

- `render_provinces: M4.17 <circle> + <text> carry
  role="button"` — pins the count at 2 per province pair.
- `render_provinces: M4.17 aria-label is composed as
  "<name>, <owner_name>" for valid owner` — pins
  `aria-label="Berlin, Germany"` appears on both circle
  and text (count == 2u).
- `render_provinces: M4.17 aria-label inside the <text>
  opening tag (uniform identity)` — explicit inside-tag
  check so the attribute can't be on the wrong element.
- `render_provinces: M4.17 aria-label falls back to
  <name> alone when owner is invalid` — pins `Ghost`
  (no `, ` suffix) and explicitly rejects `Ghost, `.
- `render_provinces: M4.17 aria-label is
  XML-attribute-escaped` — composed name with `& < >`
  metachars in both province and country names; escaped
  form `A&amp;B&lt;C&gt;, X&amp;Y&quot;Z` must appear;
  raw form must NOT.
- `render_map_html: M4.17 legend swatch <circle>
  elements do NOT carry role or aria-label` — legend
  swatches stay decorative.
- `render_map_html: M4.17 propagates role + aria-label
  through the inline SVG body` — confirms map.html
  inherits both via the shared render_svg_root.

The M4.15 and M4.16 "NO ARIA polish" cases retuned to
"still-deferred ARIA non-goals" — they now check
`aria-selected=` / `aria-current=` / `aria-pressed=` /
`aria-live=` / `aria-describedby=` / `aria-labelledby=`
are absent (instead of the over-broad
`role= / aria-label= absent` which M4.17 reverses).

Integration: no changes needed. Tests A/B/C/D/E don't
assert ARIA absence; they all stay green unchanged.

## 7. Cross-references

- [`m4-15-keyboard-focus-skeleton.md`](m4-15-keyboard-focus-skeleton.md)
  — the M4.15 keyboard-focus surface M4.17 makes
  screen-reader-readable. Without M4.15's `tabindex="0"`,
  screen readers couldn't reach the markers in the first
  place.
- [`m4-16-focus-visible-skeleton.md`](m4-16-focus-visible-skeleton.md)
  — the M4.16 visual focus ring. M4.16 makes focus
  visible to sighted users; M4.17 makes it announceable
  to screen-reader users. Same surface, two senses.
- [`m4-13-details-owner-name-polish.md`](m4-13-details-owner-name-polish.md)
  — the M4.13 widening that exposed
  `state.countries[owner].name` as `data-owner-name`.
  M4.17 reuses the same source value (and the same
  bounds check) for the aria-label suffix.
- [`m4-11-details-labels-polish.md`](m4-11-details-labels-polish.md)
  — the M4.11 details-panel labels. M4.17's aria-label
  value matches what the panel's `Province Name` /
  `Owner Name` rows render, so sighted + screen-reader
  users see / hear the same identity.
- [`m4-8-province-data-attributes-skeleton.md`](m4-8-province-data-attributes-skeleton.md)
  — the M4.8 widening pattern M4.17 mirrors (same attrs
  on circle + text via uniform identity surface).
- [`milestone-4-checkpoint.md`](milestone-4-checkpoint.md)
  — the M4 status snapshot (refreshed at M4.14). The
  "interactivity surface" + "deferred items" sections
  will need a future refresh: ARIA labels + role=button
  shipped at M4.17; the narrower ARIA surface stays
  deferred.
- [`milestone-3-result.md`](milestone-3-result.md) §5 —
  M3 invariants. M4.17 preserves all of them.
