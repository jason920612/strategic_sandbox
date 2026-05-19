#include "leviathan/systems/annual_stats.hpp"

#include <iomanip>
#include <sstream>
#include <string>

#include "internal/numeric_guards.hpp"

namespace leviathan::systems::annual_stats {

namespace ng = leviathan::systems::detail;

namespace {

constexpr const char* kModule = "annual_stats::snapshot";

std::string format_double(double v) {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(17) << v;
    return oss.str();
}

}  // namespace

core::Result<AnnualRow> snapshot(const core::GameState& state, int year) {
    AnnualRow row;
    row.year                = year;
    row.country_count       = state.countries.size();
    row.event_history_count = state.event_history.size();

    // Post-M6.7 strict-fallback hardening: empty state.countries is
    // now a hard failure rather than a zero-row success. The runner
    // gates the year-boundary call so empty-world simulations skip
    // emission entirely.
    if (state.countries.empty()) {
        return core::Result<AnnualRow>::failure(
            std::string(kModule) +
            ": state.countries is empty (year " +
            std::to_string(year) +
            ") — empty-world simulations should not emit annual"
            " stats; the runner is expected to skip the call");
    }

    double sum_stability  = 0.0;
    double sum_legitimacy = 0.0;
    double sum_gdp        = 0.0;
    double sum_corruption = 0.0;
    for (const auto& c : state.countries) {
        if (auto err = ng::require_unit_ratio<AnnualRow>(
                kModule, "country", c.id_code,
                "country.stability", c.stability)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<AnnualRow>(
                kModule, "country", c.id_code,
                "country.legitimacy", c.legitimacy)) {
            return *err;
        }
        if (auto err = ng::require_unit_ratio<AnnualRow>(
                kModule, "country", c.id_code,
                "country.corruption", c.corruption)) {
            return *err;
        }
        if (auto err = ng::require_finite_double<AnnualRow>(
                kModule, "country", c.id_code,
                "country.gdp", c.gdp)) {
            return *err;
        }
        sum_stability  += c.stability;
        sum_legitimacy += c.legitimacy;
        sum_gdp        += c.gdp;
        sum_corruption += c.corruption;
    }
    const double n = static_cast<double>(state.countries.size());
    row.avg_stability   = sum_stability  / n;
    row.avg_legitimacy  = sum_legitimacy / n;
    row.avg_gdp         = sum_gdp        / n;
    row.avg_corruption  = sum_corruption / n;
    row.total_gdp       = sum_gdp;
    return core::Result<AnnualRow>::success(row);
}

std::string write_csv_header() {
    return "date,year,country_count,avg_stability,avg_legitimacy,"
           "avg_gdp,avg_corruption,total_gdp,event_history_count\n";
}

std::string write_csv_row(const core::GameDate& snapshot_date,
                          const AnnualRow&      row) {
    std::ostringstream oss;
    oss << snapshot_date.to_string() << ','
        << row.year << ','
        << row.country_count << ','
        << format_double(row.avg_stability)  << ','
        << format_double(row.avg_legitimacy) << ','
        << format_double(row.avg_gdp)        << ','
        << format_double(row.avg_corruption) << ','
        << format_double(row.total_gdp)      << ','
        << row.event_history_count << '\n';
    return oss.str();
}

}  // namespace leviathan::systems::annual_stats
