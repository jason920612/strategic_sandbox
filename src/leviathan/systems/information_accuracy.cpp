#include "leviathan/systems/information_accuracy.hpp"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

namespace leviathan::systems::information_accuracy {

namespace {

std::optional<core::Result<double>> require_unit_ratio(
        const std::string& country_id_code,
        const char*        field_name,
        double             value) {
    if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: "
            "country '" + country_id_code + "' " + field_name +
            " = " + std::to_string(value) +
            " is not a finite ratio in [0, 1]");
    }
    return std::nullopt;
}

}  // namespace

core::Result<double> compute_for_country(
        const core::GameState& state,
        core::CountryId        country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value())
            >= state.countries.size()) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: "
            "actor CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }

    const auto& c = state.countries[
        static_cast<std::size_t>(country.value())];

    if (auto err = require_unit_ratio(
            c.id_code,
            "government_authority.intelligence_capability",
            c.government_authority.intelligence_capability)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code, "budget.intelligence",
            c.budget.intelligence)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code, "government_authority.media_control",
            c.government_authority.media_control)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code,
            "government_authority.bureaucratic_professionalism",
            c.government_authority.bureaucratic_professionalism)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code,
            "government_authority.audit_capacity",
            c.government_authority.audit_capacity)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code, "corruption", c.corruption)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code,
            "government_authority.leader_isolation",
            c.government_authority.leader_isolation)) {
        return *err;
    }
    if (auto err = require_unit_ratio(
            c.id_code, "central_control", c.central_control)) {
        return *err;
    }

    double faction_capture = 0.0;
    bool any_group = false;
    for (const auto& g : state.interest_groups) {
        if (g.country.value() != country.value()) {
            continue;
        }
        if (auto err = require_unit_ratio(
                c.id_code, "interest_group.influence", g.influence)) {
            return *err;
        }
        if (auto err = require_unit_ratio(
                c.id_code, "interest_group.radicalism", g.radicalism)) {
            return *err;
        }
        const double capture = g.influence * g.radicalism;
        if (!any_group || capture > faction_capture) {
            faction_capture = capture;
            any_group = true;
        }
    }

    const double intelligence_capacity =
        kInformationAccuracyCapabilityWeight *
            c.government_authority.intelligence_capability +
        kInformationAccuracyBudgetWeight *
            c.budget.intelligence;
    const double media_freedom_signal =
        1.0 - c.government_authority.media_control;
    const double local_autonomy_opacity =
        1.0 - c.central_control;

    const double accuracy =
        kInformationAccuracyBase +
        kInformationAccuracyIntelligenceWeight *
            intelligence_capacity +
        kInformationAccuracyMediaFreedomSignalWeight *
            media_freedom_signal +
        kInformationAccuracyBureaucraticProfessionalismWeight *
            c.government_authority.bureaucratic_professionalism +
        kInformationAccuracyAuditCapacityWeight *
            c.government_authority.audit_capacity -
        kInformationAccuracyFullCorruptionWeight *
            c.corruption -
        kInformationAccuracyFactionCaptureWeight *
            faction_capture -
        kInformationAccuracyLeaderIsolationWeight *
            c.government_authority.leader_isolation -
        kInformationAccuracyLocalAutonomyOpacityWeight *
            local_autonomy_opacity;

    if (!std::isfinite(accuracy) || accuracy < 0.0 || accuracy > 1.0) {
        return core::Result<double>::failure(
            "information_accuracy::compute_for_country: computed accuracy = " +
            std::to_string(accuracy) +
            " is outside [0, 1] despite validated inputs");
    }
    return core::Result<double>::success(accuracy);
}

}  // namespace leviathan::systems::information_accuracy
