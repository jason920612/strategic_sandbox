#include "leviathan/systems/data_loader.hpp"

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "leviathan/core/game_date.hpp"
#include "leviathan/core/result.hpp"

namespace leviathan::systems::data_loader {

namespace {

using nlohmann::json;

std::string fmt_err(std::string_view source, std::string_view msg) {
    std::string out;
    out.reserve(source.size() + msg.size() + 2);
    out.append(source.data(), source.size());
    out.append(": ");
    out.append(msg.data(), msg.size());
    return out;
}

const json* navigate(const json& root, std::string_view dotted_path) {
    // Walks a dotted path like "simulation.start_date". Returns null
    // if any segment is missing or any intermediate value is not an
    // object.
    const json* cur = &root;
    std::size_t pos = 0;
    while (pos <= dotted_path.size()) {
        const std::size_t dot = dotted_path.find('.', pos);
        const std::size_t end = (dot == std::string_view::npos)
                                ? dotted_path.size()
                                : dot;
        const std::string segment(dotted_path.substr(pos, end - pos));
        if (!cur->is_object()) return nullptr;
        if (!cur->contains(segment)) return nullptr;
        cur = &cur->at(segment);
        if (dot == std::string_view::npos) break;
        pos = dot + 1;
    }
    return cur;
}

core::Result<std::string> require_string(const json& root,
                                         std::string_view path,
                                         std::string_view source) {
    const json* v = navigate(root, path);
    if (v == nullptr) {
        std::string msg = "missing required field '";
        msg.append(path.data(), path.size());
        msg += "'";
        return core::Result<std::string>::failure(fmt_err(source, msg));
    }
    if (!v->is_string()) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' has wrong type (expected string)";
        return core::Result<std::string>::failure(fmt_err(source, msg));
    }
    return core::Result<std::string>::success(v->get<std::string>());
}

core::Result<double> require_number(const json& root,
                                    std::string_view path,
                                    std::string_view source) {
    const json* v = navigate(root, path);
    if (v == nullptr) {
        std::string msg = "missing required field '";
        msg.append(path.data(), path.size());
        msg += "'";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    if (!v->is_number()) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' has wrong type (expected number)";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    const double d = v->get<double>();
    if (!std::isfinite(d)) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' is not finite";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    return core::Result<double>::success(d);
}

// Returns the number iff it parses as finite AND is >= 0. Used for
// quantities like GDP that have a physical floor of zero.
core::Result<double> require_nonneg_number(const json& root,
                                           std::string_view path,
                                           std::string_view source) {
    auto n = require_number(root, path, source);
    if (!n) return core::Result<double>::failure(std::move(n.error()));
    if (n.value() < 0.0) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' must be >= 0 (got ";
        msg += std::to_string(n.value());
        msg += ")";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    return n;
}

// Returns the number iff it parses as finite AND is in [0, 1]. Used for
// the many M1.1 ratio fields (stability, legitimacy, corruption, ...).
core::Result<double> require_ratio(const json& root,
                                   std::string_view path,
                                   std::string_view source) {
    auto n = require_number(root, path, source);
    if (!n) return core::Result<double>::failure(std::move(n.error()));
    if (n.value() < 0.0 || n.value() > 1.0) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' must be in [0, 1] (got ";
        msg += std::to_string(n.value());
        msg += ")";
        return core::Result<double>::failure(fmt_err(source, msg));
    }
    return n;
}

core::Result<core::GameDate> require_date(const json& root,
                                          std::string_view path,
                                          std::string_view source) {
    auto str_result = require_string(root, path, source);
    if (!str_result) {
        return core::Result<core::GameDate>::failure(
            std::move(str_result.error()));
    }
    auto date_result = core::GameDate::parse(str_result.value());
    if (!date_result) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' = \"";
        msg += str_result.value();
        msg += "\" is not a real Gregorian date";
        return core::Result<core::GameDate>::failure(fmt_err(source, msg));
    }
    return core::Result<core::GameDate>::success(date_result.value());
}

