// M5.5: EventFirer implementation.
//
// See include/leviathan/systems/event_firer.hpp for the public
// contract (semantics, conversion rules, deliberate non-goals).
// This file is the small bridge that turns M5.3 actor binding
// into M5.4 EventInstance records.

#include "leviathan/systems/event_firer.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "leviathan/core/log_entry.hpp"
#include "leviathan/systems/bias_noise.hpp"
#include "leviathan/systems/information_accuracy.hpp"
#include "leviathan/systems/reported_value.hpp"

namespace leviathan::systems::event_firer {
namespace {

// Resolve a CountryId to its on-disk id_code by linear scan of
// state.countries. Linear scan is fine here: state.countries is
// canonically small (scenario-load order; canonical 1930 has 3),
// and this is not on a per-tick hot path (a future M5.x runner
// integration will fire at most once per month-boundary
// evaluation, not once per day).
//
// Returns empty string if no country has matching id. Caller
// (M5.5 record_match below) treats empty country_id_code as a
// signal that state is internally inconsistent — the save
// layer's M5.4 validation will reject the resulting save round-
// trip, surfacing the bug loudly instead of silently corrupting
// history.
std::string country_id_code_for(const core::GameState& state,
                                core::CountryId        cid) {
    for (const auto& c : state.countries) {
        if (c.id == cid) {
            return c.id_code;
        }
    }
    return std::string{};
}

// Convert one TriggerEvaluation into one EventInstanceActor.
// kind string mapping: Country -> "country", InterestGroup ->
// "interest_group" (matches the M5.4 save-layer allowlist).
core::EventInstanceActor to_actor(
        const core::GameState&                          state,
        const event_evaluator::TriggerEvaluation&       te) {
    core::EventInstanceActor a;
    a.id_code = te.actor.id_code;
    a.index   = te.actor.index;
    switch (te.actor.kind) {
        case event_evaluator::TriggerActorKind::Country: {
            a.kind            = "country";
            // A country IS its own owning country: id_code is
            // both the actor's identity and its
            // country_id_code. Using id_code directly avoids
            // a redundant lookup of state.countries[idx]
            // that would produce the same string.
            a.country_id_code = te.actor.id_code;
            break;
        }
        case event_evaluator::TriggerActorKind::InterestGroup: {
            a.kind            = "interest_group";
            // The IG's owning country is referenced by
            // CountryId — resolve to id_code via the state
            // lookup. If lookup fails (state internally
            // inconsistent), country_id_code stays empty and
            // the save layer rejects it on next round-trip.
            a.country_id_code = country_id_code_for(state,
                                                    te.actor.country);
            break;
        }
    }
    return a;
}

// M6.9 distortion result: the four numeric values + the
// visible_report string a non-debug consumer needs. The four
// pieces are emitted as four separate metadata keys so the
// JSONL writer keeps each one greppable; lumping them into a
// single composite string would hide the structure.
struct DistortionFields {
    bool         emit_distortion = false;
    std::string  visible_report;
    double       information_accuracy = 0.0;
    double       reported_intensity   = 0.0;
    double       noise_sample         = 0.0;
};

// Compose the M6.3 / M6.4 / M6.5 pipeline into the
// player-facing distortion fields for one fired event,
// against the first-actor country. RFC-080 §8 numeric anchor:
// `TrueValue = 1.0` per fired event ("this event happened
// with intensity 1.0"); the player's perception of intensity
// is `accuracy + noise`. Pure read of state — no mutation.
//
// Returns failure if any of the three helpers fails. Returns
// success with `emit_distortion = false` when there is no
// country actor to compute accuracy against (vacuous-actor
// edge case).
core::Result<DistortionFields> compute_distortion_fields(
    const core::GameState&        state,
    const core::EventDefinition&  definition,
    const std::string&            first_actor_country_id_code,
    const core::GameDate&         fired_on) {
    using R = core::Result<DistortionFields>;
    DistortionFields out;
    out.visible_report = definition.visible_report;

    if (first_actor_country_id_code.empty()) {
        // Vacuous-actor event: no country anchor; emit only
        // the visible_report string (still useful for the
        // player as the public-facing text) but skip the
        // numeric distortion fields. Not an error.
        return R::success(std::move(out));
    }

    // Look up the country INDEX (not the stored CountryId field).
    // information_accuracy::compute_for_country validates its
    // CountryId argument as a valid index into state.countries;
    // constructing the CountryId from the vector index sidesteps
    // any stale or default-constructed `c.id` on hand-built
    // fixtures. The id-code → index mapping is the canonical
    // resolution (RFC-070 §6 / M1.1).
    std::size_t country_index = 0;
    bool found = false;
    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        if (state.countries[i].id_code == first_actor_country_id_code) {
            country_index = i;
            found = true;
            break;
        }
    }
    if (!found) {
        // Country id_code resolved by record_match but not
        // findable in state.countries — internal inconsistency.
        // Fail loudly per feedback_no_silent_degradation.
        return R::failure(
            "event_firer::record_match (M6.9): first-actor "
            "country_id_code '" + first_actor_country_id_code +
            "' has no matching entry in state.countries");
    }
    const core::CountryId cid{static_cast<int>(country_index)};

