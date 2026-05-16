#include "leviathan/systems/diagnostics.hpp"

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

}  // namespace leviathan::systems::diagnostics
