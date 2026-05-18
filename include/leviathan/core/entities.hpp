// Entity placeholders + M1 baseline state.
//
// M0 introduced these as ID-only stubs; M1.1 fleshed out CountryState;
// M1.2 fleshed out FactionState; M1.3 added BudgetState (embedded in
// CountryState); M1.4 fleshed out PolicyData with its effects vector;
// M4.1 fleshed out the province map-node data layer as ProvinceNode
// (replacing the M0 ProvinceState stub); M5.1 fleshed out
// EventDefinition with the trigger/effect schema (replacing the M0
// stub). Remaining entity growth is additive and milestone-scoped.
//
// Naming convention for CountryState numeric fields:
//   * gdp / tax_revenue / budget_balance      - absolute amounts
//   * legal_tax_burden, fiscal_capacity, ...  - 0-to-1 ratios
//   * stability, legitimacy                   - 0-to-1 ratios
//   * military_power, threat_perception       - 0-to-1 ratios
//
// The DataLoader (M0.7) maps JSON "initial_gdp" / "initial_stability"
// onto the runtime `gdp` / `stability` fields; the other fields share
// names with their JSON counterparts. This keeps the on-disk config
// readable ("initial_..." reads as a baseline) while keeping the
// runtime struct compact.

#ifndef LEVIATHAN_CORE_ENTITIES_HPP
#define LEVIATHAN_CORE_ENTITIES_HPP

#include <string>
#include <vector>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/ids.hpp"

namespace leviathan::core {

// One policy currently active on a country (M1.15).
//
// Inserted into `CountryState::active_policies` by every successful
// `policy::apply_policy_effects` call. The pair is purely a record:
// `expires_on` is the calendar date when the policy's `duration_days`
// will have elapsed (computed at apply time as `current_date +
// duration_days`). M1.15 only TRACKS this list. Nothing removes
// expired entries yet; no system reverts the policy's effects when
// the date arrives. Those behaviours are deliberate non-goals for
// this sub-milestone and will land in a later milestone that adds
// an explicit expiration / revert mechanism.
//
// We store `policy_id_code` (the on-disk string identifier) rather
// than a `PolicyId`. The string is stable across loads even if
// PolicyId values get reassigned by vector index; the numeric id is
// always re-resolvable from `state.policies` if a future caller
// needs it.
struct ActivePolicy {
    std::string policy_id_code;
    GameDate    expires_on{};
};

// Per-country budget allocation. Each field is the share of total
// state spending devoted to that category, in [0, 1]. The seven
// categories follow RFC-010 §2.4 and RFC-060 §3.
//
// We deliberately DO NOT enforce that the seven fields sum to 1 at
// the data-model layer. A new state may be authored with an
// under-allocated budget (e.g. uncommitted surplus) or an
// over-allocated one (deficit spending). Whether the sum is
// meaningful is an economy-tick concern (M1.5+) and a future
// Diagnostics::sanity_check rule, not a load-time invariant here.
struct BudgetState {
    double administration  = 0.0;
    double military        = 0.0;
    double education       = 0.0;
    double welfare         = 0.0;
    double intelligence    = 0.0;
    double infrastructure  = 0.0;
    double industry        = 0.0;
};

// Per-country government authority state (M2.16).
//
// Four [0, 1] ratio fields, defaulting to 0.5, capturing the
// stripped-down RFC-020 §3 "state control" surface. M2.16 only adds
// the data shape — no system reads or writes these yet. Future
// gameplay (RFC-020 §2-§3) will consume them as command-execution
// resistance / information-accuracy inputs.
//
// Deliberately deferred from the RFC-020 §3 list and meant to land
// in later sub-milestones: local_control (distinct from the
// existing `CountryState::central_control`), legal_mandate,
// leader_prestige, party_organization. `corruption` and
// `administrative_efficiency` already live on `CountryState` (M1.1)
// and are not duplicated here.
//
// Defaults are 0.5 across the board so a country file that omits
// the block loads to a neutral starting position; scenarios that
// want explicit values override per-field.
struct GovernmentAuthorityState {
    double bureaucratic_compliance  = 0.5;
    double military_loyalty         = 0.5;
    double intelligence_capability  = 0.5;
    double media_control            = 0.5;
};

struct CountryState {
    // Identity
    CountryId   id;
    std::string id_code;
    std::string name;
    std::string display_name;

