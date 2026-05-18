#include "leviathan/systems/annual_stats.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace leviathan::systems::annual_stats {

namespace {

std::string format_double(double v) {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(17) << v;
    return oss.str();
}

}  // namespace

AnnualRow snapshot(const core::GameState& state, int year) {
    AnnualRow row;
    row.year                = year;
    row.country_count       = state.countries.size();
    row.event_history_count = state.event_history.size();

    if (state.countries.empty()) {
        return row;
    }

    double sum_stability  = 0.0;
    double sum_legitimacy = 0.0;
    double sum_gdp        = 0.0;
    double sum_corruption = 0.0;
    for (const auto& c : state.countries) {
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
    return row;
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