core::Result<std::string> read_whole_file(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return core::Result<std::string>::failure(
            path.string() + ": cannot open file for reading");
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return core::Result<std::string>::success(ss.str());
}

}  // namespace

core::Result<core::SimulationConfig> parse_simulation_config(
        std::string_view json_text,
        std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false,
                            /*ignore_comments=*/false);
    if (root.is_discarded()) {
        return core::Result<core::SimulationConfig>::failure(
            fmt_err(source_label, "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<core::SimulationConfig>::failure(
            fmt_err(source_label, "top-level JSON value is not an object"));
    }

    core::SimulationConfig cfg;

    auto start = require_date(root, "simulation.start_date", source_label);
    if (!start) {
        return core::Result<core::SimulationConfig>::failure(
            std::move(start.error()));
    }
    cfg.start_date = start.value();

    // end_date, seed, daily_tick are optional - keep struct defaults if
    // absent.
    if (navigate(root, "simulation.end_date") != nullptr) {
        auto end = require_date(root, "simulation.end_date", source_label);
        if (!end) {
            return core::Result<core::SimulationConfig>::failure(
                std::move(end.error()));
        }
        cfg.end_date = end.value();
    }

    if (const json* v = navigate(root, "simulation.seed"); v != nullptr) {
        if (v->is_number_unsigned()) {
            // The full uint64_t range is valid - take the raw value.
            // This branch covers seeds in (INT64_MAX, UINT64_MAX] that
            // would silently truncate if forced through int64_t.
            cfg.seed = v->get<std::uint64_t>();
        } else if (v->is_number_integer()) {
            // Signed integer literal; permit it only if non-negative.
            const auto seed_signed = v->get<std::int64_t>();
            if (seed_signed < 0) {
                return core::Result<core::SimulationConfig>::failure(
                    fmt_err(source_label, "'simulation.seed' is negative"));
            }
            cfg.seed = static_cast<std::uint64_t>(seed_signed);
        } else {
            return core::Result<core::SimulationConfig>::failure(
                fmt_err(source_label,
                        "'simulation.seed' has wrong type (expected unsigned integer)"));
        }
    }

    if (const json* v = navigate(root, "simulation.daily_tick"); v != nullptr) {
        if (!v->is_boolean()) {
            return core::Result<core::SimulationConfig>::failure(
                fmt_err(source_label,
                        "'simulation.daily_tick' has wrong type (expected boolean)"));
        }
        cfg.daily_tick = v->get<bool>();
    }

    return core::Result<core::SimulationConfig>::success(std::move(cfg));
}

core::Result<core::SimulationConfig> load_simulation_config(
        const std::filesystem::path& path) {
    auto text_result = read_whole_file(path);
    if (!text_result) {
        return core::Result<core::SimulationConfig>::failure(
            std::move(text_result.error()));
    }
    return parse_simulation_config(text_result.value(), path.string());
}