    // Absolute economic state
    double gdp            = 0.0;
    double tax_revenue    = 0.0;   // runtime-only, derived; not in JSON config
    double budget_balance = 0.0;   // runtime-only, can be negative

    // Fiscal / administrative ratios (0..1)
    double legal_tax_burden          = 0.0;
    double fiscal_capacity           = 0.0;
    double administrative_efficiency = 0.0;
    double central_control           = 0.0;
    double corruption                = 0.0;

    // Political ratios (0..1)
    double stability  = 0.0;
    double legitimacy = 0.0;

    // Strategic ratios (0..1)
    double military_power     = 0.0;
    double threat_perception  = 0.0;

    // Most recent gdp_growth_rate written by economy::tick (M1.12).
    // Fractional, e.g. 0.0035 = +0.35% monthly. Starts at 0.0 so the
    // first stability::tick sees "no growth signal yet" (RFC-080 §5
    // EconomicGrowth = 0). Updated AFTER the monthly economy tick;
    // the next month's stability::tick reads it as input, producing
    // a deliberate one-month lag (see docs/m1-12-...md).
    double last_gdp_growth_rate = 0.0;

    // Per-category budget allocation (M1.3). Loaded as a nested
    // JSON object; saved likewise.
    BudgetState budget;

    // Government authority sub-state (M2.16). Four [0, 1] ratios
    // capturing the stripped-down RFC-020 §3 "state control"
    // surface — bureaucratic compliance, military loyalty,
    // intelligence capability, media control. Defaults to all
    // 0.5 so a country file that omits the block loads neutrally.
    // M2.16 is data-only: no M1 system reads or writes these
    // fields yet. Save format v10 makes the block required at the
    // save-file level; DataLoader treats the block as optional in
    // country JSON.
    GovernmentAuthorityState government_authority;

    // Policies currently enacted on this country (M1.15). Appended
    // by `policy::apply_policy_effects` on each successful call;
    // never removed in M1.15. Order is insertion-order. Persisted
    // by SaveSystem as save format v7.
    std::vector<ActivePolicy> active_policies;
};

// Map-node entry for the M4 SVG-map skeleton (M4.1).
//
// M4.1 fleshes out what used to be the M0 `ProvinceState` stub
// (`id` + `owner` only, never read by any system) into the
// minimum shape required by a future SVG exporter / HTML viewer
// / clickable map: a stable string identifier, a human-readable
// label, the owning country, and a 2-D position in normalised
// `[0, 1]` map space.
//
// Normalised coordinates were chosen so the data layer can land
// before any projection / pixel-size / SVG-path decision; a
// future renderer multiplies through by the canvas dimensions.
//
// Deliberately deferred until a future M4.x sub-milestone:
// population, terrain, resources, neighbour adjacency, ports,
// fronts, raw SVG paths, colours, controller-vs-owner split,
// claims, victory points. M4.1 ships nodes, not behaviour.
struct ProvinceNode {
    std::string id_code;
    std::string name;

    CountryId   owner = CountryId::invalid();

    double      x = 0.0;   // normalised map x in [0, 1]
    double      y = 0.0;   // normalised map y in [0, 1]
};

struct FactionState {
    // Identity
    FactionId   id;                  // numeric handle (caller-assigned)
    CountryId   country;             // numeric link to CountryState (caller-resolved)
    std::string id_code;             // on-disk identifier, e.g. "GER_military"
    std::string country_id_code;     // links to CountryState::id_code, e.g. "GER"
    std::string name;                // "German Military"
    std::string type;                // RFC-010 §2.5: military, bureaucracy,
                                     // workers, local_elites, media,
                                     // intelligence, students, technical_elites