    auto acc_r =
        information_accuracy::compute_for_country(state, cid);
    if (!acc_r) {
        return R::failure(
            "event_firer::record_match (M6.9 information_accuracy): " +
            std::move(acc_r.error()));
    }
    const double accuracy = acc_r.value();

    // M6.4 reported value: TrueValue = 1.0 anchor; reported
    // intensity = accuracy by direct multiplication.
    auto rep_r = reported_value::from_true_value(1.0, accuracy);
    if (!rep_r) {
        return R::failure(
            "event_firer::record_match (M6.9 reported_value): " +
            std::move(rep_r.error()));
    }

    // M6.5 noise: amplitude = 1 - accuracy. At accuracy 1.0
    // (intelligence maxed, no corruption) the amplitude is 0
    // and the bias_noise helper returns 0 verbatim — player
    // sees the truth. At accuracy 0.0 the amplitude is 1.0
    // and the noise spans [-1, +1] for maximal distortion.
    // Per `feedback_no_silent_degradation`, the amplitude is
    // computed by subtraction over a strict-validated
    // accuracy in [0, 1], so it is provably in [0, 1] —
    // bias_noise's input validation never trips.
    const double amplitude = 1.0 - accuracy;
    auto noise_r = bias_noise::sample_for_event(
        definition.id_code,
        first_actor_country_id_code,
        fired_on,
        amplitude);
    if (!noise_r) {
        return R::failure(
            "event_firer::record_match (M6.9 bias_noise): " +
            std::move(noise_r.error()));
    }

    out.emit_distortion        = true;
    out.information_accuracy   = accuracy;
    out.reported_intensity     = rep_r.value();
    out.noise_sample           = noise_r.value();
    return R::success(std::move(out));
}

// Stable double->string format for metadata numeric fields.
// Use %.17g so the round-tripped double is exact within IEEE-754
// — matches save_system's existing double serialisation
// philosophy. The locale-independent printf path avoids the
// std::to_string "always 6 fractional digits" lossy default.
std::string format_double(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return std::string(buf);
}

}  // namespace

