#include "leviathan/systems/diagnostics.hpp"

#include <cmath>
#include <cstddef>
#include <iomanip>
#include <ios>
#include <ostream>
#include <set>
#include <sstream>
#include <string>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/ids.hpp"
#include "leviathan/core/interest_group_kind.hpp"

namespace leviathan::systems::diagnostics {

namespace {

// Format a double with full round-trip precision (17 significant
// digits in std::scientific). Determinism over prettiness: the same
// double value always produces the same string.
std::string fmt_double(double v) {
    std::ostringstream s;
    s << std::scientific << std::setprecision(17) << v;
    return s.str();
}

// True iff `a` and `b` are within `tol`. Two same-sign infinities
// compare equal (`a == b` covers that). NaN never compares equal,
// even to itself. One finite + one non-finite is never within
// tolerance.
bool approx_equal(double a, double b, double tol) {
    if (a == b) return true;
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    return std::abs(a - b) <= tol;
}

void push_mismatch(std::vector<StateMismatch>& out,
                   std::string path,
                   std::string detail) {
    StateMismatch m;
    m.field_path = std::move(path);
    m.detail     = std::move(detail);
    out.push_back(std::move(m));
}

void check_double(std::vector<StateMismatch>& out,
                  const std::string& path,
                  double a, double b, double tol) {
    if (!approx_equal(a, b, tol)) {
        push_mismatch(out, path,
                      fmt_double(a) + " != " + fmt_double(b) +
                      " (tolerance " + fmt_double(tol) + ")");
    }
}

void check_string(std::vector<StateMismatch>& out,
                  const std::string& path,
                  const std::string& a, const std::string& b) {
    if (a != b) {
        push_mismatch(out, path, "'" + a + "' != '" + b + "'");
    }
}

void check_int(std::vector<StateMismatch>& out,
               const std::string& path,
               long long a, long long b) {
    if (a != b) {
        push_mismatch(out, path,
                      std::to_string(a) + " != " + std::to_string(b));
    }
}

}  // namespace

SummaryRow snapshot(const core::GameState& state) {
    SummaryRow row;
    row.date          = state.current_date;
    row.country_count = state.countries.size();
    row.log_count     = state.logs.size();
    row.seed          = state.rng.seed;
    return row;
}

void write_csv_header(std::ostream& out) {
    // Pinned by tests. If you change this header, expect the existing
    // determinism / format tests to fail loudly - bump a `summary_csv`
    // version note in docs/m0-10-diagnostics.md before doing so.
    out << "date,country_count,log_count,seed\n";
}

void write_csv_row(std::ostream& out, const SummaryRow& row) {
    out << row.date.to_string() << ','
        << row.country_count    << ','
        << row.log_count        << ','
        << row.seed
        << '\n';
}

// ---------------------------------------------------------------------------
// M1.14: per-country snapshot.
// ---------------------------------------------------------------------------

core::Result<CountrySummaryRow> country_snapshot(const core::GameState& state,
                                                 core::CountryId country) {
    if (!country.valid() ||
        country.value() < 0 ||
        static_cast<std::size_t>(country.value()) >= state.countries.size()) {
        return core::Result<CountrySummaryRow>::failure(
            "diagnostics::country_snapshot: country CountryId " +
            std::to_string(country.value()) +
            " is not a valid index into state.countries");
    }
    const auto& c = state.countries[static_cast<std::size_t>(country.value())];

    CountrySummaryRow row;
    row.date                 = state.current_date;
    row.id_code              = c.id_code;
    row.gdp                  = c.gdp;
    row.tax_revenue          = c.tax_revenue;
    row.budget_balance       = c.budget_balance;
    row.stability            = c.stability;
    row.legitimacy           = c.legitimacy;
    row.last_gdp_growth_rate = c.last_gdp_growth_rate;
    return core::Result<CountrySummaryRow>::success(std::move(row));
}

void write_country_csv_header(std::ostream& out) {
    // Pinned by tests. Bumping a column here is breaking.
    out << "date,id_code,gdp,tax_revenue,budget_balance,"
           "stability,legitimacy,last_gdp_growth_rate\n";
}

void write_country_csv_row(std::ostream& out, const CountrySummaryRow& row) {
    out << row.date.to_string() << ','
        << row.id_code          << ','
        << fmt_double(row.gdp)                  << ','
        << fmt_double(row.tax_revenue)          << ','
        << fmt_double(row.budget_balance)       << ','
        << fmt_double(row.stability)            << ','
        << fmt_double(row.legitimacy)           << ','
        << fmt_double(row.last_gdp_growth_rate)
        << '\n';
}

// ---------------------------------------------------------------------------
// M1.16: per-faction snapshot.
// ---------------------------------------------------------------------------

core::Result<FactionSummaryRow> faction_snapshot(const core::GameState& state,
                                                 core::FactionId faction) {
    if (!faction.valid() ||
        faction.value() < 0 ||
        static_cast<std::size_t>(faction.value()) >= state.factions.size()) {
        return core::Result<FactionSummaryRow>::failure(
            "diagnostics::faction_snapshot: faction FactionId " +
            std::to_string(faction.value()) +
            " is not a valid index into state.factions");
    }
    const auto& f = state.factions[static_cast<std::size_t>(faction.value())];

    FactionSummaryRow row;
    row.date            = state.current_date;
    row.id_code         = f.id_code;
    row.country_id_code = f.country_id_code;
    row.type            = f.type;
    row.support         = f.support;
    row.influence       = f.influence;
    row.radicalism      = f.radicalism;
    row.loyalty         = f.loyalty;
    row.resources       = f.resources;
    return core::Result<FactionSummaryRow>::success(std::move(row));
}

void write_faction_csv_header(std::ostream& out) {
    // Pinned by tests. Bumping a column here is breaking.
    out << "date,id_code,country_id_code,type,support,influence,"
           "radicalism,loyalty,resources\n";
}

void write_faction_csv_row(std::ostream& out, const FactionSummaryRow& row) {
    out << row.date.to_string() << ','
        << row.id_code          << ','
        << row.country_id_code  << ','
        << row.type             << ','
        << fmt_double(row.support)    << ','
        << fmt_double(row.influence)  << ','
        << fmt_double(row.radicalism) << ','
        << fmt_double(row.loyalty)    << ','
        << fmt_double(row.resources)
        << '\n';
}

// ---------------------------------------------------------------------------
// M3.5: per-interest-group snapshot.
// ---------------------------------------------------------------------------

std::string csv_escape(std::string_view field) {
    bool needs_quoting = false;
    for (const char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quoting = true;
            break;
        }
    }
    if (!needs_quoting) {
        return std::string(field);
    }
    std::string out;
    out.reserve(field.size() + 2);
    out.push_back('"');
    for (const char c : field) {
        out.push_back(c);
        if (c == '"') {
            out.push_back('"');  // RFC 4180: embedded `"` doubled.
        }
    }
    out.push_back('"');
    return out;
}