    // Behavioural ratios (0..1)
    double support    = 0.0;   // popular backing for the faction
    double influence  = 0.0;   // political clout / share of decision power
    double radicalism = 0.0;   // willingness to escalate
    double loyalty    = 0.0;   // loyalty to the current regime

    // Absolute resources (>= 0). Units are abstract for now; M1.3
    // (economy / budget) decides what they map to.
    double resources  = 0.0;

    // Policy id_codes this faction is inclined to favour. M1.4 will
    // tighten the semantics once PolicyData has shape; for M1.2 it is
    // a free-form list of strings that survives the JSON round trip.
    std::vector<std::string> preferred_policies;
};

// One change a policy applies when it is enacted.
//
// `target` is a free-form path string ("country.military_power",
// "faction:military.support", ...). `op` is currently a free-form
// operation string ("add", "set", ...). M1.4 stores these as
// strings; M1.5 (PolicySystem) interprets them. If branch-on-`op`
// dispatch gets unwieldy, a future PR can introduce an enum.
struct PolicyEffect {
    std::string target;
    std::string op;
    double      value = 0.0;
};

struct PolicyData {
    // Identity
    PolicyId    id;             // numeric, caller-assigned
    std::string id_code;        // on-disk identifier, e.g. "increase_military_budget"
    std::string name;           // "Increase Military Budget"

    // Classification
    std::string category;       // "budget" / "tax" / "media" / "intelligence" / ...

    // Activation cost
    int    duration_days = 0;   // >= 0
    double admin_cost    = 0.0; // [0, 1] - share of administrative capacity consumed

