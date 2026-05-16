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
        if (!v->is_number_unsigned() && !v->is_number_integer()) {
            return core::Result<core::SimulationConfig>::failure(
                fmt_err(source_label,
                        "'simulation.seed' has wrong type (expected unsigned integer)"));
        }
        const auto seed_signed = v->get<std::int64_t>();
        if (seed_signed < 0) {
            return core::Result<core::SimulationConfig>::failure(
                fmt_err(source_label, "'simulation.seed' is negative"));
        }
        cfg.seed = static_cast<std::uint64_t>(seed_signed);
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

    auto gdp = require_number(root, "initial_gdp", source_label);
    if (!gdp) {
        return core::Result<core::CountryState>::failure(
            std::move(gdp.error()));
    }
    country.initial_gdp = gdp.value();

    auto stab = require_number(root, "initial_stability", source_label);
    if (!stab) {
        return core::Result<core::CountryState>::failure(
            std::move(stab.error()));
    }
    country.initial_stability = stab.value();

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

}  // namespace leviathan::systems::data_loader