core::Result<InterestGroupSummaryRow> interest_group_snapshot(
        const core::GameState& state, std::size_t group_index) {
    if (group_index >= state.interest_groups.size()) {
        return core::Result<InterestGroupSummaryRow>::failure(
            "diagnostics::interest_group_snapshot: index " +
            std::to_string(group_index) +
            " is not a valid index into state.interest_groups (size " +
            std::to_string(state.interest_groups.size()) + ")");
    }
    const auto& g = state.interest_groups[group_index];

    // The country handle must resolve into state.countries. Bad data
    // here is almost always a scenario / save bug — fail loudly rather
    // than silently emit an empty `country_id_code` that future tooling
    // would have to guess at.
    if (!g.country.valid() ||
        g.country.value() < 0 ||
        static_cast<std::size_t>(g.country.value()) >= state.countries.size()) {
        return core::Result<InterestGroupSummaryRow>::failure(
            "diagnostics::interest_group_snapshot: interest_groups[" +
            std::to_string(group_index) + "] '" + g.id_code +
            "' references invalid country CountryId " +
            std::to_string(g.country.value()) +
            " (state.countries size is " +
            std::to_string(state.countries.size()) + ")");
    }

    InterestGroupSummaryRow row;
    row.date            = state.current_date;
    row.id_code         = g.id_code;
    row.name            = g.name;
    row.kind            = core::interest_group_kind_to_string(g.kind);
    row.country_id      = g.country.value();
    row.country_id_code =
        state.countries[static_cast<std::size_t>(g.country.value())].id_code;
    row.influence       = g.influence;
    row.loyalty         = g.loyalty;
    row.radicalism      = g.radicalism;
    return core::Result<InterestGroupSummaryRow>::success(std::move(row));
}