    // Effects applied on enactment (M1.5 will interpret them).
    std::vector<PolicyEffect> effects;
};

// One comparison clause for an event firing precondition.
//
// `target` is a free-form path string against a small allowlist
// enforced at LOAD time (M5.1 schema):
//   country.stability
//   country.legitimacy
//   country.government_authority.bureaucratic_compliance
//   interest_group.radicalism
//   interest_group.loyalty
// `op` is a comparison operator from the allowlist {lt, lte, gt,
// gte}. `value` must be finite.
//
// M5.1 introduced the trigger schema (data only). M5.2 added
// the trigger evaluator with AND semantics across a definition's
// triggers. M5.3 enriched matches with actor binding (which
// country / interest group satisfied each trigger). M5.4 added
// the event_history data layer for fired-event records. Auto-fire
// / effects application / runner integration / events.jsonl
// integration are still deferred to a future M5.x.
struct EventTrigger {
    std::string target;
    std::string op;
    double      value = 0.0;
};

// One event definition loaded from a scenario fixture and stored
// in `GameState::events`. M5.1 ships the schema only — no firing,
// no effects application, no event history. Effects reuse the
// existing `PolicyEffect` shape so a future M5.x can extract a
// shared effect applicator.
//
// M0 had an `{ EventId id; std::string name; }` stub here; M5.1
// upgrades it in place. `id_code` is the natural string identifier
// (matches `ProvinceNode::id_code` / `InterestGroupState::id_code`
// style — no separate numeric `EventId` until a system needs one).
struct EventDefinition {
    std::string               id_code;
    std::string               name;
    std::string               description;
    std::vector<EventTrigger> triggers;   // non-empty at load
    std::vector<PolicyEffect> effects;    // may be empty (warning-only)
};

// M5.4: one fired event's recorded actor (the country or interest
// group that satisfied a trigger). Mirrors the runtime
// `event_evaluator::TriggerActor` shape but stores stable strings
// only — no `CountryId` numeric handle — so the save layer can
// round-trip without depending on session-local index values.
//
// `kind` is a closed enum-as-string: `"country"` or
// `"interest_group"`. The save layer enforces the allowlist.
//
// `id_code` is the entity's stable identifier (country id_code for
// `kind="country"`; IG id_code for `kind="interest_group"`).
//
// `country_id_code` is always the *owning* country: for
// `kind="country"` it equals `id_code`; for `kind="interest_group"`
// it is the IG's owning country (so a future "apply this IG-trigger's
// effect to the IG's owning country" path doesn't need a lookup).
//
// `index` is the position in the appropriate live state vector at
// the time the event fired. It is a transient runtime hint, not a
// stable identifier — `id_code` is authoritative.
struct EventInstanceActor {
    std::string kind;             // "country" | "interest_group"
    std::string id_code;
    std::string country_id_code;  // owning country id_code (self for kind=country)
    std::size_t index = 0;        // pos in state.countries / state.interest_groups
};

// M5.4: one fired event record stored in `GameState::event_history`.
//
// M5.4 ships the DATA layer only — no system creates these records
// yet (no auto-fire, no effects application). Hand-built records
// round-trip through the save layer for forward-compat with future
// M5.x sub-milestones that introduce the firer.
//
// `event_id_code` references an `EventDefinition::id_code`. The
// save layer does not cross-check that the referenced event still
// exists in `state.events` — replaying a save with a different
// scenario manifest could legitimately leave history entries that
// no longer have a matching definition.
//
// `fired_on` is the calendar date the event fired (mirrors the M2.4
// `AppliedPlayerCommand::applied_on` shape).
//
// `actors` mirrors the M5.3 `EventMatch::triggers` actor binding,
// one entry per `EventDefinition::triggers` entry that was
// satisfied at fire time, in definition order. Empty vector is
// allowed at the data layer (vacuously-true / no-trigger defs)
// but the M5.1 loader rejects empty triggers, so this case is
// unreachable through canonical events.
struct EventInstance {
    std::string                     event_id_code;
    GameDate                        fired_on;
    std::vector<EventInstanceActor> actors;
};

// Coarse-grained category for an interest group / political actor
// (M3.1). Mirrors the RFC-020 §5 long-term faction list, narrowed
// to the ten kinds M3 expects to model in its first iteration.
// String mapping is shared via
// `leviathan/core/interest_group_kind.hpp` (M3.5); scenario JSON
// spells the variant out ("Bureaucracy", "Military", ...).
//
// Future kinds (Nationalists, Aristocracy, Intelligence as a
// distinct group, ...) land additively. Existing variant names
// must not be renamed or reordered.
enum class InterestGroupKind {
    Bureaucracy,
    Military,
    Workers,
    Farmers,
    Religious,
    Media,
    Students,
    LocalElites,
    Business,
    Technocrats,
};

// One interest group / political actor (M3.1).
//
// M3.1 ships only the data shape: no M1/M2 system reads or
// mutates these fields. Future M3 sub-milestones will introduce
// monthly reactions, command resistance contributions, event
// triggers, and AI behaviour against this struct.
//
// Identity:
//   * `id_code` — stable on-disk identifier ("ger_bureaucracy").
//   * `name`    — human-readable label.
//   * `kind`    — coarse category from `InterestGroupKind`.
//   * `country` — owning country; numeric handle (caller-resolved
//                 from the scenario manifest's `country` id_code).
//
// Behavioural ratios (all in [0, 1] unless noted):
//   * `influence`  — share of political clout this group commands.
//   * `loyalty`    — loyalty to the current regime / player.
//   * `radicalism` — willingness to escalate / disrupt stability.
//
// Defaults reflect "neutral newly-authored group": equal influence
// and loyalty at the mid-point, zero radicalism.
struct InterestGroupState {
    std::string       id_code;
    std::string       name;
    InterestGroupKind kind    = InterestGroupKind::Bureaucracy;
    CountryId         country = CountryId::invalid();

    double influence  = 0.5;
    double loyalty    = 0.5;
    double radicalism = 0.0;
};

}  // namespace leviathan::core

#endif  // LEVIATHAN_CORE_ENTITIES_HPP