core::Result<bool> record_match(
        core::GameState&                                state,
        const event_evaluator::EventMatch&              match,
        const core::GameDate&                           fired_on) {
    core::EventInstance inst;
    inst.event_id_code = match.event_id_code;
    inst.fired_on      = fired_on;
    inst.actors.reserve(match.triggers.size());
    for (const auto& te : match.triggers) {
        inst.actors.push_back(to_actor(state, te));
    }

    // RCR-1: RFC-090 §5.9 — emit one per-fire LogEntry into
    // `state.logs` so the M0.6 events.jsonl artefact surfaces
    // event firings. Canonical scenarios at M5 deliberately
    // tuned their events to NOT fire, so canonical events.jsonl
    // bytes stay byte-identical with the M5 close-out (the
    // canonical-non-fire property is the load-bearing invariant
    // that keeps M1.17 / M2 / M3 / M4 / M5 determinism baselines
    // green). Only scenarios where events fire receive these
    // new entries.
    //
    // kind="event_fired"; metadata is a stable insertion-order
    // sequence of:
    //   event_id_code
    //   actor_kind         (kind of actors[0], or "<none>" when
    //                       actors is empty)
    //   actor_id_code      (id_code of actors[0], or "")
    //   country_id_code    (country_id_code of actors[0], or "")
    //   true_cause         (M6.8: verbatim
    //                       EventDefinition.true_cause string)
    //
    // M6.8 (RFC-090 §6.8 "debug 模式顯示真相"): the
    // `true_cause` metadata key is appended here UNCONDITIONALLY
    // at fire time so the truth is always present in
    // `state.logs` (and therefore in the save.json `logs` array
    // — debuggable / inspectable / replay-stable). The
    // events.jsonl artefact filters this key out unless
    // `RunnerOptions::debug_mode == true`; see
    // `logging::export_jsonl` and runner::end_tick. The
    // EventDefinition.true_cause field has been required non-
    // empty on `state.events` since M6.1, so this lookup never
    // produces an empty string.
    //
    // Multi-actor events emit ONE log entry with the first
    // actor's surface attached; the EventInstance in
    // state.event_history records the full actor list. This
    // keeps events.jsonl per-fire records compact while leaving
    // the full audit trail in the save artefact.
    core::LogEntry entry;
    entry.date     = fired_on;
    entry.category = "event_fired";
    entry.source   = "event_firer";
    entry.message  = std::string("event ") + match.event_id_code +
                     " fired";
    entry.metadata.push_back({"event_id_code", match.event_id_code});
    if (!inst.actors.empty()) {
        const auto& a0 = inst.actors.front();
        entry.metadata.push_back({"actor_kind",      a0.kind});
        entry.metadata.push_back({"actor_id_code",   a0.id_code});
        entry.metadata.push_back({"country_id_code", a0.country_id_code});
    } else {
        entry.metadata.push_back({"actor_kind",      std::string("<none>")});
        entry.metadata.push_back({"actor_id_code",   std::string()});
        entry.metadata.push_back({"country_id_code", std::string()});
    }
    // M6.8: append the M6.1 true_cause string sourced from the
    // EventDefinition that owns this match. event_index is
    // populated by the M5.2 / M5.3 match-construction surface and
    // is always in range when record_match is called on a real
    // match (event_engine::tick_events / event_evaluator never
    // emit a match with an out-of-range index).
    if (match.event_index >= state.events.size()) {
        // Defensive: a malformed match would silently drop the
        // M6.8 / M6.9 fields. Fail loudly per
        // `feedback_no_silent_degradation`.
        return core::Result<bool>::failure(
            "event_firer::record_match: match.event_index " +
            std::to_string(match.event_index) +
            " out of range (state.events.size = " +
            std::to_string(state.events.size()) + ")");
    }
    const auto& definition = state.events[match.event_index];
    entry.metadata.push_back({"true_cause", definition.true_cause});

    // M6.9 (RFC-090 §6.9 "非 debug 模式隱藏真相"): compose the
    // M6.3 / M6.6 / M6.7 information_accuracy helper with the
    // M6.4 reported_value helper and the M6.5 bias_noise helper
    // to emit the player-facing distortion fields. The
    // distortion is computed BEFORE state mutation; on failure
    // neither the EventInstance nor the LogEntry is appended
    // (per-event atomicity per feedback_no_silent_degradation).
    const std::string first_actor_country =
        inst.actors.empty() ? std::string{}
                            : inst.actors.front().country_id_code;
    auto dist_r = compute_distortion_fields(
        state, definition, first_actor_country, fired_on);
    if (!dist_r) {
        return core::Result<bool>::failure(std::move(dist_r.error()));
    }
    const auto& dist = dist_r.value();
    // visible_report is always emitted (player-facing text;
    // present even on vacuous-actor events for grep'ability).
    entry.metadata.push_back({"visible_report", dist.visible_report});
    if (dist.emit_distortion) {
        entry.metadata.push_back(
            {"information_accuracy", format_double(dist.information_accuracy)});
        entry.metadata.push_back(
            {"reported_intensity",   format_double(dist.reported_intensity)});
        entry.metadata.push_back(
            {"noise_sample",         format_double(dist.noise_sample)});
    }
    state.logs.push_back(std::move(entry));

    state.event_history.push_back(std::move(inst));
    return core::Result<bool>::success(true);
}

