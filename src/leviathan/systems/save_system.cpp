#include "leviathan/systems/save_system.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "leviathan/core/entities.hpp"
#include "leviathan/core/game_date.hpp"
#include "leviathan/core/log_entry.hpp"

namespace leviathan::systems::save_system {

namespace {

// nlohmann::json (default) backs objects with std::map, which sorts
// keys alphabetically on dump. That breaks the M0.6 "metadata
// insertion order is byte-stable" invariant when we round-trip
// through a save file. ordered_json preserves insertion order both
// in object construction (used by serialize) and during parsing
// (used by deserialize, so the order in the file is what comes back
// out).
using json = nlohmann::ordered_json;

// ----- error formatting ----------------------------------------------------

std::string fmt_err(std::string_view source, std::string_view msg) {
    std::string out;
    out.reserve(source.size() + msg.size() + 2);
    out.append(source.data(), source.size());
    out.append(": ");
    out.append(msg.data(), msg.size());
    return out;
}

// ----- severity <-> string -------------------------------------------------

std::string severity_to_string(core::LogSeverity s) {
    switch (s) {
        case core::LogSeverity::Debug: return "debug";
        case core::LogSeverity::Info:  return "info";
        case core::LogSeverity::Warn:  return "warn";
        case core::LogSeverity::Error: return "error";
    }
    return "info";
}

core::Result<core::LogSeverity> severity_from_string(std::string_view s) {
    if (s == "debug") return core::Result<core::LogSeverity>::success(core::LogSeverity::Debug);
    if (s == "info")  return core::Result<core::LogSeverity>::success(core::LogSeverity::Info);
    if (s == "warn")  return core::Result<core::LogSeverity>::success(core::LogSeverity::Warn);
    if (s == "error") return core::Result<core::LogSeverity>::success(core::LogSeverity::Error);
    std::string msg = "unknown severity '";
    msg.append(s.data(), s.size());
    msg += "' (expected debug|info|warn|error)";
    return core::Result<core::LogSeverity>::failure(std::move(msg));
}

// ----- serialise -----------------------------------------------------------

json country_to_json(const core::CountryState& c) {
    // M1.1 schema. Field order is fixed; tests pin the on-disk shape.
    // The save format separates runtime state (gdp / stability /
    // tax_revenue / budget_balance) from the JSON config's "initial_*"
    // fields - saves always store runtime values.
    json j = json::object();
    j["id"]                        = c.id.value();
    j["id_code"]                   = c.id_code;
    j["name"]                      = c.name;
    j["display_name"]              = c.display_name;
    j["gdp"]                       = c.gdp;
    j["tax_revenue"]               = c.tax_revenue;
    j["budget_balance"]            = c.budget_balance;
    j["legal_tax_burden"]          = c.legal_tax_burden;
    j["fiscal_capacity"]           = c.fiscal_capacity;
    j["administrative_efficiency"] = c.administrative_efficiency;
    j["central_control"]           = c.central_control;
    j["corruption"]                = c.corruption;
    j["stability"]                 = c.stability;
    j["legitimacy"]                = c.legitimacy;
    j["military_power"]            = c.military_power;
    j["threat_perception"]         = c.threat_perception;
    return j;
}

json log_entry_to_json(const core::LogEntry& e) {
    json j = json::object();
    j["date"]     = e.date.to_string();
    j["category"] = e.category;
    j["severity"] = severity_to_string(e.severity);
    j["source"]   = e.source;
    j["message"]  = e.message;
    // `json` is ordered_json here, so insertion order survives dump.
    json md = json::object();
    for (const auto& [k, v] : e.metadata) {
        md[k] = v;
    }
    j["metadata"] = std::move(md);
    return j;
}

// ----- deserialise helpers -------------------------------------------------

const json* navigate(const json& root, std::string_view dotted_path) {
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

core::Result<std::uint64_t> require_u64(const json& root,
                                        std::string_view path,
                                        std::string_view source) {
    const json* v = navigate(root, path);
    if (v == nullptr) {
        std::string msg = "missing required field '";
        msg.append(path.data(), path.size());
        msg += "'";
        return core::Result<std::uint64_t>::failure(fmt_err(source, msg));
    }
    if (v->is_number_unsigned()) {
        return core::Result<std::uint64_t>::success(v->get<std::uint64_t>());
    }
    if (v->is_number_integer()) {
        const auto s = v->get<std::int64_t>();
        if (s < 0) {
            std::string msg = "'";
            msg.append(path.data(), path.size());
            msg += "' is negative";
            return core::Result<std::uint64_t>::failure(fmt_err(source, msg));
        }
        return core::Result<std::uint64_t>::success(static_cast<std::uint64_t>(s));
    }
    std::string msg = "'";
    msg.append(path.data(), path.size());
    msg += "' has wrong type (expected unsigned integer)";
    return core::Result<std::uint64_t>::failure(fmt_err(source, msg));
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
    return core::Result<double>::success(v->get<double>());
}

core::Result<core::GameDate> require_date(const json& root,
                                          std::string_view path,
                                          std::string_view source) {
    auto s = require_string(root, path, source);
    if (!s) return core::Result<core::GameDate>::failure(std::move(s.error()));
    auto d = core::GameDate::parse(s.value());
    if (!d) {
        std::string msg = "'";
        msg.append(path.data(), path.size());
        msg += "' = \"";
        msg += s.value();
        msg += "\" is not a real Gregorian date";
        return core::Result<core::GameDate>::failure(fmt_err(source, msg));
    }
    return core::Result<core::GameDate>::success(d.value());
}

core::Result<core::CountryState> country_from_json(const json& j,
                                                   std::size_t array_index,
                                                   std::string_view source) {
    // Build a contextualised source label so every child error names
    // the right array element. Sub-helpers see this as the source.
    const std::string ctx =
        std::string(source) + ": countries[" + std::to_string(array_index) + "]";

    if (!j.is_object()) {
        return core::Result<core::CountryState>::failure(
            ctx + ": expected JSON object");
    }

    core::CountryState c;

    auto id_value = require_u64(j, "id", ctx);
    if (!id_value) {
        return core::Result<core::CountryState>::failure(std::move(id_value.error()));
    }
    // CountryId::underlying_type is int. A save with an id larger than
    // INT_MAX would silently truncate under static_cast, so reject it
    // up front rather than producing a corrupted CountryId.
    using under = core::CountryId::underlying_type;
    constexpr auto kMaxId =
        static_cast<std::uint64_t>(std::numeric_limits<under>::max());
    if (id_value.value() > kMaxId) {
        return core::Result<core::CountryState>::failure(
            ctx + ": 'id' is out of range for CountryId (max " +
            std::to_string(kMaxId) + ")");
    }
    c.id = core::CountryId{static_cast<under>(id_value.value())};

    auto id_code = require_string(j, "id_code", ctx);
    if (!id_code) {
        return core::Result<core::CountryState>::failure(std::move(id_code.error()));
    }
    c.id_code = id_code.value();

    auto name = require_string(j, "name", ctx);
    if (!name) {
        return core::Result<core::CountryState>::failure(std::move(name.error()));
    }
    c.name = name.value();

    auto display = require_string(j, "display_name", ctx);
    if (!display) {
        return core::Result<core::CountryState>::failure(std::move(display.error()));
    }
    c.display_name = display.value();

    // M1.1 schema: read every numeric field. SaveSystem does NOT
    // enforce [0,1] ratio ranges - if the in-memory state held an
    // out-of-range value, that's the loader's job (DataLoader) to
    // reject at config time. Saves of valid in-memory state will
    // always be in range.
    auto load_num = [&](const char* key, double& dst)
        -> core::Result<core::CountryState> {
        auto r = require_number(j, key, ctx);
        if (!r) {
            return core::Result<core::CountryState>::failure(std::move(r.error()));
        }
        dst = r.value();
        return core::Result<core::CountryState>::success(core::CountryState{});
    };

    if (auto r = load_num("gdp",                       c.gdp);            !r) return r;
    if (auto r = load_num("tax_revenue",               c.tax_revenue);    !r) return r;
    if (auto r = load_num("budget_balance",            c.budget_balance); !r) return r;
    if (auto r = load_num("legal_tax_burden",          c.legal_tax_burden);          !r) return r;
    if (auto r = load_num("fiscal_capacity",           c.fiscal_capacity);           !r) return r;
    if (auto r = load_num("administrative_efficiency", c.administrative_efficiency); !r) return r;
    if (auto r = load_num("central_control",           c.central_control);           !r) return r;
    if (auto r = load_num("corruption",                c.corruption);                !r) return r;
    if (auto r = load_num("stability",                 c.stability);                 !r) return r;
    if (auto r = load_num("legitimacy",                c.legitimacy);                !r) return r;
    if (auto r = load_num("military_power",            c.military_power);            !r) return r;
    if (auto r = load_num("threat_perception",         c.threat_perception);         !r) return r;

    return core::Result<core::CountryState>::success(std::move(c));
}

core::Result<core::LogEntry> log_entry_from_json(const json& j,
                                                 std::size_t array_index,
                                                 std::string_view source) {
    const std::string ctx =
        std::string(source) + ": logs[" + std::to_string(array_index) + "]";

    if (!j.is_object()) {
        return core::Result<core::LogEntry>::failure(
            ctx + ": expected JSON object");
    }

    core::LogEntry e;

    auto date = require_date(j, "date", ctx);
    if (!date) {
        return core::Result<core::LogEntry>::failure(std::move(date.error()));
    }
    e.date = date.value();

    auto category = require_string(j, "category", ctx);
    if (!category) {
        return core::Result<core::LogEntry>::failure(std::move(category.error()));
    }
    e.category = category.value();

    auto sev_str = require_string(j, "severity", ctx);
    if (!sev_str) {
        return core::Result<core::LogEntry>::failure(std::move(sev_str.error()));
    }
    auto sev = severity_from_string(sev_str.value());
    if (!sev) {
        return core::Result<core::LogEntry>::failure(
            ctx + ": 'severity': " + sev.error());
    }
    e.severity = sev.value();

    auto source_field = require_string(j, "source", ctx);
    if (!source_field) {
        return core::Result<core::LogEntry>::failure(std::move(source_field.error()));
    }
    e.source = source_field.value();

    auto message = require_string(j, "message", ctx);
    if (!message) {
        return core::Result<core::LogEntry>::failure(std::move(message.error()));
    }
    e.message = message.value();

    // metadata is optional; if present, must be an object with string
    // values. Iteration order is JSON-defined (nlohmann::json preserves
    // insertion order from the parse stream).
    if (j.contains("metadata")) {
        if (!j.at("metadata").is_object()) {
            return core::Result<core::LogEntry>::failure(
                ctx + ": 'metadata': expected JSON object");
        }
        for (auto it = j.at("metadata").begin(); it != j.at("metadata").end(); ++it) {
            if (!it.value().is_string()) {
                return core::Result<core::LogEntry>::failure(
                    ctx + ": metadata['" + it.key() + "'] is not a string");
            }
            e.metadata.emplace_back(it.key(), it.value().get<std::string>());
        }
    }

    return core::Result<core::LogEntry>::success(std::move(e));
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

// ===========================================================================
// Public surface
// ===========================================================================

std::string serialize(const core::GameState& state) {
    json root = json::object();
    root["save_version"]          = kSaveFormatVersion;
    root["rng_algorithm_version"] = kRngAlgorithmVersion;
    root["current_date"]          = state.current_date.to_string();

    json rng = json::object();
    rng["seed"]    = state.rng.seed;
    rng["counter"] = state.rng.counter;
    root["rng"]    = std::move(rng);

    json countries = json::array();
    for (const auto& c : state.countries) {
        countries.push_back(country_to_json(c));
    }
    root["countries"] = std::move(countries);

    // Reserve container keys for future entity types. Their on-disk
    // shapes are not pinned yet; M1 will populate them.
    root["provinces"] = json::array();
    root["factions"]  = json::array();
    root["policies"]  = json::array();
    root["events"]    = json::array();

    json logs = json::array();
    for (const auto& e : state.logs) {
        logs.push_back(log_entry_to_json(e));
    }
    root["logs"] = std::move(logs);

    return root.dump(/*indent=*/2);
}

core::Result<bool> save(const core::GameState& state,
                        const std::filesystem::path& path) {
    std::error_code ec;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return core::Result<bool>::failure(
                path.string() + ": cannot create parent directory: " + ec.message());
        }
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) {
        return core::Result<bool>::failure(
            path.string() + ": cannot open file for writing");
    }
    const std::string text = serialize(state);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!f.good()) {
        return core::Result<bool>::failure(
            path.string() + ": write failed");
    }
    return core::Result<bool>::success(true);
}

core::Result<core::GameState> deserialize(std::string_view json_text,
                                          std::string_view source_label) {
    json root = json::parse(json_text, /*cb=*/nullptr,
                            /*allow_exceptions=*/false);
    if (root.is_discarded()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label, "JSON parse error (malformed document)"));
    }
    if (!root.is_object()) {
        return core::Result<core::GameState>::failure(
            fmt_err(source_label, "top-level JSON value is not an object"));
    }

    // ---- version gates --------------------------------------------------
    auto save_v = require_u64(root, "save_version", source_label);
    if (!save_v) {
        return core::Result<core::GameState>::failure(std::move(save_v.error()));
    }
    if (save_v.value() != kSaveFormatVersion) {
        std::string msg = "unsupported save_version " +
                          std::to_string(save_v.value()) +
                          " (this binary supports " +
                          std::to_string(kSaveFormatVersion) + ")";
        return core::Result<core::GameState>::failure(fmt_err(source_label, msg));
    }

    auto algo_v = require_u64(root, "rng_algorithm_version", source_label);
    if (!algo_v) {
        return core::Result<core::GameState>::failure(std::move(algo_v.error()));
    }
    if (algo_v.value() != kRngAlgorithmVersion) {
        std::string msg = "unsupported rng_algorithm_version " +
                          std::to_string(algo_v.value()) +
                          " (this binary supports " +
                          std::to_string(kRngAlgorithmVersion) + ")";
        return core::Result<core::GameState>::failure(fmt_err(source_label, msg));
    }

    // ---- payload --------------------------------------------------------
    core::GameState state;

    auto date = require_date(root, "current_date", source_label);
    if (!date) {
        return core::Result<core::GameState>::failure(std::move(date.error()));
    }
    state.current_date = date.value();

    auto seed = require_u64(root, "rng.seed", source_label);
    if (!seed) {
        return core::Result<core::GameState>::failure(std::move(seed.error()));
    }
    state.rng.seed = seed.value();

    auto counter = require_u64(root, "rng.counter", source_label);
    if (!counter) {
        return core::Result<core::GameState>::failure(std::move(counter.error()));
    }
    state.rng.counter = counter.value();

    // countries
    if (root.contains("countries")) {
        const auto& arr = root.at("countries");
        if (!arr.is_array()) {
            return core::Result<core::GameState>::failure(
                fmt_err(source_label, "'countries' is not an array"));
        }
        state.countries.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto c = country_from_json(arr[i], i, source_label);
            if (!c) {
                return core::Result<core::GameState>::failure(std::move(c.error()));
            }
            state.countries.push_back(std::move(c).value());
        }
    }

    // logs
    if (root.contains("logs")) {
        const auto& arr = root.at("logs");
        if (!arr.is_array()) {
            return core::Result<core::GameState>::failure(
                fmt_err(source_label, "'logs' is not an array"));
        }
        state.logs.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i) {
            auto e = log_entry_from_json(arr[i], i, source_label);
            if (!e) {
                return core::Result<core::GameState>::failure(std::move(e.error()));
            }
            state.logs.push_back(std::move(e).value());
        }
    }

    // provinces, factions, policies, events: keys reserved, contents
    // not yet schema-pinned. Tolerate present-but-empty arrays.
    return core::Result<core::GameState>::success(std::move(state));
}

core::Result<core::GameState> load(const std::filesystem::path& path) {
    auto text = read_whole_file(path);
    if (!text) {
        return core::Result<core::GameState>::failure(std::move(text.error()));
    }
    return deserialize(text.value(), path.string());
}

}  // namespace leviathan::systems::save_system