core::Result<core::CountryState> parse_country(
        std::string_view json_text,
        std::string_view source_label) {
    json root = json::parse(json_text, nullptr, /*allow_exceptions=*/false);
    if (root.is_discarded()) {
        return core::Result<core::CountryState>::failure(
            fmt_err(source_label, "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<core::CountryState>::failure(
            fmt_err(source_label, "top-level JSON value is not an object"));
    }

    core::CountryState country;

    auto id_code = require_string(root, "id", source_label);
    if (!id_code) {
        return core::Result<core::CountryState>::failure(
            std::move(id_code.error()));
    }
    country.id_code = id_code.value();

    auto name = require_string(root, "name", source_label);
    if (!name) {
        return core::Result<core::CountryState>::failure(
            std::move(name.error()));
    }
    country.name = name.value();

    // display_name defaults to name when absent.
    if (navigate(root, "display_name") != nullptr) {
        auto dn = require_string(root, "display_name", source_label);
        if (!dn) {
            return core::Result<core::CountryState>::failure(
                std::move(dn.error()));
        }
        country.display_name = dn.value();
    } else {
        country.display_name = country.name;
    }

    // ---- absolute economic baseline ---------------------------------
    auto gdp = require_nonneg_number(root, "initial_gdp", source_label);
    if (!gdp) {
        return core::Result<core::CountryState>::failure(std::move(gdp.error()));
    }
    country.gdp = gdp.value();   // initial_gdp -> runtime gdp

    // ---- ratio fields (0..1) ----------------------------------------
    // Each field is REQUIRED. M1.1 prioritises data quality over
    // forgiving defaults, so missing fields produce clear errors
    // rather than silent zeroes.
    auto stab = require_ratio(root, "initial_stability", source_label);
    if (!stab) {
        return core::Result<core::CountryState>::failure(std::move(stab.error()));
    }
    country.stability = stab.value();   // initial_stability -> runtime stability

    auto ltb = require_ratio(root, "legal_tax_burden", source_label);
    if (!ltb) {
        return core::Result<core::CountryState>::failure(std::move(ltb.error()));
    }
    country.legal_tax_burden = ltb.value();

    auto fc = require_ratio(root, "fiscal_capacity", source_label);
    if (!fc) {
        return core::Result<core::CountryState>::failure(std::move(fc.error()));
    }
    country.fiscal_capacity = fc.value();

    auto ae = require_ratio(root, "administrative_efficiency", source_label);
    if (!ae) {
        return core::Result<core::CountryState>::failure(std::move(ae.error()));
    }
    country.administrative_efficiency = ae.value();

    auto cc = require_ratio(root, "central_control", source_label);
    if (!cc) {
        return core::Result<core::CountryState>::failure(std::move(cc.error()));
    }
    country.central_control = cc.value();

    auto corr = require_ratio(root, "corruption", source_label);
    if (!corr) {
        return core::Result<core::CountryState>::failure(std::move(corr.error()));
    }
    country.corruption = corr.value();

    auto leg = require_ratio(root, "legitimacy", source_label);
    if (!leg) {
        return core::Result<core::CountryState>::failure(std::move(leg.error()));
    }
    country.legitimacy = leg.value();

    auto mp = require_ratio(root, "military_power", source_label);
    if (!mp) {
        return core::Result<core::CountryState>::failure(std::move(mp.error()));
    }
    country.military_power = mp.value();

    auto tp = require_ratio(root, "threat_perception", source_label);
    if (!tp) {
        return core::Result<core::CountryState>::failure(std::move(tp.error()));
    }
    country.threat_perception = tp.value();

    // tax_revenue and budget_balance are runtime-only. They are not
    // read from the config JSON; they start at 0 and will be updated
    // by future economy systems (M1.5+).

    // ---- M1.3 budget block -----------------------------------------
    // Required: the "budget" key must exist and be a JSON object.
    // Within it, every category is a required ratio in [0, 1].
    const json* budget_node = navigate(root, "budget");
    if (budget_node == nullptr) {
        return core::Result<core::CountryState>::failure(
            fmt_err(source_label, "missing required field 'budget'"));
    }
    if (!budget_node->is_object()) {
        return core::Result<core::CountryState>::failure(
            fmt_err(source_label,
                    "'budget' has wrong type (expected JSON object)"));
    }

    // Use a contextualised source so missing-category errors read as
    // "<src>: budget: missing required field 'military'".
    const std::string budget_ctx = std::string(source_label) + ": budget";

    auto read_cat = [&](const char* key, double& dst) -> core::Result<bool> {
        auto r = require_ratio(*budget_node, key, budget_ctx);
        if (!r) return core::Result<bool>::failure(std::move(r.error()));
        dst = r.value();
        return core::Result<bool>::success(true);
    };

    if (auto r = read_cat("administration", country.budget.administration); !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }
    if (auto r = read_cat("military",       country.budget.military);       !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }
    if (auto r = read_cat("education",      country.budget.education);      !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }
    if (auto r = read_cat("welfare",        country.budget.welfare);        !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }
    if (auto r = read_cat("intelligence",   country.budget.intelligence);   !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }
    if (auto r = read_cat("infrastructure", country.budget.infrastructure); !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }
    if (auto r = read_cat("industry",       country.budget.industry);       !r) {
        return core::Result<core::CountryState>::failure(std::move(r.error()));
    }

    return core::Result<core::CountryState>::success(std::move(country));
}

core::Result<core::CountryState> load_country(
        const std::filesystem::path& path) {
    auto text_result = read_whole_file(path);
    if (!text_result) {
        return core::Result<core::CountryState>::failure(
            std::move(text_result.error()));
    }
    return parse_country(text_result.value(), path.string());
}

// =========================================================================
// FactionState
// =========================================================================

core::Result<core::FactionState> parse_faction(
        std::string_view json_text,
        std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false);
    if (root.is_discarded()) {
        return core::Result<core::FactionState>::failure(
            fmt_err(source_label, "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<core::FactionState>::failure(
            fmt_err(source_label, "top-level JSON value is not an object"));
    }

    core::FactionState f;

    auto id_code = require_string(root, "id", source_label);
    if (!id_code) {
        return core::Result<core::FactionState>::failure(std::move(id_code.error()));
    }
    f.id_code = id_code.value();

    auto country = require_string(root, "country", source_label);
    if (!country) {
        return core::Result<core::FactionState>::failure(std::move(country.error()));
    }
    f.country_id_code = country.value();

    auto type = require_string(root, "type", source_label);
    if (!type) {
        return core::Result<core::FactionState>::failure(std::move(type.error()));
    }
    f.type = type.value();

    auto name = require_string(root, "name", source_label);
    if (!name) {
        return core::Result<core::FactionState>::failure(std::move(name.error()));
    }
    f.name = name.value();

    // Behavioural ratios
    auto support = require_ratio(root, "support", source_label);
    if (!support) {
        return core::Result<core::FactionState>::failure(std::move(support.error()));
    }
    f.support = support.value();

    auto influence = require_ratio(root, "influence", source_label);
    if (!influence) {
        return core::Result<core::FactionState>::failure(std::move(influence.error()));
    }
    f.influence = influence.value();

    auto radicalism = require_ratio(root, "radicalism", source_label);
    if (!radicalism) {
        return core::Result<core::FactionState>::failure(std::move(radicalism.error()));
    }
    f.radicalism = radicalism.value();

    auto loyalty = require_ratio(root, "loyalty", source_label);
    if (!loyalty) {
        return core::Result<core::FactionState>::failure(std::move(loyalty.error()));
    }
    f.loyalty = loyalty.value();

    // Resources are absolute and non-negative.
    auto resources = require_nonneg_number(root, "resources", source_label);
    if (!resources) {
        return core::Result<core::FactionState>::failure(std::move(resources.error()));
    }
    f.resources = resources.value();

    // preferred_policies is a required array of strings (may be empty).
    {
        const json* arr = navigate(root, "preferred_policies");
        if (arr == nullptr) {
            return core::Result<core::FactionState>::failure(
                fmt_err(source_label,
                        "missing required field 'preferred_policies'"));
        }
        if (!arr->is_array()) {
            return core::Result<core::FactionState>::failure(
                fmt_err(source_label,
                        "'preferred_policies' has wrong type (expected array of strings)"));
        }
        f.preferred_policies.reserve(arr->size());
        for (std::size_t i = 0; i < arr->size(); ++i) {
            if (!(*arr)[i].is_string()) {
                return core::Result<core::FactionState>::failure(
                    fmt_err(source_label,
                            "preferred_policies[" + std::to_string(i) +
                            "] is not a string"));
            }
            f.preferred_policies.push_back((*arr)[i].get<std::string>());
        }
    }

    return core::Result<core::FactionState>::success(std::move(f));
}

core::Result<core::FactionState> load_faction(
        const std::filesystem::path& path) {
    auto text_result = read_whole_file(path);
    if (!text_result) {
        return core::Result<core::FactionState>::failure(
            std::move(text_result.error()));
    }
    return parse_faction(text_result.value(), path.string());
}

}  // namespace leviathan::systems::data_loader