core::Result<FireOutcome> record_matches(
        core::GameState&                                  state,
        const std::vector<event_evaluator::EventMatch>&   matches,
        const core::GameDate&                             fired_on) {
    FireOutcome out;
    for (const auto& m : matches) {
        auto r = record_match(state, m, fired_on);
        if (!r) {
            return core::Result<FireOutcome>::failure(std::move(r.error()));
        }
        out.recorded += 1;
    }
    return core::Result<FireOutcome>::success(std::move(out));
}

core::Result<bool> record_followup(
        core::GameState&             state,
        const core::EventInstance&   parent_instance,
        const core::EventDefinition& followup_definition,
        const core::GameDate&        fired_on) {
    core::EventInstance inst;
    inst.event_id_code = followup_definition.id_code;
    inst.fired_on      = fired_on;
    inst.actors        = parent_instance.actors;   // inherit from parent

    // Same log emission shape as record_match. Use the parent's
    // first-actor surface so events.jsonl can be filtered by
    // country_id_code consistently across base + followup fires.
    // M6.8 (RFC-090 §6.8): the followup's own `true_cause`
    // string is recorded as the last metadata key, mirroring the
    // record_match path; `logging::export_jsonl` filters it out
    // unless `RunnerOptions::debug_mode == true`.
    core::LogEntry entry;
    entry.date     = fired_on;
    entry.category = "event_fired";
    entry.source   = "event_firer";
    entry.message  = std::string("event ") + followup_definition.id_code +
                     " fired (followup of " +
                     parent_instance.event_id_code + ")";
    entry.metadata.push_back({"event_id_code", followup_definition.id_code});
    entry.metadata.push_back({"followup_of",   parent_instance.event_id_code});
    if (!inst.actors.empty()) {
        const auto& a0 = inst.actors.front();
        entry.metadata.push_back({"actor_kind",      a0.kind});
        entry.metadata.push_back({"actor_id_code",   a0.id_code});
        entry.metadata.push_back({"country_id_code", a0.country_id_code});
    } else {
        entry.metadata.push_back({"actor_kind",      std::string("<none>")});
        entry.metadata.push_back({"actor_id_code",   std::string()});
        entry.metadata.push_back({"country_id_code", std::string()});
    }
    entry.metadata.push_back(
        {"true_cause", followup_definition.true_cause});

    // M6.9: same distortion-field composition as record_match.
    // Followups inherit the parent's first-actor country, so the
    // accuracy / noise are computed against that country. Per-
    // event atomicity preserved: if distortion fails, neither
    // the EventInstance nor the LogEntry is appended.
    const std::string first_actor_country =
        inst.actors.empty() ? std::string{}
                            : inst.actors.front().country_id_code;
    auto dist_r = compute_distortion_fields(
        state, followup_definition, first_actor_country, fired_on);
    if (!dist_r) {
        return core::Result<bool>::failure(std::move(dist_r.error()));
    }
    const auto& dist = dist_r.value();
    entry.metadata.push_back({"visible_report", dist.visible_report});
    if (dist.emit_distortion) {
        entry.metadata.push_back(
            {"information_accuracy", format_double(dist.information_accuracy)});
        entry.metadata.push_back(
            {"reported_intensity",   format_double(dist.reported_intensity)});
        entry.metadata.push_back(
            {"noise_sample",         format_double(dist.noise_sample)});
    }
    state.logs.push_back(std::move(entry));

    state.event_history.push_back(std::move(inst));
    return core::Result<bool>::success(true);
}

}  // namespace leviathan::systems::event_firer