void write_interest_group_csv_header(std::ostream& out) {
    // Pinned by tests. Bumping a column here is breaking.
    out << "date,id_code,name,kind,country_id,country_id_code,"
           "influence,loyalty,radicalism\n";
}

void write_interest_group_csv_row(std::ostream& out,
                                  const InterestGroupSummaryRow& row) {
    out << row.date.to_string()         << ','
        << csv_escape(row.id_code)      << ','
        << csv_escape(row.name)         << ','
        << csv_escape(row.kind)         << ','
        << row.country_id               << ','
        << csv_escape(row.country_id_code) << ','
        << fmt_double(row.influence)    << ','
        << fmt_double(row.loyalty)      << ','
        << fmt_double(row.radicalism)
        << '\n';
}

// ---------------------------------------------------------------------------
// M3.6: per-system formula trace CSV writers.
// ---------------------------------------------------------------------------

void write_country_feedback_csv_header(std::ostream& out) {
    // Pinned by tests. Bumping a column is breaking.
    out << "date,country_id,country_id_code,matched_groups,"
           "weight_sum,weighted_radicalism,target_stability,"
           "stability_before,stability_after,stability_delta\n";
}

void write_country_feedback_csv_row(
        std::ostream& out,
        const interest_group::CountryFeedbackTraceRow& row) {
    out << row.date.to_string()             << ','
        << row.country_id                   << ','
        << csv_escape(row.country_id_code)  << ','
        << row.matched_groups               << ','
        << fmt_double(row.weight_sum)               << ','
        << fmt_double(row.weighted_radicalism)      << ','
        << fmt_double(row.target_stability)         << ','
        << fmt_double(row.stability_before)         << ','
        << fmt_double(row.stability_after)          << ','
        << fmt_double(row.stability_delta)
        << '\n';
}

void write_authority_pressure_csv_header(std::ostream& out) {
    // Pinned by tests. Bumping a column is breaking.
    out << "date,country_id,country_id_code,matched_groups,"
           "weight_sum,weighted_bureaucracy_loyalty,"
           "target_bureaucratic_compliance,"
           "bureaucratic_compliance_before,"
           "bureaucratic_compliance_after,"
           "bureaucratic_compliance_delta\n";
}

void write_authority_pressure_csv_row(
        std::ostream& out,
        const interest_group::AuthorityPressureTraceRow& row) {
    out << row.date.to_string()             << ','
        << row.country_id                   << ','
        << csv_escape(row.country_id_code)  << ','
        << row.matched_groups               << ','
        << fmt_double(row.weight_sum)                       << ','
        << fmt_double(row.weighted_bureaucracy_loyalty)     << ','
        << fmt_double(row.target_bureaucratic_compliance)   << ','
        << fmt_double(row.bureaucratic_compliance_before)   << ','
        << fmt_double(row.bureaucratic_compliance_after)    << ','
        << fmt_double(row.bureaucratic_compliance_delta)
        << '\n';
}

std::vector<Issue> sanity_check(const core::GameState& state) {
    std::vector<Issue> issues;

    if (!state.current_date.is_valid()) {
        Issue iss;
        iss.severity = Severity::Error;
        iss.code     = "invalid_date";
        iss.message  = "GameState.current_date is not a valid Gregorian date: " +
                       state.current_date.to_string();
        issues.push_back(std::move(iss));
    }

    // Scan countries. We do this in one pass so the "invalid id" issue
    // appears (in order) before any "duplicate id" issue that involves
    // the same country, which gives the most useful diagnostic output
    // when both are wrong.
    std::set<core::CountryId::underlying_type> seen;
    std::set<core::CountryId::underlying_type> already_reported_dup;

    for (std::size_t i = 0; i < state.countries.size(); ++i) {
        const auto& c = state.countries[i];
        if (!c.id.valid()) {
            Issue iss;
            iss.severity = Severity::Error;
            iss.code     = "invalid_country_id";
            iss.message  = "countries[" + std::to_string(i) +
                           "] has an invalid CountryId";
            issues.push_back(std::move(iss));
            continue;  // skip the duplicate check for an unset id
        }
        const auto v = c.id.value();
        if (seen.count(v) != 0 && already_reported_dup.count(v) == 0) {
            Issue iss;
            iss.severity = Severity::Error;
            iss.code     = "duplicate_country_id";
            iss.message  = "CountryId " + std::to_string(v) +
                           " appears more than once";
            issues.push_back(std::move(iss));
            already_reported_dup.insert(v);
        }
        seen.insert(v);
    }

    return issues;
}

// ---------------------------------------------------------------------------
// M2.10: compare_states
// ---------------------------------------------------------------------------

std::vector<StateMismatch> compare_states(const core::GameState& a,
                                          const core::GameState& b,
                                          const CompareOptions& opts) {
    std::vector<StateMismatch> out;
    const double tol = opts.double_tolerance;

    // ---- current_date ----------------------------------------------
    if (a.current_date != b.current_date) {
        push_mismatch(out, "current_date",
                      "'" + a.current_date.to_string() + "' != '" +
                      b.current_date.to_string() + "'");
    }

    // ---- player_country --------------------------------------------
    check_int(out, "player_country",
              a.player_country.value(),
              b.player_country.value());

    // ---- countries -------------------------------------------------
    if (a.countries.size() != b.countries.size()) {
        push_mismatch(out, "countries.size()",
                      std::to_string(a.countries.size()) + " != " +
                      std::to_string(b.countries.size()));
    } else {
        for (std::size_t i = 0; i < a.countries.size(); ++i) {
            const auto& ca = a.countries[i];
            const auto& cb = b.countries[i];
            const std::string prefix = "countries[" + std::to_string(i) + "]";

            check_string(out, prefix + ".id_code",      ca.id_code,      cb.id_code);
            check_string(out, prefix + ".name",         ca.name,         cb.name);
            check_string(out, prefix + ".display_name", ca.display_name, cb.display_name);

            check_double(out, prefix + ".gdp",                       ca.gdp,                       cb.gdp,                       tol);
            check_double(out, prefix + ".tax_revenue",               ca.tax_revenue,               cb.tax_revenue,               tol);
            check_double(out, prefix + ".budget_balance",            ca.budget_balance,            cb.budget_balance,            tol);
            check_double(out, prefix + ".legal_tax_burden",          ca.legal_tax_burden,          cb.legal_tax_burden,          tol);
            check_double(out, prefix + ".fiscal_capacity",           ca.fiscal_capacity,           cb.fiscal_capacity,           tol);
            check_double(out, prefix + ".administrative_efficiency", ca.administrative_efficiency, cb.administrative_efficiency, tol);
            check_double(out, prefix + ".central_control",           ca.central_control,           cb.central_control,           tol);
            check_double(out, prefix + ".corruption",                ca.corruption,                cb.corruption,                tol);
            check_double(out, prefix + ".stability",                 ca.stability,                 cb.stability,                 tol);
            check_double(out, prefix + ".legitimacy",                ca.legitimacy,                cb.legitimacy,                tol);
            check_double(out, prefix + ".military_power",            ca.military_power,            cb.military_power,            tol);
            check_double(out, prefix + ".threat_perception",         ca.threat_perception,         cb.threat_perception,         tol);
            check_double(out, prefix + ".last_gdp_growth_rate",      ca.last_gdp_growth_rate,      cb.last_gdp_growth_rate,      tol);

            check_double(out, prefix + ".budget.administration", ca.budget.administration, cb.budget.administration, tol);
            check_double(out, prefix + ".budget.military",       ca.budget.military,       cb.budget.military,       tol);
            check_double(out, prefix + ".budget.education",      ca.budget.education,      cb.budget.education,      tol);
            check_double(out, prefix + ".budget.welfare",        ca.budget.welfare,        cb.budget.welfare,        tol);
            check_double(out, prefix + ".budget.intelligence",   ca.budget.intelligence,   cb.budget.intelligence,   tol);
            check_double(out, prefix + ".budget.infrastructure", ca.budget.infrastructure, cb.budget.infrastructure, tol);
            check_double(out, prefix + ".budget.industry",       ca.budget.industry,       cb.budget.industry,       tol);

            // M2.16 government_authority block. Field paths mirror
            // the save JSON shape so the same string works in CLI
            // output and reviewer-friendly error messages.
            check_double(out, prefix + ".government_authority.bureaucratic_compliance",
                         ca.government_authority.bureaucratic_compliance,
                         cb.government_authority.bureaucratic_compliance, tol);
            check_double(out, prefix + ".government_authority.military_loyalty",
                         ca.government_authority.military_loyalty,
                         cb.government_authority.military_loyalty, tol);
            check_double(out, prefix + ".government_authority.intelligence_capability",
                         ca.government_authority.intelligence_capability,
                         cb.government_authority.intelligence_capability, tol);
            check_double(out, prefix + ".government_authority.media_control",
                         ca.government_authority.media_control,
                         cb.government_authority.media_control, tol);

            // active_policies block
            if (ca.active_policies.size() != cb.active_policies.size()) {
                push_mismatch(out, prefix + ".active_policies.size()",
                              std::to_string(ca.active_policies.size()) + " != " +
                              std::to_string(cb.active_policies.size()));
            } else {
                for (std::size_t k = 0; k < ca.active_policies.size(); ++k) {
                    const auto& aa = ca.active_policies[k];
                    const auto& bb = cb.active_policies[k];
                    const std::string ap_prefix =
                        prefix + ".active_policies[" + std::to_string(k) + "]";
                    check_string(out, ap_prefix + ".policy_id_code",
                                 aa.policy_id_code, bb.policy_id_code);
                    if (aa.expires_on != bb.expires_on) {
                        push_mismatch(out, ap_prefix + ".expires_on",
                                      "'" + aa.expires_on.to_string() + "' != '" +
                                      bb.expires_on.to_string() + "'");
                    }
                }
            }
        }
    }

    // ---- factions --------------------------------------------------
    if (a.factions.size() != b.factions.size()) {
        push_mismatch(out, "factions.size()",
                      std::to_string(a.factions.size()) + " != " +
                      std::to_string(b.factions.size()));
    } else {
        for (std::size_t i = 0; i < a.factions.size(); ++i) {
            const auto& fa = a.factions[i];
            const auto& fb = b.factions[i];
            const std::string prefix = "factions[" + std::to_string(i) + "]";

            check_string(out, prefix + ".id_code",         fa.id_code,         fb.id_code);
            check_string(out, prefix + ".country_id_code", fa.country_id_code, fb.country_id_code);
            check_string(out, prefix + ".type",            fa.type,            fb.type);

            check_double(out, prefix + ".support",    fa.support,    fb.support,    tol);
            check_double(out, prefix + ".influence",  fa.influence,  fb.influence,  tol);
            check_double(out, prefix + ".radicalism", fa.radicalism, fb.radicalism, tol);
            check_double(out, prefix + ".loyalty",    fa.loyalty,    fb.loyalty,    tol);
            check_double(out, prefix + ".resources",  fa.resources,  fb.resources,  tol);

            check_int(out, prefix + ".preferred_policies.size()",
                      static_cast<long long>(fa.preferred_policies.size()),
                      static_cast<long long>(fb.preferred_policies.size()));
        }
    }

    // ---- applied_commands ------------------------------------------
    if (a.applied_commands.size() != b.applied_commands.size()) {
        push_mismatch(out, "applied_commands.size()",
                      std::to_string(a.applied_commands.size()) + " != " +
                      std::to_string(b.applied_commands.size()));
    } else {
        for (std::size_t i = 0; i < a.applied_commands.size(); ++i) {
            const auto& ea = a.applied_commands[i];
            const auto& eb = b.applied_commands[i];
            const std::string prefix =
                "applied_commands[" + std::to_string(i) + "]";

            if (ea.applied_on != eb.applied_on) {
                push_mismatch(out, prefix + ".applied_on",
                              "'" + ea.applied_on.to_string() + "' != '" +
                              eb.applied_on.to_string() + "'");
            }
            check_int(out, prefix + ".command.kind",
                      static_cast<long long>(ea.command.kind),
                      static_cast<long long>(eb.command.kind));
            // Compare both possible payloads; an EnactPolicy entry has
            // unused AdjustBudget fields and vice-versa, but since both
            // states should mirror, the strings/values match by default.
            check_string(out, prefix + ".command.policy_id_code",
                         ea.command.policy_id_code, eb.command.policy_id_code);
            check_string(out, prefix + ".command.budget_category",
                         ea.command.budget_category, eb.command.budget_category);
            check_double(out, prefix + ".command.budget_delta",
                         ea.command.budget_delta, eb.command.budget_delta, tol);
        }
    }

    // ---- interest_groups (M3.1) ------------------------------------
    if (a.interest_groups.size() != b.interest_groups.size()) {
        push_mismatch(out, "interest_groups.size()",
                      std::to_string(a.interest_groups.size()) + " != " +
                      std::to_string(b.interest_groups.size()));
    } else {
        for (std::size_t i = 0; i < a.interest_groups.size(); ++i) {
            const auto& ga = a.interest_groups[i];
            const auto& gb = b.interest_groups[i];
            const std::string prefix =
                "interest_groups[" + std::to_string(i) + "]";

            check_string(out, prefix + ".id_code", ga.id_code, gb.id_code);
            check_string(out, prefix + ".name",    ga.name,    gb.name);
            check_int(out, prefix + ".kind",
                      static_cast<long long>(ga.kind),
                      static_cast<long long>(gb.kind));
            check_int(out, prefix + ".country",
                      static_cast<long long>(ga.country.value()),
                      static_cast<long long>(gb.country.value()));
            check_double(out, prefix + ".influence",  ga.influence,  gb.influence,  tol);
            check_double(out, prefix + ".loyalty",    ga.loyalty,    gb.loyalty,    tol);
            check_double(out, prefix + ".radicalism", ga.radicalism, gb.radicalism, tol);
        }
    }

    // ---- provinces (M4.1) ------------------------------------------
    if (a.provinces.size() != b.provinces.size()) {
        push_mismatch(out, "provinces.size()",
                      std::to_string(a.provinces.size()) + " != " +
                      std::to_string(b.provinces.size()));
    } else {
        for (std::size_t i = 0; i < a.provinces.size(); ++i) {
            const auto& pa = a.provinces[i];
            const auto& pb = b.provinces[i];
            const std::string prefix =
                "provinces[" + std::to_string(i) + "]";

            check_string(out, prefix + ".id_code", pa.id_code, pb.id_code);
            check_string(out, prefix + ".name",    pa.name,    pb.name);
            check_int(out, prefix + ".owner",
                      static_cast<long long>(pa.owner.value()),
                      static_cast<long long>(pb.owner.value()));
            check_double(out, prefix + ".x", pa.x, pb.x, tol);
            check_double(out, prefix + ".y", pa.y, pb.y, tol);
        }
    }

    // ---- events (M5.1) ---------------------------------------------
    // Walks state.events (typed EventDefinition since M5.1; was the
    // M0 stub before). Per-entry: id_code, name, description,
    // triggers[].(target/op/value), effects[].(target/op/value).
    // Mirrors the M3.1 interest_groups / M4.1 provinces shape.
    if (a.events.size() != b.events.size()) {
        push_mismatch(out, "events.size()",
                      std::to_string(a.events.size()) + " != " +
                      std::to_string(b.events.size()));
    } else {
        for (std::size_t i = 0; i < a.events.size(); ++i) {
            const auto& ea = a.events[i];
            const auto& eb = b.events[i];
            const std::string prefix =
                "events[" + std::to_string(i) + "]";

            check_string(out, prefix + ".id_code",
                         ea.id_code, eb.id_code);
            check_string(out, prefix + ".name", ea.name, eb.name);
            check_string(out, prefix + ".description",
                         ea.description, eb.description);

            if (ea.triggers.size() != eb.triggers.size()) {
                push_mismatch(out, prefix + ".triggers.size()",
                              std::to_string(ea.triggers.size()) + " != " +
                              std::to_string(eb.triggers.size()));
            } else {
                for (std::size_t ti = 0; ti < ea.triggers.size(); ++ti) {
                    const auto& ta = ea.triggers[ti];
                    const auto& tb = eb.triggers[ti];
                    const std::string tprefix =
                        prefix + ".triggers[" + std::to_string(ti) + "]";
                    check_string(out, tprefix + ".target",
                                 ta.target, tb.target);
                    check_string(out, tprefix + ".op", ta.op, tb.op);
                    check_double(out, tprefix + ".value",
                                 ta.value, tb.value, tol);
                }
            }

            if (ea.effects.size() != eb.effects.size()) {
                push_mismatch(out, prefix + ".effects.size()",
                              std::to_string(ea.effects.size()) + " != " +
                              std::to_string(eb.effects.size()));
            } else {
                for (std::size_t ei = 0; ei < ea.effects.size(); ++ei) {
                    const auto& fa = ea.effects[ei];
                    const auto& fb = eb.effects[ei];
                    const std::string eprefix =
                        prefix + ".effects[" + std::to_string(ei) + "]";
                    check_string(out, eprefix + ".target",
                                 fa.target, fb.target);
                    check_string(out, eprefix + ".op", fa.op, fb.op);
                    check_double(out, eprefix + ".value",
                                 fa.value, fb.value, tol);
                }
            }
        }
    }

    return out;
}

}  // namespace leviathan::systems::diagnostics
